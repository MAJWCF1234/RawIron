#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <iostream>
#include <chrono>

namespace {

bool IsValidImage(const ri::render::software::SoftwareImage& image) {
    return image.width > 0 && image.height > 0
        && image.pixels.size() == static_cast<std::size_t>(image.width * image.height * 3);
}

// Compare sampled tangent normals against an independent world-normal control.
bool TestNormalMapping() {
    namespace fs = std::filesystem;
    using namespace ri::scene;
    using namespace ri::render::software;
    const auto path = fs::temp_directory_path() / ("RawIron-normal-frame-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tga");
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove(path, ec); } } cleanup{path};
    const unsigned char tga[] = {0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,24,32,230,191,166};
    { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(tga), sizeof(tga)); }
    const auto render = [&](bool mapped, bool mirrorU, bool mirrorV, ri::math::Vec2 strength, int mode) {
        Scene scene("NormalFrameRegression");
        const int camera = scene.CreateNode("Camera");
        scene.GetNode(camera).localTransform.position = {0,0,-5};
        scene.AttachCamera(camera,scene.AddCamera({}));
        auto mesh = MakeBillboardQuadMesh("NormalFramePanel");
        for (auto& uv : mesh.texCoords) { if (mirrorU) uv.x=1-uv.x; if (mirrorV) uv.y=1-uv.y; if (mode==5) uv={0,0}; if (mode==6) uv=uv*.00001f; }
        const auto expected = ri::math::Normalize(ri::math::Vec3{
            (166.f/255*2-1)*strength.x*(mirrorU ? -1.f : 1.f),
            (191.f/255*2-1)*strength.y*(mirrorV ? -1.f : 1.f), -(230.f/255*2-1)});
        mesh.normals.assign(4, mapped || mode==5 || mode==7 || mode==8 ? ri::math::Vec3{0,0,-1} : expected);
        const int node = scene.CreateNode("Panel");
        for (auto& position : mesh.positions) position=position*4;
        Material material{}; material.baseColor={.45f,.45f,.45f}; material.roughness=1;
        material.doubleSided=true; material.normalScale=strength;
        if (mapped) material.normalTexture=mode==7 ? path.string()+"-missing" : path.string();
        scene.AttachMesh(node,scene.AddMesh(mesh),scene.AddMaterial(material));
        LightNodeOptions light{}; light.transform.rotationDegrees={35,-25,0}; light.light.intensity=.7f;
        AddLightNode(scene,light);
        ScenePreviewOptions options{}; options.width=64; options.height=64;
        options.textureRoot=path.parent_path(); options.rayTracingResolutionScale=1;
        options.rayTracingSamplesPerPixel=1; options.rayTracingShadowRays=1; options.rayTracingSunRadius=0;
        options.rayTracingAmbientOcclusion=false; options.rayTracingReflections=false;
        options.fogStrength=0; options.orderedDither=false;
        SoftwareImage image;
        RenderScenePreviewRayTraceInto(scene,camera,options,image);
        double sum=0;
        for (int y=28;y<36;++y) for (int x=28;x<36;++x) sum+=image.pixels[(y*64+x)*3];
        return sum/64;
    };
    bool passed=true;
    for (int mode=0;mode<9;++mode) {
        const ri::math::Vec2 strength=mode==8 ? ri::math::Vec2{std::numeric_limits<float>::quiet_NaN(),1} : mode==3 ? ri::math::Vec2{.5f,-.5f} : mode==4 ? ri::math::Vec2{0,0} : ri::math::Vec2{1,1};
        const auto actual=render(true,mode==1,mode==2,strength,mode);
        const auto expected=render(false,mode==1,mode==2,strength,mode);
        if (std::abs(actual-expected)>2) {
            std::cerr << "Normal mapping mode " << mode << ": sampled=" << actual << " analytic=" << expected << '\n';
            passed=false;
        }
    }
    return passed;
}

} // namespace

int main() {
    if (!TestNormalMapping()) return EXIT_FAILURE;
    {
        ri::scene::Scene scaled("scaled-normal-regression");
        const auto view=ri::scene::AddOrbitCamera(scaled,{});
        ri::scene::Mesh mesh;
        mesh.positions={{0,0,0},{1,0,1},{0,1,1}};
        const auto n=ri::math::Normalize(ri::math::Cross(mesh.positions[1],mesh.positions[2]));
        mesh.normals={n,n,n}; mesh.texCoords={{0,0},{1,0},{0,1}};
        const int object=scaled.CreateNode("scaled");
        scaled.AttachMesh(object,scaled.AddMesh(mesh),scaled.AddMaterial({}));
        scaled.GetNode(object).localTransform.scale={2,3,.5f};
        ri::render::software::ScenePreviewOptions opts;
        opts.width=32; opts.height=32;
        ri::render::software::ScenePreviewCache scaledCache;
        ri::render::software::SoftwareImage result;
        ri::render::software::RenderScenePreviewRayTraceInto(scaled,view.cameraNode,opts,result,&scaledCache);
        const auto expected=ri::math::Normalize(ri::math::Cross(ri::math::Vec3{2,0,.5f},ri::math::Vec3{0,3,.5f}));
        if (scaledCache.rayTraceScene.triN0.empty()
            || ri::math::Distance(scaledCache.rayTraceScene.triN0[0],expected)>1.e-5f) return EXIT_FAILURE;
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    ri::scene::Scene scene{"RayTraceSafety"};
    ri::scene::OrbitCameraHandles orbit = ri::scene::AddOrbitCamera(scene, {});
    ri::scene::SetOrbitCameraState(
        scene,
        orbit,
        ri::scene::OrbitCameraState{
            .target = {0.0f, 0.0f, 0.0f},
            .distance = 6.0f,
            .yawDegrees = 180.0f,
            .pitchDegrees = 0.0f,
        });

    const int cubeNode = ri::scene::AddPrimitiveNode(
        scene,
        ri::scene::PrimitiveNodeOptions{
            .nodeName = "MovingCube",
            .primitive = ri::scene::PrimitiveType::Cube,
            .metallic = 1.0f,
            .roughness = 0.0f,
        });

    const int invalidMeshNode = scene.CreateNode("InvalidMesh");
    scene.GetNode(invalidMeshNode).mesh = INT_MAX;
    scene.GetNode(invalidMeshNode).material = 0;
    const int invalidMaterialNode = scene.CreateNode("InvalidMaterial");
    scene.GetNode(invalidMaterialNode).mesh = 0;
    scene.GetNode(invalidMaterialNode).material = INT_MAX;
    const int invalidTransformNode = ri::scene::AddPrimitiveNode(scene, {});
    scene.GetNode(invalidTransformNode).localTransform.position.x = nan;
    const int invalidLightNode = scene.CreateNode("InvalidLight");
    scene.GetNode(invalidLightNode).light = INT_MAX;
    const int invalidBatch = scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "InvalidBatch",
        .parent = INT_MAX,
        .mesh = 0,
        .material = 0,
        .transforms = {ri::scene::Transform{}},
    });
    if (invalidBatch == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    ri::scene::Camera& camera = scene.GetCamera(orbit.camera);
    camera.fieldOfViewDegrees = nan;
    camera.nearClip = nan;
    camera.farClip = infinity;

    ri::render::software::ScenePreviewOptions options{};
    options.width = 128;
    options.height = 128;
    options.renderer = ri::render::software::ScenePreviewRenderer::RayTrace;
    options.rayTracingResolutionScale = nan;
    options.rayTracingMaxBounces = INT_MAX;
    options.rayTracingShadowRays = INT_MAX;
    options.rayTracingSunRadius = infinity;
    options.rayTracingAmbientOcclusionRadius = nan;
    options.rayTracingAmbientOcclusionStrength = infinity;
    options.rayTracingParallelRowsThreshold = 32;
    options.animationTimeSeconds = std::numeric_limits<double>::infinity();
    options.clearTop = {nan, infinity, -infinity};

    ri::render::software::ScenePreviewCache cache{};
    ri::render::software::SoftwareImage image{};
    ri::render::software::RenderScenePreviewRayTraceInto(scene, orbit.cameraNode, options, image, &cache);
    if (!IsValidImage(image) || cache.rayTraceScene.triV0.empty() || cache.rayTraceBvh.empty()) {
        return EXIT_FAILURE;
    }
    const std::uint64_t firstStamp = cache.rayTraceScene.geometryStamp;
    const ri::math::Vec3 firstVertex = cache.rayTraceScene.triV0.front();

    scene.GetNode(cubeNode).localTransform.position.x = 3.0f;
    ri::render::software::RenderScenePreviewRayTraceInto(scene, orbit.cameraNode, options, image, &cache);
    if (!IsValidImage(image) || cache.rayTraceScene.geometryStamp == firstStamp
        || cache.rayTraceScene.triV0.empty()) {
        return EXIT_FAILURE;
    }
    const ri::math::Vec3 movedVertex = cache.rayTraceScene.triV0.front();
    if (!std::isfinite(movedVertex.x) || std::fabs(movedVertex.x - firstVertex.x) < 2.5f) {
        return EXIT_FAILURE;
    }

    ri::render::software::SoftwareImage invalidCameraImage{};
    ri::render::software::RenderScenePreviewRayTraceInto(scene, INT_MAX, options, invalidCameraImage, &cache);
    if (!IsValidImage(invalidCameraImage)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
