#include "RawIron/Scene/Helpers.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ri::scene {

Mesh MakeUvSphereMesh(const std::string& name) {
    constexpr int kSegmentsAround = 12;
    constexpr int kSegmentsDown = 4;
    constexpr float kPi = 3.14159265358979323846f;

    Mesh mesh{};
    mesh.name = name;
    mesh.primitive = PrimitiveType::Sphere;
    mesh.positions.reserve(static_cast<std::size_t>((kSegmentsAround + 1) * (kSegmentsDown + 1)));
    mesh.texCoords.reserve(static_cast<std::size_t>((kSegmentsAround + 1) * (kSegmentsDown + 1)));
    mesh.indices.reserve(static_cast<std::size_t>(kSegmentsAround * kSegmentsDown * 6));

    for (int y = 0; y <= kSegmentsDown; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(kSegmentsDown);
        const float theta = v * kPi;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int x = 0; x <= kSegmentsAround; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(kSegmentsAround);
            const float phi = u * (kPi * 2.0f);
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);
            mesh.positions.push_back(ri::math::Vec3{
                sinTheta * cosPhi * 0.5f,
                cosTheta * 0.5f,
                sinTheta * sinPhi * 0.5f,
            });
            mesh.texCoords.push_back(ri::math::Vec2{u, v});
        }
    }

    const int stride = kSegmentsAround + 1;
    for (int y = 0; y < kSegmentsDown; ++y) {
        for (int x = 0; x < kSegmentsAround; ++x) {
            const int a = y * stride + x;
            const int b = a + 1;
            const int c = a + stride;
            const int d = c + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

namespace {

constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kPi = 3.14159265358979323846f;

float ClampPositive(float value, float fallback) {
    constexpr float kMinimum = 0.001f;
    if (value <= 0.0f) {
        return std::max(fallback, kMinimum);
    }
    return std::max(value, kMinimum);
}

Mesh MakeProceduralTerrainMesh(const ProceduralTerrainOptions& options, const std::string& name) {
    const int resolutionX = std::clamp(options.resolutionX, 8, 512);
    const int resolutionZ = std::clamp(options.resolutionZ, 8, 512);
    const float sizeX = ClampPositive(options.sizeX, 420.0f);
    const float sizeZ = ClampPositive(options.sizeZ, 420.0f);
    const float amp = std::max(0.0f, options.heightAmplitude);
    const float freq = std::max(0.0001f, options.heightFrequency);
    const float detailAmp = std::max(0.0f, options.detailAmplitude);
    const float detailFreq = std::max(0.0001f, options.detailFrequency);

    Mesh mesh{};
    mesh.name = name;
    mesh.primitive = PrimitiveType::Custom;

    const int vertsX = resolutionX + 1;
    const int vertsZ = resolutionZ + 1;
    mesh.positions.reserve(static_cast<std::size_t>(vertsX * vertsZ));
    mesh.texCoords.reserve(static_cast<std::size_t>(vertsX * vertsZ));
    mesh.indices.reserve(static_cast<std::size_t>(resolutionX * resolutionZ * 6));

    for (int z = 0; z <= resolutionZ; ++z) {
        const float vz = static_cast<float>(z) / static_cast<float>(resolutionZ);
        const float worldZ = (vz - 0.5f) * sizeZ;
        for (int x = 0; x <= resolutionX; ++x) {
            const float vx = static_cast<float>(x) / static_cast<float>(resolutionX);
            const float worldX = (vx - 0.5f) * sizeX;
            const float ridge = std::sin(worldX * freq) * std::cos(worldZ * freq * 0.78f);
            const float swell = std::sin((worldX + worldZ) * freq * 0.45f);
            const float detail = std::sin(worldX * detailFreq) * std::sin(worldZ * detailFreq * 1.31f);
            const float height = (ridge * amp) + (swell * amp * 0.55f) + (detail * detailAmp);
            mesh.positions.push_back(ri::math::Vec3{worldX, height, worldZ});
            mesh.texCoords.push_back(ri::math::Vec2{vx, vz});
        }
    }

    for (int z = 0; z < resolutionZ; ++z) {
        for (int x = 0; x < resolutionX; ++x) {
            const int a = (z * vertsX) + x;
            const int b = a + 1;
            const int c = ((z + 1) * vertsX) + x;
            const int d = c + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

Mesh MakePrimitiveMesh(PrimitiveType primitive, const std::string& name) {
    switch (primitive) {
        case PrimitiveType::Cube:
            return Mesh{
                .name = name,
                .primitive = primitive,
                .vertexCount = 24,
                .indexCount = 36,
                .positions = {},
                .texCoords = {},
                .indices = {},
            };
        case PrimitiveType::Plane:
            return Mesh{
                .name = name,
                .primitive = primitive,
                .vertexCount = 4,
                .indexCount = 6,
                .positions = {},
                .texCoords = {},
                .indices = {},
            };
        case PrimitiveType::Sphere:
            return MakeUvSphereMesh(name);
        case PrimitiveType::Custom:
            return Mesh{
                .name = name,
                .primitive = primitive,
                .vertexCount = 0,
                .indexCount = 0,
                .positions = {},
                .texCoords = {},
                .indices = {},
            };
    }

    return Mesh{
        .name = name,
        .primitive = primitive,
        .vertexCount = 0,
        .indexCount = 0,
        .positions = {},
        .texCoords = {},
        .indices = {},
    };
}

} // namespace

int AddPrimitiveNode(Scene& scene, const PrimitiveNodeOptions& options) {
    const int material = scene.AddMaterial(Material{
        .name = options.materialName,
        .shadingModel = options.shadingModel,
        .baseColor = options.baseColor,
        .baseColorTexture = options.baseColorTexture,
        .baseColorTextureFrames = options.baseColorTextureFrames,
        .baseColorTextureFramesPerSecond = options.baseColorTextureFramesPerSecond,
        .textureTiling = options.textureTiling,
        .emissiveColor = options.emissiveColor,
        .metallic = options.metallic,
        .roughness = options.roughness,
        .opacity = options.opacity,
        .alphaCutoff = options.alphaCutoff,
        .doubleSided = options.doubleSided,
        .transparent = options.transparent,
    });
    const int mesh = scene.AddMesh(MakePrimitiveMesh(options.primitive, options.nodeName + "Mesh"));
    const int node = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(node).localTransform = options.transform;
    scene.AttachMesh(node, mesh, material);
    return node;
}

int AddProceduralTerrainNode(Scene& scene, const ProceduralTerrainOptions& options) {
    const int material = scene.AddMaterial(Material{
        .name = options.materialName,
        .shadingModel = options.shadingModel,
        .baseColor = options.baseColor,
        .baseColorTexture = options.baseColorTexture,
        .textureTiling = options.textureTiling,
    });
    const int mesh = scene.AddMesh(MakeProceduralTerrainMesh(options, options.nodeName + "Mesh"));
    const int node = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(node).localTransform = options.transform;
    scene.AttachMesh(node, mesh, material);
    return node;
}

int AddLightNode(Scene& scene, const LightNodeOptions& options) {
    const int light = scene.AddLight(options.light);
    const int node = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(node).localTransform = options.transform;
    scene.AttachLight(node, light);
    return node;
}

int AddGridHelper(Scene& scene, const GridHelperOptions& options) {
    const float size = ClampPositive(options.size, 16.0f);
    PrimitiveNodeOptions primitive{};
    primitive.nodeName = options.nodeName;
    primitive.parent = options.parent;
    primitive.primitive = PrimitiveType::Plane;
    primitive.transform = options.transform;
    primitive.transform.scale = ri::math::Vec3{size, 1.0f, size};
    primitive.materialName = options.nodeName + "Material";
    primitive.shadingModel = ShadingModel::Unlit;
    primitive.baseColor = options.color;
    return AddPrimitiveNode(scene, primitive);
}

AxesHelperHandles AddAxesHelper(Scene& scene, const AxesHelperOptions& options) {
    AxesHelperHandles handles{};
    handles.root = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(handles.root).localTransform = options.transform;

    const float axisLength = ClampPositive(options.axisLength, 1.0f);
    const float axisThickness = ClampPositive(options.axisThickness, 0.05f);
    const float axisOffset = axisLength * 0.5f;

    PrimitiveNodeOptions xAxis{};
    xAxis.nodeName = "XAxisHelper";
    xAxis.parent = handles.root;
    xAxis.primitive = PrimitiveType::Cube;
    xAxis.materialName = "XAxisHelperMaterial";
    xAxis.shadingModel = ShadingModel::Unlit;
    xAxis.baseColor = ri::math::Vec3{1.0f, 0.2f, 0.2f};
    xAxis.transform.position = ri::math::Vec3{axisOffset, 0.0f, 0.0f};
    xAxis.transform.scale = ri::math::Vec3{axisLength, axisThickness, axisThickness};
    handles.xAxis = AddPrimitiveNode(scene, xAxis);

    PrimitiveNodeOptions yAxis{};
    yAxis.nodeName = "YAxisHelper";
    yAxis.parent = handles.root;
    yAxis.primitive = PrimitiveType::Cube;
    yAxis.materialName = "YAxisHelperMaterial";
    yAxis.shadingModel = ShadingModel::Unlit;
    yAxis.baseColor = ri::math::Vec3{0.2f, 1.0f, 0.2f};
    yAxis.transform.position = ri::math::Vec3{0.0f, axisOffset, 0.0f};
    yAxis.transform.scale = ri::math::Vec3{axisThickness, axisLength, axisThickness};
    handles.yAxis = AddPrimitiveNode(scene, yAxis);

    PrimitiveNodeOptions zAxis{};
    zAxis.nodeName = "ZAxisHelper";
    zAxis.parent = handles.root;
    zAxis.primitive = PrimitiveType::Cube;
    zAxis.materialName = "ZAxisHelperMaterial";
    zAxis.shadingModel = ShadingModel::Unlit;
    zAxis.baseColor = ri::math::Vec3{0.25f, 0.45f, 1.0f};
    zAxis.transform.position = ri::math::Vec3{0.0f, 0.0f, axisOffset};
    zAxis.transform.scale = ri::math::Vec3{axisThickness, axisThickness, axisLength};
    handles.zAxis = AddPrimitiveNode(scene, zAxis);

    return handles;
}

OrbitCameraHandles AddOrbitCamera(Scene& scene, const OrbitCameraOptions& options) {
    OrbitCameraHandles handles{};
    handles.orbit = options.orbit;
    handles.camera = scene.AddCamera(options.camera);
    handles.root = scene.CreateNode(options.rigName, options.parent);
    handles.swivel = scene.CreateNode(options.swivelName, handles.root);
    handles.cameraNode = scene.CreateNode(options.cameraNodeName, handles.swivel);
    scene.AttachCamera(handles.cameraNode, handles.camera);
    SetOrbitCameraState(scene, handles, options.orbit);
    return handles;
}

void SetOrbitCameraState(Scene& scene, OrbitCameraHandles& handles, const OrbitCameraState& orbitState) {
    OrbitCameraState sanitized = orbitState;
    sanitized.distance = ClampPositive(orbitState.distance, 6.0f);
    handles.orbit = sanitized;

    Node& root = scene.GetNode(handles.root);
    root.localTransform.position = sanitized.target;
    root.localTransform.rotationDegrees = ri::math::Vec3{0.0f, sanitized.yawDegrees, 0.0f};

    Node& swivel = scene.GetNode(handles.swivel);
    swivel.localTransform.position = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    swivel.localTransform.rotationDegrees = ri::math::Vec3{sanitized.pitchDegrees, 0.0f, 0.0f};

    Node& cameraNode = scene.GetNode(handles.cameraNode);
    cameraNode.localTransform.position = ri::math::Vec3{0.0f, 0.0f, sanitized.distance};
    cameraNode.localTransform.rotationDegrees = ri::math::Vec3{0.0f, 180.0f, 0.0f};
}

std::optional<OrbitCameraState> ComputeOrbitCameraStateFromPosition(const ri::math::Vec3& cameraPosition,
                                                                    const ri::math::Vec3& target) {
    const ri::math::Vec3 offset = cameraPosition - target;
    const float distance = ri::math::Length(offset);
    if (distance <= 0.001f) {
        return std::nullopt;
    }

    const float horizontalDistance = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    const float yawDegrees = std::atan2(offset.x, offset.z) * kRadiansToDegrees;
    const float pitchDegrees = -std::atan2(offset.y, std::max(horizontalDistance, 0.0001f)) * kRadiansToDegrees;
    return OrbitCameraState{
        .target = target,
        .distance = distance,
        .yawDegrees = yawDegrees,
        .pitchDegrees = pitchDegrees,
    };
}

std::optional<OrbitCameraState> ComputeFramedOrbitCameraState(const Scene& scene,
                                                              const Camera& camera,
                                                              const std::vector<int>& nodeHandles,
                                                              const OrbitCameraState& seed,
                                                              float padding) {
    const std::optional<WorldBounds> bounds = ComputeCombinedWorldBounds(scene, nodeHandles);
    if (!bounds.has_value()) {
        return std::nullopt;
    }

    const ri::math::Vec3 size = GetBoundsSize(*bounds);
    const float radius = std::max(ri::math::Length(size) * 0.5f, 0.25f);
    const float safePadding = std::max(padding, 1.0f);

    float distance = 3.0f;
    if (camera.projection == ProjectionType::Perspective) {
        const float fieldOfViewDegrees = std::clamp(camera.fieldOfViewDegrees, 10.0f, 160.0f);
        const float halfFovRadians = ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f);
        const float fitByFov = (radius * safePadding) / std::tan(halfFovRadians);
        const float fitByComfort = radius * safePadding * 1.6f;
        distance = std::max({fitByFov, fitByComfort, 1.0f});
    } else {
        distance = std::max({size.x, size.y, size.z}) * safePadding * 2.0f;
        distance = std::max(distance, 1.0f);
    }

    OrbitCameraState framed = seed;
    framed.target = GetBoundsCenter(*bounds);
    framed.distance = distance;
    return framed;
}

bool FrameNodesWithOrbitCamera(Scene& scene,
                               OrbitCameraHandles& handles,
                               const std::vector<int>& nodeHandles,
                               float padding) {
    const Camera& camera = scene.GetCamera(handles.camera);
    const std::optional<OrbitCameraState> framed = ComputeFramedOrbitCameraState(
        scene,
        camera,
        nodeHandles,
        handles.orbit,
        padding);
    if (!framed.has_value()) {
        return false;
    }

    SetOrbitCameraState(scene, handles, *framed);
    return true;
}

} // namespace ri::scene
