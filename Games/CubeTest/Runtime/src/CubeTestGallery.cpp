#include "RawIron/Games/CubeTest/CubeTestGallery.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ri::games::cubetest {
namespace {
constexpr std::array<GalleryRoomGuide, 10> rooms{{
    {"baseline", "Baseline materials", 0, "SceneUtilities primitives / native Vulkan PBR",
     "misc_exporter_gltf.html: hardwood2_diffuse.jpg and uv_grid_opengl.jpg",
     "Walk around the five samples; compare tint, roughness, metalness and opacity.",
     "The same UV grid is shared by the four PBR samples. No LRT texture pack is required."},
    {"sprites", "Camera-facing sprites", 26, "Scene mesh billboard stream / native Vulkan sprite batch",
     "css3d_sprites.html: sprite.png",
     "Move around the cloud and watch its four animated layouts.",
     "512 sprite quads should keep facing the camera through the layout transitions."},
    {"normals", "Normal conventions", 52, "Scene Material::normalScale / native Vulkan tangent-space shading",
     "misc_exporter_gltf_normals.html: NormalMapOpenGL.png, NormalMapDirectX.png; webgl_materials_normalmap.html: LeePerrySmith.glb, Map-COL.jpg, Map-SPEC.jpg, Infinite-Level_02_Tangent_SmoothUV.jpg",
     "Compare six neutral panels: OpenGL, converted DirectX, unconverted DirectX; the lower row mirrors U.",
     "The first two columns should agree; the third deliberately inverts the bump. Use --normal-comparison for an isolated view. Head fidelity remains under review."},
    {"exporter", "glTF import and export", 78, "SceneUtilities ModelLoader / GltfLoader / GltfExporter",
     "misc_exporter_gltf.html: ShaderBall.glb, coffeemat.glb, hardwood2_diffuse.jpg, uv_grid_opengl.jpg",
     "Inspect the models and hierarchy; use --export-gltf=<scene.gltf> for a portable batch export.",
     "Quantized/meshopt geometry must load. Coffee's embedded KTX2 textures are unsupported and marked magenta; this is not material parity. Exported supported images resolve locally."},
    {"interaction", "Interactive props", 104, "World InteractivePropField / CubeTestAuthority",
     "webxr_xr_cubes.html, webxr_xr_dragging.html, webxr_xr_haptics.html (behavior references; no content assets)",
     "Aim at a prop, hold E to carry, release E to throw. XR grip uses the shared prop state.",
     "Only one owner can hold a prop. Real-headset haptics have not been certified."},
    {"projectile", "Pooled projectiles", 130, "World InteractivePropField emission / authority commands",
     "webxr_xr_ballshooter.html (behavior reference; no content assets)",
     "Left-click to launch a pooled projectile at the targets.",
     "Targets react to impacts; the bounded projectile pool reuses slots instead of growing without limit."},
    {"teleport", "Validated teleport", 156, "World TeleportArc / TraceScene clearance and slope tests",
     "webxr_vr_teleport.html (behavior reference; no content assets)",
     "Aim at a landing surface and press T. Walk through the colored gates to return.",
     "Only clear, walkable landing volumes are accepted; blocked or steep destinations are rejected."},
    {"lathe", "Lathed profiles", 182, "Structural revolve / StructuralPrimitiveBundle",
     "webgl_geometries.html: LatheGeometry, existing local uv_grid_opengl.jpg",
     "Compare the 20-segment and 96-segment vessels; walk around the partial sweep.",
     "Profile arc-length UVs and smooth normals share an exact full-revolution seam. Open ends are intentional."},
    {"tubes", "Swept spline tubes", 208, "Structural spline_sweep / StructuralPrimitiveBundle",
     "webgl_geometry_extrude_splines.html: TubeGeometry and open/closed Catmull-Rom paths (behavior reference)",
     "Inspect the capped open spline and the coarse/fine closed loops from both sides.",
     "Parallel-transport frames distribute closure twist; UVs follow path length. No JavaScript runtime or per-frame mesh rebuild."},
    {"surfaces", "Parametric surfaces", 234, "Structural torus / mobius / parametric_patch",
     "webgl_geometries.html: torus and parametric Mobius surfaces, existing local uv_grid_opengl.jpg",
     "Walk around the torus, Mobius ribbon and saddle; examine the UV grid at curved seams.",
     "The torus has two periodic UV seams. The Mobius ribbon reverses its width at closure and uses a double-sided material."},
}};
}

std::span<const GalleryRoomGuide> CubeTestRoomGuides() { return rooms; }
const GalleryRoomGuide* FindCubeTestRoom(std::string_view id) {
    const auto found = std::find_if(rooms.begin(), rooms.end(), [id](const auto& room) { return room.id == id; });
    return found == rooms.end() ? nullptr : &*found;
}
ri::math::Vec3 CubeTestRoomArrival(const GalleryRoomGuide& room) {
    return {room.centerX - 6.35f, 0.20f, 0.0f};
}
const GalleryRoomGuide& CubeTestRoomAt(float worldX) {
    if (!std::isfinite(worldX)) return rooms.front();
    return *std::min_element(rooms.begin(), rooms.end(), [worldX](const auto& a, const auto& b) {
        return std::abs(a.centerX - worldX) < std::abs(b.centerX - worldX);
    });
}
std::string DescribeCubeTestRoom(const GalleryRoomGuide& room) {
    return std::string(room.title) + " [" + std::string(room.id) + "]\nSubsystem: " + std::string(room.subsystem)
        + "\nSource (Three.js 0.185.1): " + std::string(room.reference)
        + "\nControls: " + std::string(room.controls) + "\nObserve: " + std::string(room.observation)
        + "\nWASD / mouse look | Shift sprint | Space jump | Home reset to starting room | Esc exit"
          "\nColored gates link adjacent rooms in both directions. F1 opens this room guide."
          "\nAll copied assets live under Games/CubeTest/assets/reference/threejs-r185.\n";
}
std::string CubeTestGalleryHelp() {
    std::string help = "Cube Test gallery guide\nUse --start-room=<id> to choose an area.\n\n";
    for (const auto& room : rooms) help += DescribeCubeTestRoom(room) + "\n";
    return help;
}
} // namespace ri::games::cubetest
