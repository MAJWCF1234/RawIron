#include "RawIron/Logic/LogicAuthoringSenseRuntime.h"
#include "RawIron/Logic/LogicGraph.h"

#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct CapturedOutput {
    std::string sourceId;
    std::string outputName;
    double analog = 0.0;
    bool hasAnalog = false;
    std::string tagField;
};

int gFailures = 0;

void Check(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++gFailures;
        return;
    }
    std::cout << "PASS: " << message << '\n';
}

[[nodiscard]] std::vector<CapturedOutput> RunSenseTick(
    const std::vector<ri::logic::LogicAuthoringSenseProbeRecord>& probes,
    const std::array<float, 3>& probePosition,
    ri::logic::LogicAuthoringSenseRuntimeState& state,
    const ri::logic::LogicAuthoringSenseRuntimeOptions* options) {
    ri::logic::LogicGraph graph({});
    std::vector<CapturedOutput> outputs{};
    graph.SetOutputHandler([&outputs](const ri::logic::LogicOutputEvent& event) {
        CapturedOutput captured{};
        captured.sourceId = event.sourceId;
        captured.outputName = event.outputName;
        if (event.context.analogSignal.has_value()) {
            captured.analog = *event.context.analogSignal;
            captured.hasAnalog = true;
        } else if (event.context.parameter.has_value()) {
            captured.analog = *event.context.parameter;
            captured.hasAnalog = true;
        }
        if (const auto tagIt = event.context.fields.find("tag"); tagIt != event.context.fields.end()) {
            captured.tagField = tagIt->second;
        }
        outputs.push_back(std::move(captured));
    });

    std::unordered_map<std::string, std::string> kitIdByLogicNodeId{};
    for (const ri::logic::LogicAuthoringSenseProbeRecord& probe : probes) {
        kitIdByLogicNodeId[probe.logicNodeId] = probe.kitId;
    }
    ri::logic::BindLogicSenseInputDispatchHandler(graph, state, kitIdByLogicNodeId);
    ri::logic::TickLogicAuthoringSenseNodes(graph, probes, probePosition, state, options);
    return outputs;
}

[[nodiscard]] bool HasOutput(const std::vector<CapturedOutput>& outputs, const std::string_view name) {
    for (const CapturedOutput& output : outputs) {
        if (output.outputName == name) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<double> FindAnalog(const std::vector<CapturedOutput>& outputs,
                                               const std::string_view name) {
    for (const CapturedOutput& output : outputs) {
        if (output.outputName == name && output.hasAnalog) {
            return output.analog;
        }
    }
    return std::nullopt;
}

void DispatchKitInput(ri::logic::LogicAuthoringSenseRuntimeState& state,
                      const std::string& logicNodeId,
                      const std::string& kitId,
                      const std::string_view inputName,
                      const ri::logic::LogicContext& ctx) {
    ri::logic::LogicGraph graph({});
    std::unordered_map<std::string, std::string> kitIdByLogicNodeId{{logicNodeId, kitId}};
    ri::logic::BindLogicSenseInputDispatchHandler(graph, state, kitIdByLogicNodeId);
    graph.DispatchInput(logicNodeId, inputName, ctx);
}

} // namespace

int main() {
    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "tag_player",
                .kitId = "sense_tag",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        ri::logic::LogicAuthoringSenseRuntimeOptions options{};
        options.probeInstigatorTag = "player";
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {2.0f, 0.0f, 0.0f}, state, &options);
        Check(HasOutput(outputs, "seen"), "sense_tag emits seen for matching player tag");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        ri::logic::LogicContext tagCtx{};
        tagCtx.parameter = 1.0;
        tagCtx.fields["tag"] = "enemy";
        DispatchKitInput(state, "tag_filter", "sense_tag", "Tag", tagCtx);

        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "tag_filter",
                .kitId = "sense_tag",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        ri::logic::LogicAuthoringSenseRuntimeOptions options{};
        options.probeInstigatorTag = "player";
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {2.0f, 0.0f, 0.0f}, state, &options);
        Check(!HasOutput(outputs, "seen"), "sense_tag rejects probe when tag filter mismatches");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        ri::logic::LogicContext tagCtx{};
        tagCtx.parameter = 1.0;
        tagCtx.fields["tag"] = "enemy";
        DispatchKitInput(state, "tag_enemy", "sense_tag", "Tag", tagCtx);

        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "tag_enemy",
                .kitId = "sense_tag",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        ri::logic::LogicAuthoringSenseRuntimeOptions options{};
        options.probeInstigatorTag = "enemy";
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {2.0f, 0.0f, 0.0f}, state, &options);
        Check(HasOutput(outputs, "seen"), "sense_tag emits seen when configured tag matches probe");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "scalar_dist",
                .kitId = "sense_scalar",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {3.0f, 4.0f, 0.0f}, state, nullptr);
        const std::optional<double> value = FindAnalog(outputs, "val");
        Check(HasOutput(outputs, "ok") && value.has_value() && std::fabs(*value - 5.0) < 0.01,
              "sense_scalar default distance key reports probe distance");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        ri::logic::LogicContext keyCtx{};
        keyCtx.parameter = 1.0;
        keyCtx.fields["key"] = "dx";
        DispatchKitInput(state, "scalar_dx", "sense_scalar", "Key", keyCtx);

        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "scalar_dx",
                .kitId = "sense_scalar",
                .position = {1.0f, 0.0f, 0.0f},
            },
        };
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {4.0f, 0.0f, 0.0f}, state, nullptr);
        const std::optional<double> value = FindAnalog(outputs, "val");
        Check(value.has_value() && std::fabs(*value - 3.0) < 0.01, "sense_scalar dx key reports x delta");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        ri::logic::LogicContext pollCtx{};
        pollCtx.parameter = 1.0;
        DispatchKitInput(state, "scalar_poll", "sense_scalar", "Poll", pollCtx);

        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "scalar_poll",
                .kitId = "sense_scalar",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        const std::vector<CapturedOutput> first =
            RunSenseTick(probes, {2.0f, 0.0f, 0.0f}, state, nullptr);
        const std::vector<CapturedOutput> second =
            RunSenseTick(probes, {2.0f, 0.0f, 0.0f}, state, nullptr);
        Check(HasOutput(first, "ok") && !HasOutput(second, "ok"),
              "sense_scalar poll mode emits once per poll pulse");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "ray_block",
                .kitId = "sense_ray",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        ri::logic::LogicAuthoringSenseRuntimeOptions clearOptions{};
        clearOptions.raycast = [](const ri::logic::LogicSenseRaycastRequest&) -> std::optional<ri::logic::LogicSenseRaycastHit> {
            return std::nullopt;
        };
        (void)RunSenseTick(probes, {10.0f, 0.0f, 0.0f}, state, &clearOptions);

        ri::logic::LogicAuthoringSenseRuntimeOptions blockedOptions{};
        blockedOptions.raycast = [](const ri::logic::LogicSenseRaycastRequest& request)
            -> std::optional<ri::logic::LogicSenseRaycastHit> {
            return ri::logic::LogicSenseRaycastHit{request.maxDistance * 0.5f};
        };
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {10.0f, 0.0f, 0.0f}, state, &blockedOptions);
        Check(HasOutput(outputs, "miss"), "sense_ray reports miss when raycast is blocked mid-span");
    }

    {
        ri::logic::LogicAuthoringSenseRuntimeState state{};
        const std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{
            ri::logic::LogicAuthoringSenseProbeRecord{
                .logicNodeId = "ray_clear",
                .kitId = "sense_ray",
                .position = {0.0f, 0.0f, 0.0f},
            },
        };
        ri::logic::LogicAuthoringSenseRuntimeOptions options{};
        options.raycast = [](const ri::logic::LogicSenseRaycastRequest&) -> std::optional<ri::logic::LogicSenseRaycastHit> {
            return std::nullopt;
        };
        const std::vector<CapturedOutput> outputs =
            RunSenseTick(probes, {6.0f, 0.0f, 0.0f}, state, &options);
        Check(HasOutput(outputs, "hit"), "sense_ray reports hit when raycast is clear");
    }

    if (gFailures == 0) {
        std::cout << "LogicSenseSmoke: all checks passed.\n";
        return 0;
    }
    std::cerr << "LogicSenseSmoke: " << gFailures << " check(s) failed.\n";
    return 1;
}
