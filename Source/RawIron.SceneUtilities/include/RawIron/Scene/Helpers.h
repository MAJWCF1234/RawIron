#pragma once

#include "RawIron/Math/Vec2.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Scene/Transform.h"

#include <optional>
#include <string>
#include <vector>

namespace ri::scene {

struct PrimitiveNodeOptions {
    std::string nodeName = "Primitive";
    int parent = kInvalidHandle;
    PrimitiveType primitive = PrimitiveType::Custom;
    Transform transform{};
    std::string materialName = "material";
    ShadingModel shadingModel = ShadingModel::Lit;
    ri::math::Vec3 baseColor{1.0f, 1.0f, 1.0f};
    std::string baseColorTexture{};
    std::vector<std::string> baseColorTextureFrames{};
    float baseColorTextureFramesPerSecond = 0.0f;
    ri::math::Vec2 textureTiling{1.0f, 1.0f};
    ri::math::Vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool transparent = false;
};

struct LightNodeOptions {
    std::string nodeName = "Light";
    int parent = kInvalidHandle;
    Transform transform{};
    Light light{};
};

struct GridHelperOptions {
    std::string nodeName = "GridHelper";
    int parent = kInvalidHandle;
    Transform transform{};
    float size = 16.0f;
    ri::math::Vec3 color{0.28f, 0.30f, 0.34f};
};

struct AxesHelperOptions {
    std::string nodeName = "AxesHelper";
    int parent = kInvalidHandle;
    Transform transform{};
    float axisLength = 1.0f;
    float axisThickness = 0.05f;
};

struct OrbitCameraState {
    ri::math::Vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 6.0f;
    float yawDegrees = 180.0f;
    float pitchDegrees = -10.0f;
};

struct OrbitCameraOptions {
    std::string rigName = "OrbitRig";
    std::string swivelName = "OrbitSwivel";
    std::string cameraNodeName = "MainCamera";
    int parent = kInvalidHandle;
    Camera camera{};
    OrbitCameraState orbit{};
};

struct AxesHelperHandles {
    int root = kInvalidHandle;
    int xAxis = kInvalidHandle;
    int yAxis = kInvalidHandle;
    int zAxis = kInvalidHandle;
};

struct OrbitCameraHandles {
    int root = kInvalidHandle;
    int swivel = kInvalidHandle;
    int cameraNode = kInvalidHandle;
    int camera = kInvalidHandle;
    OrbitCameraState orbit{};
};

struct ProceduralTerrainOptions {
    std::string nodeName = "ProceduralTerrain";
    int parent = kInvalidHandle;
    Transform transform{};
    std::string materialName = "terrain-material";
    ShadingModel shadingModel = ShadingModel::Lit;
    ri::math::Vec3 baseColor{0.44f, 0.49f, 0.40f};
    std::string baseColorTexture{};
    ri::math::Vec2 textureTiling{24.0f, 24.0f};
    int resolutionX = 96;
    int resolutionZ = 96;
    float sizeX = 420.0f;
    float sizeZ = 420.0f;
    float heightAmplitude = 3.4f;
    float heightFrequency = 0.026f;
    float detailAmplitude = 0.95f;
    float detailFrequency = 0.11f;
};

int AddPrimitiveNode(Scene& scene, const PrimitiveNodeOptions& options);
int AddProceduralTerrainNode(Scene& scene, const ProceduralTerrainOptions& options);
int AddLightNode(Scene& scene, const LightNodeOptions& options);
int AddGridHelper(Scene& scene, const GridHelperOptions& options);
AxesHelperHandles AddAxesHelper(Scene& scene, const AxesHelperOptions& options);
OrbitCameraHandles AddOrbitCamera(Scene& scene, const OrbitCameraOptions& options);
void SetOrbitCameraState(Scene& scene, OrbitCameraHandles& handles, const OrbitCameraState& orbitState);
std::optional<OrbitCameraState> ComputeOrbitCameraStateFromPosition(const ri::math::Vec3& cameraPosition,
                                                                    const ri::math::Vec3& target);
std::optional<OrbitCameraState> ComputeFramedOrbitCameraState(const Scene& scene,
                                                              const Camera& camera,
                                                              const std::vector<int>& nodeHandles,
                                                              const OrbitCameraState& seed = {},
                                                              float padding = 1.35f);
bool FrameNodesWithOrbitCamera(Scene& scene,
                               OrbitCameraHandles& handles,
                               const std::vector<int>& nodeHandles,
                               float padding = 1.35f);

} // namespace ri::scene
