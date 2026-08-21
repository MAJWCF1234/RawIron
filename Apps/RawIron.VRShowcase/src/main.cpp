#include "RawIron/Core/Log.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Games/CubeTest/CubeTestAuthority.h"
#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/Trace/TeleportTargeting.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/World/PortalTravel.h"
#include "RawIron/XR/OpenXrRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

constexpr std::array<ri::math::Vec3, 8> kCubeVertices{{
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}}};
constexpr std::array<std::array<int, 4>, 6> kCubeFaces{{
    {4, 5, 6, 7}, {1, 0, 3, 2}, {0, 4, 7, 3},
    {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}}};
constexpr std::array<ri::math::Vec3, 4> kPlaneVertices{{
    {-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, 0.5f}}};

struct HardwareTextureAtlas {
    static constexpr int kSize = 2048;
    static constexpr int kGrid = 8;
    static constexpr int kCellSize = kSize / kGrid;
    std::vector<std::uint8_t> rgba = std::vector<std::uint8_t>(
        static_cast<std::size_t>(kSize * kSize * 4), 255U);
    std::unordered_map<std::string, std::array<float, 4>> rects{};
    std::size_t loadedTextures = 0;
};

// Desktop and PCVR consume the same Material paths. The OpenXR host packs those paths into one
// immutable atlas at session startup; normal maps use a distinct key because they need linear
// decode rather than albedo's sRGB decode.
[[nodiscard]] std::string NormalAtlasKey(const std::string& texturePath) {
    return "normal:" + texturePath;
}

ri::spatial::Aabb BuildVrPlayerBounds(const ri::math::Vec3& feet) {
    return {
        .min = {feet.x - 0.25f, feet.y, feet.z - 0.25f},
        .max = {feet.x + 0.25f, feet.y + 1.8f, feet.z + 0.25f}};
}

ri::math::Vec3 VrFeetFromBounds(const ri::spatial::Aabb& bounds) {
    return {
        (bounds.min.x + bounds.max.x) * 0.5f,
        bounds.min.y,
        (bounds.min.z + bounds.max.z) * 0.5f};
}

struct VrLocomotionState {
    const ri::games::cubetest::CubeTestWorld* world = nullptr;
    ri::trace::TraceScene traceScene{};
    ri::trace::MovementControllerState movement{};
    ri::trace::MovementControllerOptions options{};
    ri::world::PortalTravelerState portalTraveler{};
};

void ResolveVrLocomotion(void* user,
                         const ri::xr::HardwareLocomotionInput& input,
                         float origin[3],
                         float& yawDegrees) {
    auto& state = *static_cast<VrLocomotionState*>(user);
    const ri::trace::MovementInput movementInput{
        .moveForward = input.moveForward,
        .moveRight = input.moveRight,
        .viewForwardWorld = {input.viewForward[0], input.viewForward[1], input.viewForward[2]},
        .viewRightWorld = {input.viewRight[0], input.viewRight[1], input.viewRight[2]},
        .jumpPressed = input.jumpPressed};
    state.movement = ri::trace::SimulateMovementControllerStep(
                         state.traceScene,
                         state.movement,
                         movementInput,
                         input.deltaSeconds,
                         state.options)
                         .state;
    const ri::world::PortalTravelResult portal = ri::world::UpdatePortalTraveler(
        state.world->portals,
        state.movement.body.bounds,
        input.deltaSeconds,
        state.portalTraveler);
    if (portal.traveled) {
        state.movement.body.bounds = BuildVrPlayerBounds(portal.destinationFeet);
        if (!portal.preserveVelocity) state.movement.body.velocity = {};
        state.movement.onGround = true;
        yawDegrees = portal.destinationYawDegrees;
    }
    const ri::math::Vec3 feet = VrFeetFromBounds(state.movement.body.bounds);
    origin[0] = feet.x;
    origin[1] = feet.y;
    origin[2] = feet.z;
}

HardwareTextureAtlas BuildHardwareTextureAtlas(const ri::scene::Scene& scene) {
    HardwareTextureAtlas atlas{};
    int cellIndex = 0;
    const auto copyTexture = [&](const std::string& texturePath, const bool normalMap) {
        const std::string atlasKey = normalMap ? NormalAtlasKey(texturePath) : texturePath;
        if (texturePath.empty() || atlas.rects.contains(atlasKey)
            || cellIndex >= HardwareTextureAtlas::kGrid * HardwareTextureAtlas::kGrid) return;
        const ri::render::software::RgbaImage image =
            ri::render::software::LoadRgbaImageFile(std::filesystem::path(texturePath));
        if (!image.Valid()) return;
        const int cellX = (cellIndex % HardwareTextureAtlas::kGrid) * HardwareTextureAtlas::kCellSize;
        const int cellY = (cellIndex / HardwareTextureAtlas::kGrid) * HardwareTextureAtlas::kCellSize;
        for (int y = 0; y < HardwareTextureAtlas::kCellSize; ++y) {
            const int sourceY = (std::min)(
                y * image.height / HardwareTextureAtlas::kCellSize, image.height - 1);
            for (int x = 0; x < HardwareTextureAtlas::kCellSize; ++x) {
                const int sourceX = (std::min)(
                    x * image.width / HardwareTextureAtlas::kCellSize, image.width - 1);
                const std::size_t sourceOffset = static_cast<std::size_t>((sourceY * image.width + sourceX) * 4);
                const std::size_t destinationOffset = static_cast<std::size_t>(
                    ((cellY + y) * HardwareTextureAtlas::kSize + cellX + x) * 4);
                if (!normalMap) {
                    std::copy_n(image.rgba.data() + sourceOffset, 4, atlas.rgba.data() + destinationOffset);
                } else {
                    // The atlas image is sRGB. Encode the normally-linear tangent vector first,
                    // so sampling returns its original 0..1 vector components in the shader.
                    for (int channel = 0; channel < 3; ++channel) {
                        const float linear = static_cast<float>(image.rgba[sourceOffset + channel]) / 255.0f;
                        const float srgb = linear <= 0.0031308f
                            ? linear * 12.92f
                            : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                        atlas.rgba[destinationOffset + channel] = static_cast<std::uint8_t>(
                            std::clamp(std::lround(srgb * 255.0f), 0L, 255L));
                    }
                    atlas.rgba[destinationOffset + 3U] = image.rgba[sourceOffset + 3U];
                }
            }
        }
        const float inverseSize = 1.0f / static_cast<float>(HardwareTextureAtlas::kSize);
        atlas.rects.emplace(atlasKey, std::array<float, 4>{
            (static_cast<float>(cellX) + 0.5f) * inverseSize,
            (static_cast<float>(cellY) + 0.5f) * inverseSize,
            (static_cast<float>(cellX + HardwareTextureAtlas::kCellSize) - 0.5f) * inverseSize,
            (static_cast<float>(cellY + HardwareTextureAtlas::kCellSize) - 0.5f) * inverseSize});
        ++cellIndex;
        ++atlas.loadedTextures;
    };
    for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
        const ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
        copyTexture(material.baseColorTexture, false);
        copyTexture(material.normalTexture, true);
    }
    return atlas;
}

void AppendHardwareTriangle(std::vector<ri::xr::HardwareSceneVertex>& output,
                            const ri::math::Mat4& world,
                            const ri::math::Vec3& a,
                            const ri::math::Vec3& b,
                            const ri::math::Vec3& c,
                            const ri::math::Vec3& baseColor,
                            const std::array<ri::math::Vec2, 3>& texCoords,
                            const std::array<float, 4>& atlasRect,
                            const std::array<float, 4>& normalAtlasRect,
                            const ri::scene::Material& material,
                            const std::array<ri::math::Vec3, 3>& localNormals = {}) {
    const ri::math::Vec3 positions[]{
        ri::math::TransformPoint(world, a),
        ri::math::TransformPoint(world, b),
        ri::math::TransformPoint(world, c)};
    ri::math::Vec3 normal = ri::math::Cross(positions[1] - positions[0], positions[2] - positions[0]);
    if (ri::math::LengthSquared(normal) > 1.0e-10f) normal = ri::math::Normalize(normal);
    for (std::size_t index = 0; index < 3U; ++index) {
        const ri::math::Vec3& position = positions[index];
        ri::math::Vec3 vertexNormal = localNormals[index];
        if (ri::math::LengthSquared(vertexNormal) > 1.0e-10f) {
            vertexNormal = ri::math::TransformVector(world, vertexNormal);
            vertexNormal = ri::math::LengthSquared(vertexNormal) > 1.0e-10f
                ? ri::math::Normalize(vertexNormal)
                : normal;
        } else {
            vertexNormal = normal;
        }
        output.push_back({
            {position.x, position.y, position.z},
            {vertexNormal.x, vertexNormal.y, vertexNormal.z},
            {baseColor.x, baseColor.y, baseColor.z},
            {texCoords[index].x, texCoords[index].y},
            {atlasRect[0], atlasRect[1], atlasRect[2], atlasRect[3]},
            {normalAtlasRect[0], normalAtlasRect[1], normalAtlasRect[2], normalAtlasRect[3]},
            {material.metallic, material.roughness, material.normalScale.x, material.normalScale.y}});
    }
}

void AppendHardwareMesh(std::vector<ri::xr::HardwareSceneVertex>& output,
                        const ri::scene::Mesh& mesh,
                        const ri::scene::Material& material,
                        const ri::math::Mat4& world,
                        const HardwareTextureAtlas& atlas) {
    const ri::math::Vec3 color{
        std::clamp(material.baseColor.x + material.emissiveColor.x, 0.0f, 1.0f),
        std::clamp(material.baseColor.y + material.emissiveColor.y, 0.0f, 1.0f),
        std::clamp(material.baseColor.z + material.emissiveColor.z, 0.0f, 1.0f)};
    const auto rectIt = atlas.rects.find(material.baseColorTexture);
    const std::array<float, 4> atlasRect = rectIt != atlas.rects.end()
        ? rectIt->second
        : std::array<float, 4>{};
    const auto normalRectIt = atlas.rects.find(NormalAtlasKey(material.normalTexture));
    const std::array<float, 4> normalAtlasRect = normalRectIt != atlas.rects.end()
        ? normalRectIt->second
        : std::array<float, 4>{};
    if (!mesh.positions.empty()
        && mesh.geometryMode != ri::scene::MeshGeometryMode::CameraFacingSpriteQuads) {
        const bool indexed = mesh.indices.size() >= 3U;
        const std::size_t triangleCount = indexed ? mesh.indices.size() / 3U : mesh.positions.size() / 3U;
        for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
            const int ia = indexed ? mesh.indices[triangle * 3U] : static_cast<int>(triangle * 3U);
            const int ib = indexed ? mesh.indices[triangle * 3U + 1U] : static_cast<int>(triangle * 3U + 1U);
            const int ic = indexed ? mesh.indices[triangle * 3U + 2U] : static_cast<int>(triangle * 3U + 2U);
            if (ia < 0 || ib < 0 || ic < 0
                || static_cast<std::size_t>(ia) >= mesh.positions.size()
                || static_cast<std::size_t>(ib) >= mesh.positions.size()
                || static_cast<std::size_t>(ic) >= mesh.positions.size()) continue;
            const auto uv = [&](const int index) {
                if (mesh.texCoords.size() == mesh.positions.size()) {
                    const ri::math::Vec2 authored = mesh.texCoords[static_cast<std::size_t>(index)];
                    return ri::math::Vec2{
                        authored.x * material.textureTiling.x,
                        authored.y * material.textureTiling.y};
                }
                return ri::math::Vec2{};
            };
            AppendHardwareTriangle(
                output,
                world,
                mesh.positions[ia],
                mesh.positions[ib],
                mesh.positions[ic],
                color,
                {uv(ia), uv(ib), uv(ic)},
                atlasRect,
                normalAtlasRect,
                material,
                {mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ia)] : ri::math::Vec3{},
                 mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ib)] : ri::math::Vec3{},
                 mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ic)] : ri::math::Vec3{}});
        }
        return;
    }
    if (mesh.primitive == ri::scene::PrimitiveType::Cube) {
        for (const auto& face : kCubeFaces) {
            AppendHardwareTriangle(
                output, world, kCubeVertices[face[0]], kCubeVertices[face[1]], kCubeVertices[face[2]], color,
                {{{0.0f, 0.0f}, {material.textureTiling.x, 0.0f}, material.textureTiling}}, atlasRect, normalAtlasRect, material);
            AppendHardwareTriangle(
                output, world, kCubeVertices[face[0]], kCubeVertices[face[2]], kCubeVertices[face[3]], color,
                {{{0.0f, 0.0f}, material.textureTiling, {0.0f, material.textureTiling.y}}}, atlasRect, normalAtlasRect, material);
        }
    } else if (mesh.primitive == ri::scene::PrimitiveType::Plane) {
        AppendHardwareTriangle(
            output, world, kPlaneVertices[0], kPlaneVertices[1], kPlaneVertices[2], color,
            {{{0.0f, 0.0f}, {material.textureTiling.x, 0.0f}, material.textureTiling}}, atlasRect, normalAtlasRect, material);
        AppendHardwareTriangle(
            output, world, kPlaneVertices[0], kPlaneVertices[2], kPlaneVertices[3], color,
            {{{0.0f, 0.0f}, material.textureTiling, {0.0f, material.textureTiling.y}}}, atlasRect, normalAtlasRect, material);
    }
}

std::vector<ri::xr::HardwareSceneVertex> BuildHardwareScene(
    const ri::scene::Scene& scene,
    const HardwareTextureAtlas& atlas,
    const std::vector<int>& excludedNodes,
    const ri::math::Vec3& streamCenter,
    const float streamRadius) {
    std::vector<ri::xr::HardwareSceneVertex> vertices{};
    vertices.reserve(scene.GetRenderableNodeHandles().size() * 36U);
    const float streamRadiusSquared = streamRadius * streamRadius;
    const auto isInStreamRadius = [&](const ri::math::Mat4& matrix) {
        const ri::math::Vec3 position{matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
        return ri::math::LengthSquared(position - streamCenter) <= streamRadiusSquared;
    };
    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        if (std::ranges::find(excludedNodes, nodeHandle) != excludedNodes.end()) continue;
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        if (node.mesh < 0 || node.material < 0) continue;
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        if (!isInStreamRadius(world)) continue;
        AppendHardwareMesh(
            vertices,
            scene.GetMesh(node.mesh),
            scene.GetMaterial(node.material),
            world,
            atlas);
    }
    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const ri::scene::MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (batch.mesh < 0 || batch.material < 0) continue;
        const ri::math::Mat4 parentWorld = batch.parent >= 0
            ? scene.ComputeWorldMatrix(batch.parent)
            : ri::math::IdentityMatrix();
        for (const ri::scene::Transform& transform : batch.transforms) {
            const ri::math::Mat4 world = ri::math::Multiply(parentWorld, transform.LocalMatrix());
            if (!isInStreamRadius(world)) continue;
            AppendHardwareMesh(
                vertices,
                scene.GetMesh(batch.mesh),
                scene.GetMaterial(batch.material),
                world,
                atlas);
        }
    }
    return vertices;
}

struct VrInteractionState {
    ri::games::cubetest::CubeTestWorld* world = nullptr;
    const HardwareTextureAtlas* atlas = nullptr;
    std::vector<ri::xr::HardwareSceneVertex> vertices{};
    std::array<int, 2> grabbedProp{{-1, -1}};
    std::array<int, 2> hoveredProp{{-1, -1}};
    std::array<float, 2> grabDistance{{1.0f, 1.0f}};
    std::array<ri::math::Vec3, 2> previousTarget{};
    std::array<bool, 2> hasPreviousTarget{};
    VrLocomotionState* locomotion = nullptr;
    std::array<ri::trace::TeleportTargetingResult, 2> teleportTargets{};
    ri::runtime::RuntimeCore* authorityRuntime = nullptr;
    ri::runtime::AuthoritativeNetModule* netcode = nullptr;
    int authorityFrameIndex = 0;
    double authorityElapsedSeconds = 0.0;
};

[[nodiscard]] bool IsRemoteAuthorityClient(const VrInteractionState& state) {
    return state.netcode != nullptr && state.netcode->Config().role == ri::runtime::NetRole::Client;
}

void AppendTeleportArc(std::vector<ri::xr::HardwareSceneVertex>& output,
                       const std::vector<ri::math::Vec3>& points,
                       const ri::math::Vec3& color) {
    constexpr float halfWidth = 0.018f;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const ri::math::Vec3 delta = points[index] - points[index - 1U];
        ri::math::Vec3 side = ri::math::Cross(delta, {0.0f, 1.0f, 0.0f});
        if (ri::math::LengthSquared(side) <= 1.0e-8f) side = {1.0f, 0.0f, 0.0f};
        side = ri::math::Normalize(side) * halfWidth;
        const ri::math::Vec3 corners[]{
            points[index - 1U] - side,
            points[index - 1U] + side,
            points[index] + side,
            points[index] - side};
        constexpr int triangles[]{0, 1, 2, 0, 2, 3};
        for (const int corner : triangles) {
            output.push_back({
                {corners[corner].x, corners[corner].y, corners[corner].z},
                {color.x, color.y, color.z}});
        }
    }
}

ri::xr::HardwareInteractionFrameOutput UpdateVrInteraction(
    void* user,
    const ri::xr::HardwareInteractionFrameInput& input) {
    auto& state = *static_cast<VrInteractionState*>(user);
    ri::xr::HardwareInteractionFrameOutput output{};
    if (state.world == nullptr || state.atlas == nullptr) return output;

    const float deltaSeconds = std::clamp(input.deltaSeconds, 0.0f, 0.05f);
    if (state.authorityRuntime != nullptr) {
        state.authorityElapsedSeconds += static_cast<double>(deltaSeconds);
        const ri::core::FrameContext frame{
            .frameIndex = state.authorityFrameIndex++,
            .deltaSeconds = static_cast<double>(deltaSeconds),
            .elapsedSeconds = state.authorityElapsedSeconds,
            .realtimeSeconds = state.authorityElapsedSeconds,
            .realDeltaSeconds = static_cast<double>(deltaSeconds),
        };
        if (!state.authorityRuntime->Frame(frame)) {
            ri::core::LogInfo("VR authority runtime frame failed: "
                              + std::string(state.authorityRuntime->Context().FailureReason()));
            state.authorityRuntime = nullptr;
        }
    }
    for (std::size_t handIndex = 0; handIndex < std::size(input.hands); ++handIndex) {
        const ri::xr::HardwareInteractionHandInput& hand = input.hands[handIndex];
        const ri::math::Vec3 rayOrigin{
            hand.aimOrigin[0], hand.aimOrigin[1], hand.aimOrigin[2]};
        const ri::math::Vec3 rayDirection{
            hand.aimDirection[0], hand.aimDirection[1], hand.aimDirection[2]};
        const std::uint32_t owner = static_cast<std::uint32_t>(handIndex + 1U);
        const ri::world::InteractivePropSelection hover = hand.tracked
            ? ri::world::SelectInteractiveProp(state.world->interactionProps, rayOrigin, rayDirection, 4.5f)
            : ri::world::InteractivePropSelection{};
        if (hover.propIndex != state.hoveredProp[handIndex]) {
            state.hoveredProp[handIndex] = hover.propIndex;
            // Selection feedback is emitted only on entering a reachable prop, never from
            // physics contacts, button presses, teleportation, or every rendered frame.
            if (hover.propIndex >= 0) {
                output.hapticAmplitude[handIndex] = 0.12f;
                output.hapticDurationSeconds[handIndex] = 0.012f;
            }
        }
        if (hand.tracked && hand.teleportHeld && rayOrigin.x >= 148.0f
            && state.locomotion != nullptr) {
            state.teleportTargets[handIndex] = ri::trace::ResolveTeleportTarget(
                state.locomotion->traceScene, rayOrigin, rayDirection);
        }
        if (hand.teleportReleased && state.teleportTargets[handIndex].validLanding
            && state.locomotion != nullptr) {
            const ri::math::Vec3 destination =
                state.teleportTargets[handIndex].destinationFeet;
            state.locomotion->movement.body.bounds = BuildVrPlayerBounds(destination);
            state.locomotion->movement.body.velocity = {};
            state.locomotion->movement.onGround = true;
            output.teleportRequested = true;
            output.teleportDestinationFeet[0] = destination.x;
            output.teleportDestinationFeet[1] = destination.y;
            output.teleportDestinationFeet[2] = destination.z;
            state.teleportTargets[handIndex] = {};
        } else if (!hand.teleportHeld && !hand.teleportReleased) {
            state.teleportTargets[handIndex] = {};
        }
        if (hand.tracked && hand.selectPressed) {
            bool fired = false;
            if (IsRemoteAuthorityClient(state)) {
                ri::runtime::NetPacket packet{};
                packet.channel = 0U;
                packet.reliable = true;
                packet.payload = ri::games::cubetest::CubeTestAuthorityBridge::BuildProjectileCommand(
                    rayOrigin + rayDirection * 1.4f, rayDirection);
                fired = state.netcode->SendPacket(0U, packet, ri::runtime::NetChannelKind::Authority);
            } else {
                fired = ri::games::cubetest::EmitCubeTestProjectile(
                    *state.world,
                    rayOrigin + rayDirection * 1.4f,
                    rayDirection).propIndex >= 0;
            }
            static_cast<void>(fired);
        }
        if (!IsRemoteAuthorityClient(state) && hand.tracked && hand.grabPressed && state.grabbedProp[handIndex] < 0) {
            const ri::world::InteractivePropSelection selection = hover;
            if (selection.propIndex >= 0 && ri::world::BeginInteractivePropGrab(
                    state.world->interactionProps, selection.propIndex, owner)) {
                state.grabbedProp[handIndex] = selection.propIndex;
                state.grabDistance[handIndex] = std::clamp(selection.distance, 0.25f, 4.5f);
                state.hasPreviousTarget[handIndex] = false;
            }
        }

        const int grabbed = state.grabbedProp[handIndex];
        if (grabbed < 0) continue;
        ri::math::Vec3 releaseVelocity{};
        if (hand.tracked) {
            const ri::math::Vec3 target = rayOrigin + rayDirection * state.grabDistance[handIndex];
            if (state.hasPreviousTarget[handIndex] && deltaSeconds > 1.0e-4f) {
                releaseVelocity = (target - state.previousTarget[handIndex]) / deltaSeconds;
                const float speed = ri::math::Length(releaseVelocity);
                if (speed > 12.0f) releaseVelocity = releaseVelocity * (12.0f / speed);
            }
            (void)ri::world::MoveInteractivePropGrab(
                state.world->interactionProps, grabbed, owner, target);
            state.previousTarget[handIndex] = target;
            state.hasPreviousTarget[handIndex] = true;
        }
        if (hand.grabReleased || !hand.grabHeld || !hand.tracked) {
            (void)ri::world::EndInteractivePropGrab(
                state.world->interactionProps, grabbed, owner, releaseVelocity);
            state.grabbedProp[handIndex] = -1;
            state.hasPreviousTarget[handIndex] = false;
        }
    }

    static_cast<void>(ri::world::StepInteractivePropField(
        state.world->interactionProps, deltaSeconds, state.world->interactionField));
    static_cast<void>(ri::world::StepInteractivePropField(
        state.world->projectileProps, deltaSeconds, state.world->projectileField));

    state.vertices.clear();
    const auto appendField = [&](const std::vector<ri::world::InteractivePropState>& props,
                                 const std::vector<int>& nodes) {
        for (std::size_t index = 0; index < props.size(); ++index) {
            const ri::world::InteractivePropState& prop = props[index];
            if (!prop.active) continue;
            ri::scene::Node& node = state.world->scene.GetNode(nodes[index]);
            node.localTransform.position = prop.position;
            node.localTransform.scale = prop.halfExtents * 2.0f;
            node.localTransform.rotationDegrees = node.localTransform.rotationDegrees
                + prop.angularVelocityDegrees * deltaSeconds;
            AppendHardwareMesh(
                state.vertices,
                state.world->scene.GetMesh(node.mesh),
                state.world->scene.GetMaterial(node.material),
                state.world->scene.ComputeWorldMatrix(nodes[index]),
                *state.atlas);
        }
    };
    appendField(state.world->interactionProps, state.world->interactionPropNodes);
    appendField(state.world->projectileProps, state.world->projectilePropNodes);
    for (std::size_t handIndex = 0; handIndex < std::size(input.hands); ++handIndex) {
        if (!input.hands[handIndex].teleportHeld) continue;
        const ri::trace::TeleportTargetingResult& teleport = state.teleportTargets[handIndex];
        AppendTeleportArc(
            state.vertices,
            teleport.arcPoints,
            teleport.validLanding
                ? ri::math::Vec3{0.18f, 1.0f, 0.48f}
                : ri::math::Vec3{1.0f, 0.16f, 0.12f});
    }
    output.vertices = state.vertices.data();
    output.vertexCount = state.vertices.size();
    return output;
}

bool ConfigureProcessRuntime(const ri::core::CommandLine& commandLine, std::string& error) {
    std::filesystem::path runtimeManifest{};
    if (const auto explicitRuntime = commandLine.GetValue("--runtime-json"); explicitRuntime.has_value()) {
        runtimeManifest = *explicitRuntime;
    }
#if defined(_WIN32)
    if (runtimeManifest.empty() && commandLine.HasFlag("--steamvr")) {
        wchar_t value[32768]{};
        DWORD valueBytes = sizeof(value);
        const LSTATUS status = RegGetValueW(
            HKEY_CURRENT_USER,
            L"SOFTWARE\\Khronos\\OpenXR\\1",
            L"ActiveRuntime",
            RRF_RT_REG_SZ,
            nullptr,
            value,
            &valueBytes);
        if (status == ERROR_SUCCESS) {
            runtimeManifest = std::filesystem::path(value);
        }
    }
#endif
    if (runtimeManifest.empty()) {
        if (commandLine.HasFlag("--steamvr")) {
            error = "--steamvr could not find a per-user SteamVR OpenXR runtime manifest.";
            return false;
        }
        return true;
    }
    if (!std::filesystem::is_regular_file(runtimeManifest)) {
        error = "OpenXR runtime manifest does not exist: " + runtimeManifest.string();
        return false;
    }
#if defined(_WIN32)
    if (!SetEnvironmentVariableW(L"XR_RUNTIME_JSON", runtimeManifest.c_str())) {
        error = "Could not apply the process-local OpenXR runtime manifest.";
        return false;
    }
#else
    error = "--runtime-json process override is not implemented on this platform.";
    return false;
#endif
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.VRShowcase options:");
        ri::core::LogInfo("  --steamvr              Use the per-user SteamVR OpenXR manifest for this process only");
        ri::core::LogInfo("  --runtime-json=<path>  Use an explicit OpenXR runtime manifest for this process only");
        ri::core::LogInfo("  --frames=<n>           Submit a bounded headset test (default: 300)");
        ri::core::LogInfo("  --start-room=<name>    baseline, sprites, normals, exporter, interaction, projectile, or teleport");
        ri::core::LogInfo("  --net-mode=<mode>      offline (default), listen, dedicated, or client");
        ri::core::LogInfo("  --port=<n> --connect-host=<host> --connect-port=<n>  Authority session endpoint");
        ri::core::LogInfo("  --probe-only           Discover runtime/system/actions without starting a session");
        return 0;
    }
    std::string error;
    if (!ConfigureProcessRuntime(commandLine, error)) {
        ri::core::LogInfo(error);
        return 1;
    }
    // Build through the same game/world API as desktop. XR is a host and input/view mode,
    // never a forked copy of the showcase content or structural primitive graph.
    ri::games::cubetest::CubeTestWorld world =
        ri::games::cubetest::BuildCubeTestWorld("Raw Iron VR Capability Showcase");
    auto authorityBridge = std::make_shared<ri::games::cubetest::CubeTestAuthorityBridge>(&world);
    ri::runtime::RuntimeCore authorityRuntime(
        {.id = "rawiron.vrshowcase", .displayName = "Raw Iron VR Showcase", .mode = "xr"},
        ri::runtime::DetectRuntimePaths(std::filesystem::current_path()));
    ri::runtime::AuthoritativeNetConfig authorityConfig =
        ri::games::cubetest::BuildCubeTestAuthorityConfig(commandLine, authorityBridge);
    auto authorityModule = std::make_unique<ri::runtime::AuthoritativeNetModule>(authorityConfig);
    ri::runtime::AuthoritativeNetModule* const authorityNetcode = authorityModule.get();
    authorityRuntime.AddModule(std::move(authorityModule));
    if (!authorityRuntime.Startup(commandLine)) {
        ri::core::LogInfo("VR authority runtime startup failed: "
                          + std::string(authorityRuntime.Context().FailureReason()));
        return 1;
    }
    ri::math::Vec3 vrSpawnFeet{0.0f, 0.02f, -7.4f};
    if (const auto startRoom = commandLine.GetValue("--start-room"); startRoom.has_value()) {
        if (*startRoom == "sprites") vrSpawnFeet = {19.65f, 0.20f, 0.0f};
        else if (*startRoom == "normals") vrSpawnFeet = {45.65f, 0.20f, 0.0f};
        else if (*startRoom == "exporter") vrSpawnFeet = {71.65f, 0.20f, 0.0f};
        else if (*startRoom == "interaction") vrSpawnFeet = {97.65f, 0.20f, 0.0f};
        else if (*startRoom == "projectile") vrSpawnFeet = {123.65f, 0.20f, 0.0f};
        else if (*startRoom == "teleport") vrSpawnFeet = {149.65f, 0.20f, 0.0f};
    }
    const HardwareTextureAtlas hardwareAtlas = BuildHardwareTextureAtlas(world.scene);
    std::vector<int> dynamicNodes = world.interactionPropNodes;
    dynamicNodes.insert(
        dynamicNodes.end(), world.projectilePropNodes.begin(), world.projectilePropNodes.end());
    const std::vector<ri::xr::HardwareSceneVertex> hardwareVertices =
        BuildHardwareScene(world.scene, hardwareAtlas, dynamicNodes, vrSpawnFeet, 42.0f);
    VrInteractionState interaction{};
    interaction.world = &world;
    interaction.atlas = &hardwareAtlas;
    interaction.vertices.reserve(
        (world.interactionProps.size() + world.projectileProps.size()) * 36U + 512U);
    VrLocomotionState locomotion{};
    locomotion.world = &world;
    locomotion.traceScene = ri::trace::TraceScene(world.colliders);
    locomotion.movement.body.bounds = BuildVrPlayerBounds(vrSpawnFeet);
    locomotion.movement.onGround = true;
    locomotion.options.simulateStamina = false;
    locomotion.options.maxGroundSpeed = 3.2f;
    locomotion.options.maxAirSpeed = 3.2f;
    locomotion.options.groundAcceleration = 45.0f;
    locomotion.options.airAcceleration = 16.0f;
    interaction.locomotion = &locomotion;
    interaction.authorityRuntime = &authorityRuntime;
    interaction.netcode = authorityNetcode;

    ri::xr::OpenXrRuntime runtime;
    if (!runtime.Initialize("Raw Iron VR Showcase", error)) {
        ri::core::LogSection("Raw Iron OpenXR / SteamVR");
        ri::core::LogInfo(error);
        ri::core::LogInfo("The shared showcase world still validated with "
                          + std::to_string(world.scene.NodeCount()) + " nodes.");
        return 2;
    }

    const ri::xr::RuntimeInfo& info = runtime.Info();
    ri::core::LogSection("Raw Iron OpenXR / SteamVR");
    ri::core::LogInfo("Runtime: " + info.runtimeName + " | HMD: " + info.systemName);
    ri::core::LogInfo("Stereo views: " + std::to_string(info.stereoViews.size())
                      + " | Shared scene nodes: " + std::to_string(world.scene.NodeCount()));
    for (const std::string& warning : info.warnings) {
        ri::core::LogInfo("OpenXR binding warning: " + warning);
    }
    const std::size_t initializationWarningCount = info.warnings.size();
    if (commandLine.HasFlag("--probe-only")) {
        ri::core::LogInfo("OpenXR system, Vulkan requirements, and Raw Iron action schema are ready.");
        runtime.PollEvents();
        authorityRuntime.Shutdown();
        return 0;
    }

    const int requestedFrames = std::clamp(commandLine.GetIntOr("--frames", 300), 1, 36000);
    ri::xr::OpenXrVulkanSession session(runtime);
    ri::xr::VulkanSessionRunReport report{};
    const ri::xr::HardwareSceneView hardwareScene{
        hardwareVertices.data(),
        hardwareVertices.size(),
        {vrSpawnFeet.x, vrSpawnFeet.y, vrSpawnFeet.z},
        0.05f,
        500.0f,
        hardwareAtlas.rgba.data(),
        HardwareTextureAtlas::kSize,
        HardwareTextureAtlas::kSize,
        &locomotion,
        &ResolveVrLocomotion,
        (world.interactionProps.size() + world.projectileProps.size()) * 36U + 512U,
        &interaction,
        &UpdateVrInteraction,
        ri::xr::HardwareTurnMode::Smooth,
        120.0f,
        30.0f};
    ri::core::LogInfo("Hardware Cube Test triangles: "
                      + std::to_string(hardwareVertices.size() / 3U)
                      + " | textures=" + std::to_string(hardwareAtlas.loadedTextures));
    if (!session.RunFrames(
            static_cast<std::uint32_t>(requestedFrames), report, error, &hardwareScene)) {
        ri::core::LogInfo("OpenXR Vulkan session failed: " + error);
        authorityRuntime.Shutdown();
        return 3;
    }
    for (std::size_t index = initializationWarningCount; index < info.warnings.size(); ++index) {
        ri::core::LogInfo("OpenXR session warning: " + info.warnings[index]);
    }
    ri::core::LogInfo(
        "OpenXR Vulkan frames: submitted=" + std::to_string(report.submittedFrames)
        + " stereoValid=" + std::to_string(report.validStereoFrames)
        + " leftTracked=" + std::to_string(report.leftControllerTrackedFrames)
        + " rightTracked=" + std::to_string(report.rightControllerTrackedFrames)
        + " focused=" + std::to_string(report.focusedFrames)
        + " leftActionActive=" + std::to_string(report.leftPoseActionActiveFrames)
        + " rightActionActive=" + std::to_string(report.rightPoseActionActiveFrames)
        + " aimSources=" + std::to_string(report.aimPoseBoundSourceCount)
        + " leftHandJoints=" + std::to_string(report.leftArticulatedHandFrames)
        + " rightHandJoints=" + std::to_string(report.rightArticulatedHandFrames)
        + " locomotion=" + std::to_string(report.locomotionInputFrames)
        + " snapTurns=" + std::to_string(report.snapTurnCount)
        + " selects=" + std::to_string(report.selectPressCount)
        + " dynamic=" + std::to_string(report.dynamicSceneFrames)
        + " haptics=" + std::to_string(report.hapticPulseCount)
        + " jumps=" + std::to_string(report.jumpPressCount)
        + " teleports=" + std::to_string(report.teleportCount)
        + " swapchain=" + std::to_string(report.width) + "x" + std::to_string(report.height));
    ri::core::LogInfo(
        "OpenXR interaction profiles: left="
        + (report.leftInteractionProfile.empty() ? std::string("unbound") : report.leftInteractionProfile)
        + " right="
        + (report.rightInteractionProfile.empty() ? std::string("unbound") : report.rightInteractionProfile));
    authorityRuntime.Shutdown();
    return 0;
}
