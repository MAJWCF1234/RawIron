#include "RawIron/Structural/StructuralGraph.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ri::structural {
namespace {

struct StructuralTypeHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};

struct StructuralTypeEqual {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
};

using StructuralTypeSet = std::unordered_set<std::string, StructuralTypeHash, StructuralTypeEqual>;

const StructuralTypeSet kPostBuildTypes = {
    "terrain_hole_cutout",
    "spline_mesh_deformer",
    "spline_decal_ribbon",
    "spline_ribbon",
    "surface_scatter_volume",
    "scatter_surface_primitive",
    "shrinkwrap_modifier_primitive",
    "topological_uv_remapper",
    "tri_planar_node",
    "trim_sheet_sweep",
    "manifold_sweep",
    "voronoi_fracture_primitive",
    "metaball_primitive",
    "lattice_volume",
    "auto_fillet_boolean_primitive",
    "volumetric_csg_blender",
    "sdf_organic_blend_primitive",
    "sdf_intersection_node",
    "sdf_blend_node",
    "shadow_exclusion_volume",
    "l_system_branch_primitive",
    "extrude_along_normal_primitive",
    "primitive_demo_lattice",
    "primitive_demo_voronoi",
    "thick_polygon_primitive",
    "structural_profile",
    "half_pipe",
    "quarter_pipe",
    "pipe_elbow",
    "torus_slice",
    "spline_sweep",
    "revolve",
    "dome_vault",
    "loft_primitive",
};

const StructuralTypeSet kFrameTypes = {
    "surface_velocity_primitive",
    "volumetric_emitter_bounds",
    "particle_spawn_volume",
    "kinematic_translation_primitive",
    "kinematic_rotation_primitive",
    "spline_path_follower_primitive",
    "render_target_primitive",
    "render_target_surface",
    "planar_reflection_primitive",
    "planar_reflection_surface",
    "lod_switch_primitive",
    "instance_cloud_primitive",
    "l_system_branch_primitive",
    "geodesic_sphere",
    "extrude_along_normal_primitive",
    "superellipsoid",
    "primitive_demo_lattice",
    "primitive_demo_voronoi",
    "thick_polygon_primitive",
    "structural_profile",
    "half_pipe",
    "quarter_pipe",
    "pipe_elbow",
    "torus_slice",
    "spline_sweep",
    "revolve",
    "dome_vault",
    "loft_primitive",
    "voronoi_fracture_primitive",
    "metaball_primitive",
    "lattice_volume",
    "manifold_sweep",
    "trim_sheet_sweep",
    "recursive_fractal_primitive",
    "instanced_recursive_geometry_node",
    "cable_primitive",
};

const StructuralTypeSet kRuntimeTypes = {
    "portal",
    "anti_portal",
    "occlusion_portal",
    "clipping_volume",
    "filtered_collision_volume",
    "camera_blocking_volume",
    "ai_perception_blocker_volume",
    "damage_volume",
    "kill_volume",
    "camera_modifier_volume",
    "safe_zone_volume",
    "reflection_probe_volume",
    "light_importance_volume",
    "probe_grid_bounds",
    "localized_fog_volume",
    "volumetric_fog_blocker",
    "culling_distance_volume",
    "post_process_volume",
    "audio_reverb_volume",
    "audio_occlusion_volume",
    "spatial_query_volume",
    "traversal_link_volume",
    "ladder_volume",
    "climb_volume",
    "physics_volume",
    "custom_gravity_volume",
    "directional_wind_volume",
    "buoyancy_volume",
    "fluid_simulation_volume",
    "navmesh_bounds_volume",
    "navmesh_exclusion_volume",
    "radial_force_volume",
    "physics_constraint_volume",
    "convex_decomposition_generator",
    "automatic_convex_subdivision_modifier",
    "adaptive_lod_tessellator",
    "seamless_adaptive_resolution_primitive",
    "spline_mesh_deformer",
    "spline_decal_ribbon",
    "spline_ribbon",
    "topological_uv_remapper",
    "tri_planar_node",
    "surface_scatter_volume",
    "scatter_surface_primitive",
    "sky_projection_surface",
    "volumetric_emitter_bounds",
    "text_3d_primitive",
    "annotation_comment_primitive",
    "measure_tool_primitive",
    "pass_through_primitive",
    "instance_cloud_primitive",
    "l_system_branch_primitive",
    "geodesic_sphere",
    "extrude_along_normal_primitive",
    "superellipsoid",
    "primitive_demo_lattice",
    "primitive_demo_voronoi",
    "thick_polygon_primitive",
    "structural_profile",
    "half_pipe",
    "quarter_pipe",
    "pipe_elbow",
    "torus_slice",
    "spline_sweep",
    "revolve",
    "dome_vault",
    "loft_primitive",
    "voronoi_fracture_primitive",
    "metaball_primitive",
    "lattice_volume",
    "manifold_sweep",
    "trim_sheet_sweep",
    "render_target_surface",
    "planar_reflection_surface",
    "pivot_anchor_primitive",
    "symmetry_mirror_plane",
    "reference_image_plane",
    "streaming_level_volume",
    "checkpoint_spawn_volume",
    "hint_skip_brush",
    "teleport_volume",
    "launch_volume",
    "camera_confinement_volume",
    "door_window_cutout",
    "ambient_audio_volume",
    "ambient_audio_spline",
    "particle_spawn_volume",
    "lod_override_volume",
    "analytics_heatmap_volume",
    "navmesh_modifier_volume",
    "voxel_gi_bounds",
    "lightmap_density_volume",
    "local_grid_snap_volume",
    "light_portal",
};

int PhaseRank(StructuralPhase phase) {
    return static_cast<int>(phase);
}

std::string GetNodeId(const StructuralNode& node, std::size_t index) {
    if (!node.id.empty()) {
        return node.id;
    }
    return "__structural_graph_" + (node.type.empty() ? std::string("node") : node.type) + '_' + std::to_string(index);
}

void AppendDependencies(std::set<std::string>& deps, const std::vector<std::string>& values) {
    for (const std::string& value : values) {
        if (!value.empty()) {
            deps.insert(value);
        }
    }
}

} // namespace

std::string_view ToString(StructuralPhase phase) {
    switch (phase) {
    case StructuralPhase::Compile:
        return "compile";
    case StructuralPhase::Runtime:
        return "runtime";
    case StructuralPhase::PostBuild:
        return "post_build";
    case StructuralPhase::Frame:
        return "frame";
    }
    return "compile";
}

StructuralPhase ClassifyStructuralPhase(std::string_view type) {
    if (kFrameTypes.contains(type)) {
        return StructuralPhase::Frame;
    }
    if (kPostBuildTypes.contains(type)) {
        return StructuralPhase::PostBuild;
    }
    if (kRuntimeTypes.contains(type)) {
        return StructuralPhase::Runtime;
    }
    return StructuralPhase::Compile;
}

std::vector<std::string> GetExplicitDependencies(const StructuralNode& node) {
    std::set<std::string> deps;
    AppendDependencies(deps, node.targetIds);
    AppendDependencies(deps, node.childNodeList);
    AppendDependencies(deps, node.targetAIds);
    AppendDependencies(deps, node.targetBIds);
    if (!node.pivotAnchorId.empty()) {
        deps.insert(node.pivotAnchorId);
    }
    if (!node.anchorId.empty()) {
        deps.insert(node.anchorId);
    }
    return std::vector<std::string>(deps.begin(), deps.end());
}

StructuralDependencyGraph BuildStructuralDependencyGraph(const std::vector<StructuralNode>& nodes) {
    struct Entry {
        StructuralNode node;
        std::size_t index = 0;
        std::string id;
        StructuralPhase phase = StructuralPhase::Compile;
        std::vector<std::string> deps;
        std::size_t incoming = 0;
        std::set<std::string> outgoing;
    };

    std::vector<Entry> entries;
    entries.reserve(nodes.size());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        entries.push_back(Entry{
            .node = nodes[index],
            .index = index,
            .id = GetNodeId(nodes[index], index),
            .phase = ClassifyStructuralPhase(nodes[index].type),
            .deps = GetExplicitDependencies(nodes[index]),
            .incoming = 0,
            .outgoing = {},
        });
    }

    std::unordered_map<std::string, Entry*> byId;
    byId.reserve(entries.size());
    for (Entry& entry : entries) {
        byId[entry.id] = &entry;
    }

    StructuralGraphSummary summary{};

    for (Entry& entry : entries) {
        for (const std::string& depId : entry.deps) {
            const auto found = byId.find(depId);
            if (found == byId.end()) {
                summary.unresolvedDependencies.push_back(StructuralDependencyIssue{entry.id, depId});
                continue;
            }

            Entry* dependency = found->second;
            if (!dependency->outgoing.contains(entry.id)) {
                dependency->outgoing.insert(entry.id);
                entry.incoming += 1;
                summary.edgeCount += 1;
            }
            if (PhaseRank(dependency->phase) > PhaseRank(entry.phase)) {
                entry.phase = dependency->phase;
            }
        }
    }

    auto queueSorter = [](const Entry* lhs, const Entry* rhs) {
        const int phaseDelta = PhaseRank(lhs->phase) - PhaseRank(rhs->phase);
        return phaseDelta != 0 ? phaseDelta < 0 : lhs->index < rhs->index;
    };

    std::vector<Entry*> queue;
    for (Entry& entry : entries) {
        if (entry.incoming == 0) {
            queue.push_back(&entry);
        }
    }
    std::sort(queue.begin(), queue.end(), queueSorter);

    std::vector<Entry*> ordered;
    ordered.reserve(entries.size());

    while (!queue.empty()) {
        Entry* entry = queue.front();
        queue.erase(queue.begin());
        ordered.push_back(entry);

        for (const std::string& targetId : entry->outgoing) {
            const auto found = byId.find(targetId);
            if (found == byId.end()) {
                continue;
            }
            Entry* target = found->second;
            if (target->incoming > 0) {
                target->incoming -= 1;
            }
            if (target->incoming == 0
                && std::find(queue.begin(), queue.end(), target) == queue.end()
                && std::find(ordered.begin(), ordered.end(), target) == ordered.end()) {
                queue.push_back(target);
                std::sort(queue.begin(), queue.end(), queueSorter);
            }
        }
    }

    std::vector<Entry*> cyclicEntries;
    for (Entry& entry : entries) {
        if (std::find(ordered.begin(), ordered.end(), &entry) == ordered.end()) {
            cyclicEntries.push_back(&entry);
        }
    }
    if (!cyclicEntries.empty()) {
        std::sort(cyclicEntries.begin(), cyclicEntries.end(), [](const Entry* lhs, const Entry* rhs) {
            return lhs->index < rhs->index;
        });
        ordered.insert(ordered.end(), cyclicEntries.begin(), cyclicEntries.end());
        summary.cycleCount = 1;
    }

    StructuralDependencyGraph graph{};
    graph.orderedNodes.reserve(ordered.size());
    for (Entry* entry : ordered) {
        entry->node.id = entry->id;
        entry->node.phase = entry->phase;
        graph.orderedNodes.push_back(entry->node);
        switch (entry->phase) {
        case StructuralPhase::Compile:
            graph.summary.phaseBuckets.compile += 1;
            break;
        case StructuralPhase::Runtime:
            graph.summary.phaseBuckets.runtime += 1;
            break;
        case StructuralPhase::PostBuild:
            graph.summary.phaseBuckets.postBuild += 1;
            break;
        case StructuralPhase::Frame:
            graph.summary.phaseBuckets.frame += 1;
            break;
        }
    }

    summary.nodeCount = graph.orderedNodes.size();
    summary.unresolvedDependencyCount = summary.unresolvedDependencies.size();
    summary.phaseBuckets = graph.summary.phaseBuckets;
    graph.summary = summary;
    return graph;
}

} // namespace ri::structural
