#include "RawIron/Scene/MaterialCalibration.h"

#include "RawIron/Scene/Helpers.h"

#include <array>
#include <stdexcept>

namespace ri::scene {

int AddNormalMappingComparisonPanels(Scene& scene, const MaterialCalibrationTextures& textures, int parent) {
    if (textures.linearNormal.empty() || textures.linearNormalDirectX.empty()) {
        throw std::invalid_argument("Normal comparison requires both normal convention texture IDs.");
    }
    const int root = scene.CreateNode("NormalMappingComparison", parent);
    constexpr std::array names{"OpenGL", "DirectXConverted", "DirectXUnconverted"};
    for (int row=0; row<2; ++row) for (int column=0; column<3; ++column) {
        const std::string name = std::string("NormalComparison_") + (row ? "MirrorU_" : "Standard_") + names[column];
        auto mesh = MakeBillboardQuadMesh(name + "Mesh");
        // Face the authored camera at -Z; the UV frame remains independently defined.
        mesh.normals.assign(mesh.positions.size(), ri::math::Vec3{0,0,-1});
        for (std::size_t i=0; i<mesh.indices.size(); i+=3) std::swap(mesh.indices[i+1],mesh.indices[i+2]);
        for (auto& uv : mesh.texCoords) {
            uv.y=1-uv.y; // Native image rows start at the top; keep reference labels upright.
            if (row) uv.x=1-uv.x;
        }
        Material material{};
        material.name=name + "Material";
        material.baseColor={.45f,.45f,.45f}; material.roughness=.65f;
        material.doubleSided=true;
        material.normalTexture=column==0 ? textures.linearNormal : textures.linearNormalDirectX;
        // The source uses a bottom-left normal basis. Flipping image V for native
        // top-first rows also requires a common tangent-Y correction; DirectX
        // then receives its additional green-channel inversion.
        material.normalScale={.5f,column==1 ? .5f : -.5f};
        const int node=scene.CreateNode(name,root);
        scene.GetNode(node).localTransform.position={3.2f*(column-1),row ? -1.6f : 1.6f,0};
        scene.GetNode(node).localTransform.scale={2.8f,2.8f,1};
        scene.AttachMesh(node,scene.AddMesh(std::move(mesh)),scene.AddMaterial(std::move(material)));
    }
    return root;
}

MaterialCalibrationScene BuildNormalMappingComparisonScene(const MaterialCalibrationTextures& textures) {
    MaterialCalibrationScene result;
    result.root=AddNormalMappingComparisonPanels(result.scene,textures);
    LightNodeOptions light{}; light.nodeName="NormalComparison_Key"; light.parent=result.root;
    light.transform.rotationDegrees={35,-25,0}; light.light.intensity=1;
    AddLightNode(result.scene,light);
    result.cameraRig=result.scene.CreateNode("NormalComparison_CameraRig");
    result.scene.GetNode(result.cameraRig).localTransform.position={0,0,-11};
    result.camera=result.scene.CreateNode("NormalComparison_Camera",result.cameraRig);
    Camera camera{}; camera.fieldOfViewDegrees=40; camera.nearClip=.05f; camera.farClip=60;
    result.scene.AttachCamera(result.camera,result.scene.AddCamera(camera));
    return result;
}

MaterialCalibrationScene BuildMaterialCalibrationScene(const MaterialCalibrationTextures& textures) {
    if (textures.srgbAlbedo.empty() || textures.linearNormal.empty() || textures.linearNormalDirectX.empty()) {
        throw std::invalid_argument("Material calibration requires explicit albedo and both normal convention texture IDs.");
    }
    MaterialCalibrationScene result;
    result.root = result.scene.CreateNode("MaterialCalibration");
    const auto box = [&](const std::string& name, ri::math::Vec3 position,
                         ri::math::Vec3 scale, ri::math::Vec3 color, bool unlit = false) {
        PrimitiveNodeOptions options{};
        options.nodeName = name;
        options.parent = result.root;
        options.primitive = PrimitiveType::Cube;
        options.transform.position = position;
        options.transform.scale = scale;
        options.materialName = name + "-material";
        options.baseColor = color;
        options.shadingModel = unlit ? ShadingModel::Unlit : ShadingModel::Lit;
        return AddPrimitiveNode(result.scene, options);
    };
    const auto material = [&](int node) -> Material& {
        return result.scene.GetMaterial(result.scene.GetNode(node).material);
    };

    // Linear RGB constants. Keep each material independent to expose binding errors.
    const std::array<ri::math::Vec3, 7> swatches{{
        {0.0f, 0.0f, 0.0f}, {0.18f, 0.18f, 0.18f}, {0.5f, 0.5f, 0.5f},
        {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
    }};
    for (std::size_t i = 0; i < swatches.size(); ++i) {
        box("Calibration_Unlit_" + std::to_string(i),
            {-4.5f + 1.5f * static_cast<float>(i), 4.7f, 2.0f},
            {1.25f, 0.8f, 0.12f}, swatches[i], true);
    }
    const int albedo = box("Calibration_sRGB", {-3.4f, 3.5f, 2.0f},
        {3.5f, 0.8f, 0.12f}, {1.0f, 1.0f, 1.0f}, true);
    material(albedo).baseColorTexture = textures.srgbAlbedo;
    box("Calibration_LinearGray", {-0.7f, 3.5f, 2.0f},
        {1.25f, 0.8f, 0.12f}, {0.2158605f, 0.2158605f, 0.2158605f}, true);
    box("Calibration_NormalControl", {1.4f, 3.5f, 2.0f},
        {1.25f, 0.8f, 0.12f}, {0.65f, 0.65f, 0.65f});
    const int normal = box("Calibration_NormalMap", {3.3f, 3.5f, 2.0f},
        {1.25f, 0.8f, 0.12f}, {0.65f, 0.65f, 0.65f});
    material(normal).normalTexture = textures.linearNormal;
    material(normal).normalScale = {0.5f, 0.5f};
    const int directX = box("Calibration_NormalMapDirectX", {5.2f, 3.5f, 2.0f},
        {1.25f, 0.8f, 0.12f}, {0.65f, 0.65f, 0.65f});
    material(directX).normalTexture = textures.linearNormalDirectX;
    material(directX).normalScale = {0.5f, -0.5f};

    for (int metal = 0; metal < 2; ++metal) {
        for (int rough = 0; rough < 3; ++rough) {
            const int node = box("Calibration_PBR_" + std::to_string(metal) + "_" + std::to_string(rough),
                {-4.4f + 1.6f * static_cast<float>(rough), 2.15f - static_cast<float>(metal), 2.0f},
                {0.9f, 0.9f, 0.9f}, {0.7f, 0.7f, 0.7f});
            material(node).metallic = static_cast<float>(metal);
            material(node).roughness = std::array{0.1f, 0.5f, 0.9f}[rough];
            result.scene.GetNode(node).localTransform.rotationDegrees = {12.0f, 25.0f, 0.0f};
        }
    }
    // Submit the front object first: the rear must still fail depth, irrespective of draw order.
    box("Calibration_DepthFront", {0.8f, 1.8f, 1.4f}, {0.95f, 0.95f, 0.12f}, {1.0f, 0.0f, 0.0f}, true);
    box("Calibration_DepthBack", {1.2f, 2.0f, 2.0f}, {1.5f, 1.5f, 0.12f}, {0.0f, 0.0f, 1.0f}, true);
    box("Calibration_BlendBack", {3.5f, 1.8f, 2.0f}, {1.7f, 1.7f, 0.12f}, {0.0f, 0.0f, 1.0f}, true);
    const int blend = box("Calibration_BlendFront", {3.2f, 1.6f, 1.4f},
        {1.2f, 1.2f, 0.12f}, {1.0f, 0.0f, 0.0f}, true);
    material(blend).transparent = true;
    material(blend).opacity = 0.5f;
    result.floor = box("Calibration_ShadowReceiver", {0.0f, -0.15f, 1.0f},
        {14.0f, 0.3f, 22.0f}, {0.5f, 0.5f, 0.5f});
    box("Calibration_ShadowCaster", {5.0f, 0.65f, 0.0f}, {0.7f, 1.3f, 0.7f}, {0.5f, 0.5f, 0.5f});

    LightNodeOptions sun{};
    sun.nodeName = "Calibration_WhiteSun";
    sun.parent = result.root;
    // Directional lights emit along +Z: positive pitch aims down toward the floor.
    sun.transform.rotationDegrees = {40.0f, -30.0f, 0.0f};
    sun.light.type = LightType::Directional;
    sun.light.color = {1.0f, 1.0f, 1.0f};
    sun.light.intensity = 1.0f;
    AddLightNode(result.scene, sun);
    result.cameraRig = result.scene.CreateNode("Calibration_CameraRig", result.root);
    result.scene.GetNode(result.cameraRig).localTransform.position = {0.0f, 3.0f, -9.0f};
    result.camera = result.scene.CreateNode("Calibration_Camera", result.cameraRig);
    Camera camera{};
    camera.fieldOfViewDegrees = 45.0f;
    camera.nearClip = 0.05f;
    camera.farClip = 60.0f;
    result.scene.AttachCamera(result.camera, result.scene.AddCamera(camera));
    return result;
}

} // namespace ri::scene
