// Headless smoke test for the CPU side of the Vulkan renderer:
// command intent recording, intent staging validation, frame submission replay,
// and the pipeline state cache. No GPU or Vulkan loader required.

#include "RawIron/Render/VulkanCommandBufferRecorder.h"
#include "RawIron/Render/VulkanFrameSubmission.h"
#include "RawIron/Render/VulkanIntentStaging.h"
#include "RawIron/Render/VulkanPipelineStateCache.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <vector>

namespace {

#define RI_REQUIRE(condition)                                                              \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", __LINE__, #condition); \
            return false;                                                                  \
        }                                                                                  \
    } while (false)

using ri::render::vulkan::BuildVulkanIntentStagingPlan;
using ri::render::vulkan::ExecuteVulkanFrameSubmission;
using ri::render::vulkan::ExecuteVulkanFrameSubmissionWithPipelineCache;
using ri::render::vulkan::VulkanCommandBufferRecorder;
using ri::render::vulkan::VulkanCommandIntent;
using ri::render::vulkan::VulkanCommandIntentType;
using ri::render::vulkan::VulkanFrameSubmissionStats;
using ri::render::vulkan::VulkanIntentStagingPlan;
using ri::render::vulkan::VulkanIntentStagingStatus;
using ri::render::vulkan::VulkanPipelineStateCache;
using ri::render::vulkan::VulkanPipelineStateKey;
using ri::render::vulkan::VulkanPipelineStateRecord;
using ri::render::vulkan::VulkanSubmissionPassFilter;

constexpr std::array<float, 16> kIdentity{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

bool TestRecorderBatchDiscipline() {
    VulkanCommandBufferRecorder recorder{};
    const std::array<float, 4> clear{0.1f, 0.2f, 0.3f, 1.0f};

    // Commands outside a batch must be rejected and record nothing.
    RI_REQUIRE(!recorder.ClearColor(clear.data()));
    RI_REQUIRE(!recorder.SetViewProjection(kIdentity.data()));
    RI_REQUIRE(!recorder.DrawMesh(1, 2, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(recorder.Intents().empty());
    RI_REQUIRE(!recorder.InBatch());

    RI_REQUIRE(recorder.BeginBatch(1, 7));
    RI_REQUIRE(recorder.InBatch());
    // Nested batches are rejected.
    RI_REQUIRE(!recorder.BeginBatch(1, 7));
    // Mismatched end is rejected and the batch stays open.
    RI_REQUIRE(!recorder.EndBatch(1, 8));
    RI_REQUIRE(!recorder.EndBatch(2, 7));
    RI_REQUIRE(recorder.InBatch());

    // Null payloads are rejected.
    RI_REQUIRE(!recorder.ClearColor(nullptr));
    RI_REQUIRE(!recorder.SetViewProjection(nullptr));
    RI_REQUIRE(!recorder.DrawMesh(1, 2, 0, 3, 1, nullptr));

    RI_REQUIRE(recorder.ClearColor(clear.data()));
    RI_REQUIRE(recorder.SetViewProjection(kIdentity.data()));
    RI_REQUIRE(recorder.DrawMesh(11, 22, 6, 36, 2, kIdentity.data()));
    RI_REQUIRE(recorder.EndBatch(1, 7));
    RI_REQUIRE(!recorder.InBatch());

    const std::vector<VulkanCommandIntent>& intents = recorder.Intents();
    RI_REQUIRE(intents.size() == 5);
    RI_REQUIRE(intents[0].type == VulkanCommandIntentType::BeginBatch);
    RI_REQUIRE(intents[1].type == VulkanCommandIntentType::ClearColor);
    RI_REQUIRE(intents[1].clearColor[2] == 0.3f);
    RI_REQUIRE(intents[2].type == VulkanCommandIntentType::SetViewProjection);
    RI_REQUIRE(intents[3].type == VulkanCommandIntentType::DrawMesh);
    RI_REQUIRE(intents[3].meshHandle == 11);
    RI_REQUIRE(intents[3].materialHandle == 22);
    RI_REQUIRE(intents[3].firstIndex == 6);
    RI_REQUIRE(intents[3].indexCount == 36);
    RI_REQUIRE(intents[3].instanceCount == 2);
    RI_REQUIRE(intents[3].passIndex == 1);
    RI_REQUIRE(intents[3].pipelineBucket == 7);
    RI_REQUIRE(intents[4].type == VulkanCommandIntentType::EndBatch);

    recorder.Reset();
    RI_REQUIRE(recorder.Intents().empty());
    RI_REQUIRE(!recorder.InBatch());
    return true;
}

bool TestStagingPlanHappyPath() {
    VulkanCommandBufferRecorder recorder{};
    const std::array<float, 4> clear{0.0f, 0.0f, 0.0f, 1.0f};

    RI_REQUIRE(recorder.BeginBatch(0, 1));
    RI_REQUIRE(recorder.ClearColor(clear.data()));
    RI_REQUIRE(recorder.SetViewProjection(kIdentity.data()));
    RI_REQUIRE(recorder.DrawMesh(1, 5, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(recorder.DrawMesh(2, 5, 0, 6, 1, kIdentity.data()));
    RI_REQUIRE(recorder.EndBatch(0, 1));
    RI_REQUIRE(recorder.BeginBatch(2, 3));
    RI_REQUIRE(recorder.DrawMesh(3, 9, 0, 12, 4, kIdentity.data()));
    RI_REQUIRE(recorder.EndBatch(2, 3));

    const VulkanIntentStagingPlan plan = BuildVulkanIntentStagingPlan(recorder.Intents());
    RI_REQUIRE(plan.status == VulkanIntentStagingStatus::Ok);
    RI_REQUIRE(plan.totalIntents == recorder.Intents().size());
    RI_REQUIRE(plan.stagedIntents == 5);
    RI_REQUIRE(plan.ranges.size() == 2);
    RI_REQUIRE(plan.ranges[0].passIndex == 0);
    RI_REQUIRE(plan.ranges[0].pipelineBucket == 1);
    RI_REQUIRE(plan.ranges[0].firstIntentIndex == 1);
    RI_REQUIRE(plan.ranges[0].intentCount == 4);
    RI_REQUIRE(plan.ranges[0].clearCount == 1);
    RI_REQUIRE(plan.ranges[0].setViewProjectionCount == 1);
    RI_REQUIRE(plan.ranges[0].drawCount == 2);
    RI_REQUIRE(plan.ranges[1].passIndex == 2);
    RI_REQUIRE(plan.ranges[1].pipelineBucket == 3);
    RI_REQUIRE(plan.ranges[1].intentCount == 1);
    RI_REQUIRE(plan.ranges[1].drawCount == 1);

    // Empty streams are a valid, empty plan.
    const VulkanIntentStagingPlan emptyPlan = BuildVulkanIntentStagingPlan({});
    RI_REQUIRE(emptyPlan.status == VulkanIntentStagingStatus::Ok);
    RI_REQUIRE(emptyPlan.ranges.empty());
    return true;
}

bool TestStagingPlanErrors() {
    // BeginBatch inside an open batch.
    {
        std::vector<VulkanCommandIntent> intents(2);
        intents[0].type = VulkanCommandIntentType::BeginBatch;
        intents[1].type = VulkanCommandIntentType::BeginBatch;
        RI_REQUIRE(BuildVulkanIntentStagingPlan(intents).status
                   == VulkanIntentStagingStatus::UnexpectedBeginBatch);
    }
    // Command before any batch is opened.
    {
        std::vector<VulkanCommandIntent> intents(1);
        intents[0].type = VulkanCommandIntentType::DrawMesh;
        RI_REQUIRE(BuildVulkanIntentStagingPlan(intents).status
                   == VulkanIntentStagingStatus::CommandOutsideBatch);
    }
    // EndBatch with a different pass/pipeline than the open batch.
    {
        std::vector<VulkanCommandIntent> intents(2);
        intents[0].type = VulkanCommandIntentType::BeginBatch;
        intents[0].passIndex = 1;
        intents[1].type = VulkanCommandIntentType::EndBatch;
        intents[1].passIndex = 2;
        RI_REQUIRE(BuildVulkanIntentStagingPlan(intents).status
                   == VulkanIntentStagingStatus::MismatchedEndBatch);
    }
    // Batch left open at the end of the stream.
    {
        std::vector<VulkanCommandIntent> intents(1);
        intents[0].type = VulkanCommandIntentType::BeginBatch;
        RI_REQUIRE(BuildVulkanIntentStagingPlan(intents).status
                   == VulkanIntentStagingStatus::UnterminatedBatch);
    }
    return true;
}

bool TestPipelineStateCache() {
    VulkanPipelineStateCache cache{};
    const VulkanPipelineStateKey key{.passIndex = 1, .pipelineBucket = 2, .materialBucket = 3};

    RI_REQUIRE(!cache.Lookup(key).has_value());

    std::size_t resolverCalls = 0;
    const VulkanPipelineStateCache::ResolveFn resolver =
        [&resolverCalls](const VulkanPipelineStateKey& requested)
        -> std::optional<VulkanPipelineStateRecord> {
        ++resolverCalls;
        return VulkanPipelineStateRecord{
            .pipelineHandle = 100ULL + requested.materialBucket,
            .layoutHandle = 200ULL,
        };
    };

    const std::optional<VulkanPipelineStateRecord> first = cache.Resolve(key, resolver);
    RI_REQUIRE(first.has_value());
    RI_REQUIRE(first->pipelineHandle == 103ULL);
    RI_REQUIRE(resolverCalls == 1);

    // Second resolve is a cache hit; the resolver must not run again.
    const std::optional<VulkanPipelineStateRecord> second = cache.Resolve(key, resolver);
    RI_REQUIRE(second.has_value());
    RI_REQUIRE(second->pipelineHandle == 103ULL);
    RI_REQUIRE(resolverCalls == 1);

    RI_REQUIRE(cache.Lookup(key).has_value());
    RI_REQUIRE(cache.Stats().lookups == 2);
    RI_REQUIRE(cache.Stats().hits == 1);
    RI_REQUIRE(cache.Stats().misses == 1);
    RI_REQUIRE(cache.Stats().stored == 1);

    // Failed resolutions are not cached, so a later resolve can still succeed.
    const VulkanPipelineStateKey missingKey{.passIndex = 9, .pipelineBucket = 9, .materialBucket = 9};
    const VulkanPipelineStateCache::ResolveFn failingResolver =
        [](const VulkanPipelineStateKey&) -> std::optional<VulkanPipelineStateRecord> {
        return std::nullopt;
    };
    RI_REQUIRE(!cache.Resolve(missingKey, failingResolver).has_value());
    RI_REQUIRE(!cache.Lookup(missingKey).has_value());
    RI_REQUIRE(cache.Resolve(missingKey, resolver).has_value());

    cache.Clear();
    RI_REQUIRE(!cache.Lookup(key).has_value());
    RI_REQUIRE(cache.Stats().stored == 0);
    return true;
}

bool TestFrameSubmissionRoundTrip() {
    VulkanCommandBufferRecorder source{};
    const std::array<float, 4> clear{0.5f, 0.25f, 0.125f, 1.0f};
    RI_REQUIRE(source.BeginBatch(0, 4));
    RI_REQUIRE(source.ClearColor(clear.data()));
    RI_REQUIRE(source.DrawMesh(7, 8, 0, 9, 1, kIdentity.data()));
    RI_REQUIRE(source.EndBatch(0, 4));
    RI_REQUIRE(source.BeginBatch(3, 6));
    RI_REQUIRE(source.DrawMesh(1, 2, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(source.EndBatch(3, 6));

    const VulkanIntentStagingPlan plan = BuildVulkanIntentStagingPlan(source.Intents());
    RI_REQUIRE(plan.status == VulkanIntentStagingStatus::Ok);

    // Replaying the plan into a second recorder must reproduce the intent stream.
    VulkanCommandBufferRecorder replay{};
    VulkanFrameSubmissionStats stats{};
    RI_REQUIRE(ExecuteVulkanFrameSubmission(source.Intents(), plan, replay, {}, &stats));
    RI_REQUIRE(stats.rangesVisited == 2);
    RI_REQUIRE(stats.rangesSubmitted == 2);
    RI_REQUIRE(stats.commandsSubmitted == 3);
    RI_REQUIRE(replay.Intents().size() == source.Intents().size());
    for (std::size_t index = 0; index < replay.Intents().size(); ++index) {
        RI_REQUIRE(replay.Intents()[index].type == source.Intents()[index].type);
        RI_REQUIRE(replay.Intents()[index].passIndex == source.Intents()[index].passIndex);
        RI_REQUIRE(replay.Intents()[index].pipelineBucket == source.Intents()[index].pipelineBucket);
        RI_REQUIRE(replay.Intents()[index].meshHandle == source.Intents()[index].meshHandle);
    }

    // Pass filters skip ranges outside the window but still count the visit.
    VulkanCommandBufferRecorder filtered{};
    const VulkanSubmissionPassFilter filter{.minPassIndex = 3, .maxPassIndex = 3};
    RI_REQUIRE(ExecuteVulkanFrameSubmission(source.Intents(), plan, filtered, filter, &stats));
    RI_REQUIRE(stats.rangesVisited == 2);
    RI_REQUIRE(stats.rangesSubmitted == 1);
    RI_REQUIRE(stats.commandsSubmitted == 1);
    RI_REQUIRE(filtered.Intents().size() == 3);

    // Invalid plans are rejected before any replay happens.
    VulkanIntentStagingPlan brokenPlan = plan;
    brokenPlan.status = VulkanIntentStagingStatus::UnterminatedBatch;
    VulkanCommandBufferRecorder untouched{};
    RI_REQUIRE(!ExecuteVulkanFrameSubmission(source.Intents(), brokenPlan, untouched, {}, &stats));
    RI_REQUIRE(untouched.Intents().empty());

    // Ranges that point past the end of the intent stream are rejected.
    VulkanIntentStagingPlan truncatedPlan = plan;
    truncatedPlan.ranges[1].intentCount += 100;
    RI_REQUIRE(!ExecuteVulkanFrameSubmission(source.Intents(), truncatedPlan, untouched, {}, &stats));
    RI_REQUIRE(untouched.Intents().empty());

    // Overflowing range arithmetic and stale plans must fail before the recorder is mutated.
    VulkanIntentStagingPlan overflowPlan = plan;
    overflowPlan.ranges[0].firstIntentIndex = std::numeric_limits<std::size_t>::max() - 1U;
    overflowPlan.ranges[0].intentCount = 8U;
    RI_REQUIRE(!ExecuteVulkanFrameSubmission(source.Intents(), overflowPlan, untouched, {}, &stats));
    RI_REQUIRE(untouched.Intents().empty());

    VulkanIntentStagingPlan mismatchedPlan = plan;
    mismatchedPlan.ranges[1].passIndex = 99U;
    RI_REQUIRE(!ExecuteVulkanFrameSubmission(source.Intents(), mismatchedPlan, untouched, {}, &stats));
    RI_REQUIRE(untouched.Intents().empty());
    return true;
}

bool TestFrameSubmissionWithPipelineCache() {
    VulkanCommandBufferRecorder source{};
    RI_REQUIRE(source.BeginBatch(0, 1));
    RI_REQUIRE(source.DrawMesh(1, 40, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(source.DrawMesh(2, 40, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(source.DrawMesh(3, 41, 0, 3, 1, kIdentity.data()));
    RI_REQUIRE(source.EndBatch(0, 1));

    const VulkanIntentStagingPlan plan = BuildVulkanIntentStagingPlan(source.Intents());
    RI_REQUIRE(plan.status == VulkanIntentStagingStatus::Ok);

    VulkanPipelineStateCache cache{};
    std::size_t resolverCalls = 0;
    const VulkanPipelineStateCache::ResolveFn resolver =
        [&resolverCalls](const VulkanPipelineStateKey&) -> std::optional<VulkanPipelineStateRecord> {
        ++resolverCalls;
        return VulkanPipelineStateRecord{.pipelineHandle = 1, .layoutHandle = 1};
    };

    VulkanCommandBufferRecorder replay{};
    VulkanFrameSubmissionStats stats{};
    RI_REQUIRE(ExecuteVulkanFrameSubmissionWithPipelineCache(
        source.Intents(), plan, replay, cache, resolver, {}, &stats));
    RI_REQUIRE(stats.pipelineResolves == 3);
    RI_REQUIRE(stats.pipelineResolveFailures == 0);
    // Two draws share material bucket 40, so only two unique pipelines resolve.
    RI_REQUIRE(resolverCalls == 2);
    RI_REQUIRE(cache.Stats().stored == 2);

    // A resolver failure aborts the submission and reports the failure.
    VulkanPipelineStateCache emptyCache{};
    const VulkanPipelineStateCache::ResolveFn failingResolver =
        [](const VulkanPipelineStateKey&) -> std::optional<VulkanPipelineStateRecord> {
        return std::nullopt;
    };
    VulkanCommandBufferRecorder aborted{};
    RI_REQUIRE(!ExecuteVulkanFrameSubmissionWithPipelineCache(
        source.Intents(), plan, aborted, emptyCache, failingResolver, {}, &stats));
    RI_REQUIRE(stats.pipelineResolveFailures == 1);
    return true;
}

} // namespace

int main() {
    if (!TestRecorderBatchDiscipline()) {
        return EXIT_FAILURE;
    }
    if (!TestStagingPlanHappyPath()) {
        return EXIT_FAILURE;
    }
    if (!TestStagingPlanErrors()) {
        return EXIT_FAILURE;
    }
    if (!TestPipelineStateCache()) {
        return EXIT_FAILURE;
    }
    if (!TestFrameSubmissionRoundTrip()) {
        return EXIT_FAILURE;
    }
    if (!TestFrameSubmissionWithPipelineCache()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
