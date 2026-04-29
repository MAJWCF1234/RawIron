#pragma once

#include "RawIron/Logic/LogicAuthoring.h"
#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/Logic/WorldActorPorts.h"
#include "RawIron/World/RuntimeState.h"

#include <string_view>

namespace ri::world {

/// Route logic `DispatchInput` for non-node ids into the runtime environment (triggers, registered doors/spawners, …).
inline void BindWorldActorsToLogicGraph(ri::logic::LogicGraph& graph, RuntimeEnvironmentService& env) {
    graph.SetWorldActorInputHandler([&graph, &env](std::string_view actorId, std::string_view inputName, const ri::logic::LogicContext& ctx) {
        [[maybe_unused]] const bool applied = env.ApplyWorldActorLogicInput(graph, actorId, inputName, ctx);
    });
}

/// Same as \ref BindWorldActorsToLogicGraph (kept for existing call sites).
inline void BindGenericTriggerVolumesToLogicGraph(ri::logic::LogicGraph& graph, RuntimeEnvironmentService& env) {
    BindWorldActorsToLogicGraph(graph, env);
}

/// Generic trigger volume types that participate in \ref ri::logic::LogicGraph routing (`sourceId` = volume id).
[[nodiscard]] inline bool TriggerTypeParticipatesInLogicGraph(std::string_view volumeType) {
    return volumeType == "trigger_volume" || volumeType == "generic_trigger_volume";
}

[[nodiscard]] inline ri::logic::LogicContext MakePlayerTriggerContext(std::string_view instigatorId) {
    ri::logic::LogicContext ctx;
    ctx.instigatorId = std::string(instigatorId);
    ctx.fields[std::string{ri::logic::ports::kFieldInstigatorKind}] = "player";
    return ctx;
}

/// Fan-out trigger transitions as logic graph outputs (`OnStartTouch` / `OnStay` / `OnEndTouch`).
inline void ApplyTriggerTransitionsToLogicGraph(ri::logic::LogicGraph& graph,
                                                const TriggerUpdateResult& result,
                                                const ri::logic::LogicContext& baseContext) {
    for (const TriggerTransition& t : result.transitions) {
        if (!TriggerTypeParticipatesInLogicGraph(t.volumeType)) {
            continue;
        }
        ri::logic::LogicContext ctx = baseContext;
        ctx.sourceId = t.volumeId;
        switch (t.kind) {
        case TriggerTransitionKind::Enter:
            graph.EmitWorldOutput(t.volumeId, ri::logic::ports::kTriggerOnStartTouch, ctx);
            break;
        case TriggerTransitionKind::Stay:
            graph.EmitWorldOutput(t.volumeId, ri::logic::ports::kTriggerOnStay, ctx);
            break;
        case TriggerTransitionKind::Exit:
            graph.EmitWorldOutput(t.volumeId, ri::logic::ports::kTriggerOnEndTouch, ctx);
            break;
        }
    }
}

/// Build strict authoring compile options from currently known world actor ids.
[[nodiscard]] inline ri::logic::LogicAuthoringCompileOptions BuildLogicAuthoringCompileOptionsFromEnvironment(
    const RuntimeEnvironmentService& env,
    const bool strictUnknownWorldActors = true) {
    ri::logic::LogicAuthoringCompileOptions options{};
    options.allowUnknownWorldActorIds = !strictUnknownWorldActors;

    auto addIds = [&options](const std::vector<std::string>& ids) {
        for (const std::string& id : ids) {
            if (!id.empty()) {
                options.knownWorldActorIds.insert(id);
            }
        }
    };
    auto addVolumeIds = [&options](const auto& volumes, std::string_view actorKind) {
        for (const auto& volume : volumes) {
            if (!volume.id.empty()) {
                options.knownWorldActorIds.insert(volume.id);
                options.knownWorldActorKinds[volume.id] = std::string(actorKind);
            }
        }
    };

    addVolumeIds(env.GetGenericTriggerVolumes(), "trigger_volume");
    addVolumeIds(env.GetSpatialQueryVolumes(), "trigger_volume");
    addVolumeIds(env.GetStreamingLevelVolumes(), "trigger_volume");
    addVolumeIds(env.GetCheckpointSpawnVolumes(), "trigger_volume");
    addVolumeIds(env.GetTeleportVolumes(), "trigger_volume");
    addVolumeIds(env.GetLaunchVolumes(), "trigger_volume");
    addIds(env.GetRegisteredLogicDoorIds());
    for (const std::string& id : env.GetRegisteredLogicDoorIds()) {
        if (!id.empty()) {
            options.knownWorldActorKinds[id] = "door";
        }
    }
    addIds(env.GetRegisteredLogicSpawnerIds());
    for (const std::string& id : env.GetRegisteredLogicSpawnerIds()) {
        if (!id.empty()) {
            options.knownWorldActorKinds[id] = "spawner";
        }
    }
    return options;
}

/// Compile a 3D-authored logic graph against live runtime world actor ids.
[[nodiscard]] inline ri::logic::LogicGraphSpec CompileLogicAuthoringGraphForEnvironment(
    const ri::logic::LogicAuthoringGraph& authoring,
    const RuntimeEnvironmentService& env,
    const bool strictUnknownWorldActors = true) {
    const ri::logic::LogicAuthoringCompileOptions options =
        BuildLogicAuthoringCompileOptionsFromEnvironment(env, strictUnknownWorldActors);
    return ri::logic::CompileLogicAuthoringGraph(authoring, options);
}

/// Compile + return diagnostics (preferred for editor validation UI).
[[nodiscard]] inline ri::logic::LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReportForEnvironment(
    const ri::logic::LogicAuthoringGraph& authoring,
    const RuntimeEnvironmentService& env,
    const bool strictUnknownWorldActors = true) {
    const ri::logic::LogicAuthoringCompileOptions options =
        BuildLogicAuthoringCompileOptionsFromEnvironment(env, strictUnknownWorldActors);
    return ri::logic::CompileLogicAuthoringGraphWithReport(authoring, options);
}

} // namespace ri::world
