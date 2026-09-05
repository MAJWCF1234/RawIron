#include "RawIron/XR/OpenXrRuntime.h"
#include "RawIron/XR/HardwareSceneBuilder.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

#include <cstdlib>
#include <string>
#include <limits>
#include <iostream>

bool TestHaptics() {
    using namespace ri::xr;
    HapticPolicy gate;
    if (gate.Request(0,HapticEvent::None,true,true,0,.2f,.02f).amplitude != 0
        || gate.Request(0,HapticEvent::Grab,false,true,0,.2f,.02f).amplitude != 0
        || gate.Request(0,HapticEvent::Grab,true,false,0,.2f,.02f).amplitude != 0
        || gate.Request(2,HapticEvent::Grab,true,true,0,.2f,.02f).amplitude != 0
        || gate.Request(0,HapticEvent::Grab,true,true,0,std::numeric_limits<float>::infinity(),.02f).amplitude != 0)
        return false;
    auto pulse = gate.Request(0,HapticEvent::Selection,true,true,0,1,1);
    if (pulse.amplitude != .35f || pulse.durationSeconds != .05f
        || gate.Request(0,HapticEvent::Grab,true,true,.099,.2f,.02f).amplitude != 0
        || gate.Request(1,HapticEvent::Grab,true,true,0,.2f,.02f).amplitude != .2f
        || gate.Request(0,HapticEvent::Contact,true,true,.101,.2f,.02f).amplitude != .2f
        || gate.Request(0,HapticEvent::Grab,true,true,-1,.2f,.02f).amplitude != 0) return false;
    for (int i=0;i<1000;++i)
        if (gate.Request(0,HapticEvent::None,true,true,i,.2f,.02f).amplitude != 0) return false;
    return true;
}

bool TestSceneBuilder() {
    ri::scene::Scene scene("engine XR builder");
    ri::scene::Mesh mesh;
    mesh.positions = {{0,0,0},{1,0,0},{0,1,0}};
    mesh.normals = {{0,0,1},{0,0,1},{0,0,1}};
    mesh.texCoords = {{0,0},{1,0},{0,1}};
    mesh.indices = {0,1,2};
    const int geometry = scene.AddMesh(mesh);
    ri::scene::Material material;
    material.metallic = .3f;
    material.roughness = .7f;
    material.normalScale = {1,-1};
    const int surface = scene.AddMaterial(material);
    const int near = scene.CreateNode("near");
    scene.AttachMesh(near,geometry,surface);
    const int far = scene.CreateNode("far room after portal");
    scene.GetNode(far).localTransform.position.x = 160;
    scene.AttachMesh(far,geometry,surface);
    auto atlas = ri::xr::BuildHardwareTextureAtlas(scene);
    const auto all = ri::xr::BuildHardwareScene(scene,atlas);
    if (all.size()!=6 || all[3].position[0]!=160 || all[0].materialParams[3]!=-1
        || all[0].materialParams[0]!=.3f || all[0].materialParams[1]!=.7f) return false;
    const auto staticOnly = ri::xr::BuildHardwareScene(scene,atlas,{near});
    if (staticOnly.size()!=3 || staticOnly[0].position[0]!=160) return false;
    scene.GetMaterial(surface).baseColorTexture = "__missing_xr_texture_for_test__.png";
    atlas = ri::xr::BuildHardwareTextureAtlas(scene);
    return atlas.errors.size()==1 && atlas.loadedTextures==0;
}

bool TestScaledNormals() {
    ri::scene::Mesh mesh;
    mesh.positions={{0,0,0},{1,0,1},{0,1,1}};
    const auto n=ri::math::Normalize(ri::math::Cross(mesh.positions[1],mesh.positions[2]));
    mesh.normals={n,n,n}; mesh.texCoords={{0,0},{1,0},{0,1}};
    const auto transform=ri::math::TRS({3,1,4},{23,17,31},{2,3,.5f});
    std::vector<ri::xr::HardwareSceneVertex> vertices;
    ri::xr::AppendHardwareMesh(vertices,mesh,{},transform,{});
    if (vertices.size()!=3) return false;
    const auto expected=ri::math::Normalize(ri::math::Cross(
        ri::math::TransformVector(transform,mesh.positions[1]),ri::math::TransformVector(transform,mesh.positions[2])));
    for (const auto& v : vertices)
        if (ri::math::Distance({v.normal[0],v.normal[1],v.normal[2]},expected)>1.e-5f) return false;
    return true;
}

bool TestStructuralSurfaceUpload() {
    for (const auto* preset : {"revolve_open","spline_sweep","spline_loop","torus","mobius","parametric_patch"}) {
        ri::scene::Scene scene("XR structural surfaces");
        ri::scene::StructuralPrimitiveBundleParams params;
        params.presetField=preset;
        params.transform.position={234,1,0}; // Later rooms must not be dropped by distance.
        const auto result=ri::scene::SpawnStructuralPrimitiveBundle(scene,params);
        if (result.node==ri::scene::kInvalidHandle) return false;
        const auto& mesh=scene.GetMesh(result.mesh);
        const auto atlas=ri::xr::BuildHardwareTextureAtlas(scene);
        const auto vertices=ri::xr::BuildHardwareScene(scene,atlas);
        if (!atlas.errors.empty() || vertices.size()!=mesh.positions.size()) return false;
        for (std::size_t i=0;i<vertices.size();++i) {
            if (std::abs(vertices[i].position[0]-(mesh.positions[i].x+234))>1.e-4f
                || std::abs(vertices[i].normal[0]-mesh.normals[i].x)>1.e-4f
                || vertices[i].texCoord[0]!=mesh.texCoords[i].x || vertices[i].texCoord[1]!=mesh.texCoords[i].y) return false;
        }
    }
    return true;
}

int main() {
    if (!TestScaledNormals()) { std::cerr << "XR nonuniform-scale normal regression\n"; return EXIT_FAILURE; }
    if (!TestStructuralSurfaceUpload()) { std::cerr << "XR structural surface upload failed\n"; return EXIT_FAILURE; }
    if (!TestHaptics()) { std::cerr << "Haptic policy failed\n"; return EXIT_FAILURE; }
    if (!TestSceneBuilder()) { std::cerr << "XR scene builder failed\n"; return EXIT_FAILURE; }
    ri::xr::OpenXrRuntime runtime;
    std::string diagnostic;
    const bool ready = runtime.Initialize("Raw Iron OpenXR Smoke", diagnostic);
    std::cout << "OpenXR ready=" << ready << " system=" << runtime.Info().systemName << " diagnostic=" << diagnostic << '\n';
    if (ready) {
        if (!runtime.IsSystemReady() || runtime.Info().stereoViews.size() != 2U
            || !runtime.Info().vulkanEnable2) {
            return EXIT_FAILURE;
        }
        runtime.PollEvents();
    } else if (diagnostic.empty()) {
        // A missing runtime/HMD is valid for CI, but it must always be diagnosed.
        return EXIT_FAILURE;
    }
    runtime.Shutdown();
    return EXIT_SUCCESS;
}
