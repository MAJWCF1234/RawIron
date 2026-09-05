#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Games/CubeTest/CubeTestAuthority.h"
#include "RawIron/Games/CubeTest/CubeTestRuntime.h"
#include "RawIron/Games/CubeTest/CubeTestGallery.h"

#include "RawIron/Content/RipakArchive.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Render/SceneTextureAudit.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/World/InteractivePropReplication.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <limits>
#include <string>
#include <unordered_set>

namespace {

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::uint64_t HashPixels(const std::vector<std::uint8_t>& pixels) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::uint8_t pixel : pixels) {
        hash = (hash ^ pixel) * 1099511628211ULL;
    }
    return hash;
}

bool MatricesNear(const ri::math::Mat4& lhs, const ri::math::Mat4& rhs) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (std::abs(lhs.m[row][column] - rhs.m[row][column]) > 1.0e-6f) {
                return false;
            }
        }
    }
    return true;
}

bool TestSharedAuthority(ri::games::cubetest::CubeTestWorld& desktop) {
    using namespace ri::games::cubetest;
    auto xr = BuildCubeTestWorld("XR authority replica");
    CubeTestAuthorityBridge host(&desktop), client(&xr);
    std::string error;
    bool ok = true;
    auto command = CubeTestAuthorityBridge::BuildProjectileCommand({125.2f,1.4f,0},{1,0,0});
    const auto initial = host.CaptureSnapshot(1);
    if (!Require(initial.has_value(), "authority initial capture")) return false;
    for (int i=0;i<4;++i) ok &= Require(host.HandleCommand(7,0,command,&error), "four authority commands per tick");
    ok &= Require(!host.HandleCommand(7,0,command,&error), "fifth command rejected");
    (void)host.CaptureSnapshot(1);
    ok &= Require(!host.HandleCommand(7,0,command,&error), "same-tick capture cannot reset rate limit");
    (void)host.CaptureSnapshot(2);
    ok &= Require(host.HandleCommand(7,0,command,&error), "new authority tick replenishes budget");
    const auto accepted = host.CaptureSnapshot(3);
    if (!Require(accepted.has_value() && client.ApplySnapshot(*accepted,&error), "desktop to XR snapshot")) return false;
    const auto equivalent = client.CaptureSnapshot(3);
    ok &= Require(equivalent && equivalent->bytes == accepted->bytes, "all replicated bytes match across hosts");
    AnimateCubeTestWorld(xr,1.0,false);
    const auto afterPresentation = client.CaptureSnapshot(3);
    ok &= Require(afterPresentation && afterPresentation->bytes==accepted->bytes, "remote presentation cannot step authoritative physics");
    // Corrupt only the second embedded pool after the first decoder would have succeeded.
    auto corrupt = *accepted;
    const auto firstPool = ri::world::BuildInteractivePropSnapshot(desktop.interactionProps,3);
    corrupt.bytes[16 + firstPool.bytes.size()] ^= 0xff;
    xr.interactionProps[0].position.x += .25f;
    const auto beforeCorruption = client.CaptureSnapshot(3);
    ok &= Require(!client.ApplySnapshot(corrupt,&error), "corrupt second pool rejected");
    const auto afterCorruption = client.CaptureSnapshot(3);
    ok &= Require(beforeCorruption && afterCorruption && beforeCorruption->bytes==afterCorruption->bytes,
        "both pools roll back atomically");
    corrupt = *accepted; corrupt.bytes.push_back(0xff);
    ok &= Require(!client.ApplySnapshot(corrupt,&error), "trailing snapshot bytes rejected");
    corrupt = *accepted; corrupt.bytes.resize(10);
    ok &= Require(!client.ApplySnapshot(corrupt,&error), "truncated snapshot rejected");
    const auto beforeInvalid = host.CaptureSnapshot(4);
    for (const auto invalid : {
        CubeTestAuthorityBridge::BuildProjectileCommand({999,0,0},{1,0,0}),
        CubeTestAuthorityBridge::BuildProjectileCommand({125,1,0},{0,0,0}),
        CubeTestAuthorityBridge::BuildProjectileCommand({125,1,0},{std::numeric_limits<float>::max(),0,0}),
        CubeTestAuthorityBridge::BuildProjectileCommand({std::numeric_limits<float>::quiet_NaN(),1,0},{1,0,0})})
        ok &= Require(!host.HandleCommand(8,0,invalid,&error), "nonfinite/zero/overflow/out-of-room command rejected");
    ok &= Require(!host.HandleCommand(8,1,command,&error), "wrong command channel rejected");
    command.push_back(0);
    ok &= Require(!host.HandleCommand(8,0,command,&error), "trailing command bytes rejected");
    auto afterInvalid = host.CaptureSnapshot(4);
    ok &= Require(beforeInvalid && afterInvalid && beforeInvalid->bytes==afterInvalid->bytes, "invalid commands never mutate state");
    command.pop_back();
    for (std::size_t peer=0;peer<32;++peer) ok &= Require(host.HandleCommand(peer,0,command,&error), "bounded peer accepts command");
    ok &= Require(!host.HandleCommand(32,0,command,&error), "peer memory budget bounded");
    (void)host.CaptureSnapshot(5);
    ok &= Require(host.HandleCommand(32,0,command,&error), "old peer entries retire at next tick");
    CubeTestAuthorityBridge detached;
    ok &= Require(!detached.CaptureSnapshot(0) && !detached.HandleCommand(1,0,command,&error), "detached bridge fails safely");
    return ok;
}

bool TestGalleryContracts(const ri::games::cubetest::CubeTestWorld& world) {
    using namespace ri::games::cubetest;
    bool ok = true;
    std::size_t structuralExhibits=0;
    for (const auto& node : world.scene.Nodes()) {
        if (!node.name.starts_with("CubeTest_Procedural_") || node.name.ends_with("_Plinth")) continue;
        ++structuralExhibits;
        const auto& mesh=world.scene.GetMesh(node.mesh);
        ok &= Require(!node.structuralBrush.brushId.empty() && !node.structuralBrush.visualMesh.meshId.empty()
            && !node.structuralBrush.physicsMesh.meshId.empty() && !node.structuralBrush.queryMesh.meshId.empty(),
            "procedural exhibits must be authored through the structural collection, with M/P/Q ownership");
        ok &= Require(mesh.positions.size()>100 && mesh.normals.size()==mesh.positions.size()
            && mesh.texCoords.size()==mesh.positions.size(),"structural exhibits retain normals and UV streams");
    }
    ok &= Require(structuralExhibits==9,"three new structural platforms must contain all nine exhibits");
    const auto coffeeBounds = ri::scene::ComputeNodeWorldBounds(world.scene, world.coffeeModelNode);
    ok &= Require(coffeeBounds.has_value() && coffeeBounds->min.x > 70.0f && coffeeBounds->max.x < 86.0f
        && coffeeBounds->min.y >= 0.0f && coffeeBounds->max.y < 4.0f
        && coffeeBounds->min.z > -8.0f && coffeeBounds->max.z < 8.0f,
        "coffee exhibit must fit inside its room, not enclose and shadow adjacent rooms");
    const ri::trace::TraceScene trace(world.colliders);
    std::unordered_set<std::string> ids;
    const auto playerBounds = [](const ri::math::Vec3& feet) {
        return ri::spatial::Aabb{.min={feet.x-0.25f, feet.y, feet.z-0.25f},
            .max={feet.x+0.25f, feet.y+1.8f, feet.z+0.25f}};
    };
    for (const auto& portal : world.portals) {
        ok &= Require(ids.insert(portal.id).second && !portal.label.empty(), "portal ID must be unique and labeled");
        ok &= Require(CubeTestRoomAt(portal.destinationFeet.x).id == portal.destinationId,
            "portal destination ID must match its authored arrival room");
        const auto arrival = playerBounds(portal.destinationFeet);
        ok &= Require(!trace.TraceBox(arrival).has_value(), "portal arrival must clear the full standing player volume");
        ok &= Require(trace.FindGroundHit(portal.destinationFeet, {.maxDistance=0.4f}).has_value(),
            "portal arrival must have nearby walkable support");
        ri::world::PortalTravelerState traveler{};
        const auto triggerFeet = ri::math::Vec3{(portal.triggerBounds.min.x + portal.triggerBounds.max.x)*0.5f, 0.2f, 0};
        const auto travel = ri::world::UpdatePortalTraveler(world.portals, playerBounds(triggerFeet), 0.016f, traveler);
        ok &= Require(travel.traveled && travel.portalId == portal.id && !travel.preserveVelocity
            && std::abs(travel.destinationFeet.x - portal.destinationFeet.x) < 1e-5f
            && travel.destinationYawDegrees == portal.destinationYawDegrees, "every portal must transfer to its exact authored pose");
        ok &= Require(!ri::world::UpdatePortalTraveler(world.portals, arrival, 0.016f, traveler).traveled
            && !ri::world::UpdatePortalTraveler(world.portals, arrival, 1.0f, traveler).traveled,
            "arrival must not bounce through another trigger even after cooldown");
        const auto split = portal.id.find("-to-");
        const auto reverse = portal.destinationId + "-to-" + portal.id.substr(0, split);
        ok &= Require(std::any_of(world.portals.begin(), world.portals.end(), [&](const auto& p) { return p.id == reverse; }),
            "each authored portal route must have a return route");
    }
    for (const auto& room : CubeTestRoomGuides()) {
        ok &= Require(FindCubeTestRoom(room.id)==&room && !trace.TraceBox(playerBounds(CubeTestRoomArrival(room))).has_value(),
            "desktop and XR use the same clear room arrival");
        ok &= Require(CubeTestRoomAt(room.centerX).id == room.id && !room.subsystem.empty()
            && !room.reference.empty() && !room.controls.empty() && !room.observation.empty(),
            "each room needs discoverable subsystem, reference, controls and expected observation");
        ok &= Require(CubeTestGalleryHelp().find(DescribeCubeTestRoom(room)) != std::string::npos,
            "CLI guide must include the same room content used by in-game help");
    }
    const auto allTextures = ri::render::software::AuditSceneTextures(world.scene);
    ok &= Require(!allTextures.empty(), "gallery texture audit must inspect authored textures");
    for (const auto& entry : allTextures) {
        if (!entry.error.empty()) std::cerr << ri::render::software::DescribeSceneTexture(entry) << '\n';
        ok &= Require(entry.error.empty() && entry.width > 0 && entry.height > 0,
            "every gallery texture must decode without fallback");
    }
    bool unsupportedMarker = false;
    for (std::size_t index = 0; index < world.scene.MaterialCount(); ++index) {
        const auto& sample = world.scene.GetMaterial(static_cast<int>(index));
        if (sample.name.find("[UNSUPPORTED TEXTURE]") != std::string::npos) {
            unsupportedMarker = true;
            ok &= Require(sample.baseColor.x == 1.0f && sample.baseColor.y == 0.0f && sample.baseColor.z == 1.0f,
                "unsupported embedded base-color textures must produce an explicit magenta marker, not white");
        }
    }
    ok &= Require(unsupportedMarker, "coffee's unsupported KTX2 images must not disappear without a visible diagnostic");

    ri::scene::Scene probe("texture defaults");
    const int materialIndex = probe.AddMaterial({.name="default-material"});
    auto& material = probe.GetMaterial(materialIndex);
    ok &= Require(material.shadingModel == ri::scene::ShadingModel::Lit && material.metallic == 0.0f
        && material.roughness == 1.0f && material.opacity == 1.0f && !material.transparent
        && material.normalScale.x == 1.0f && material.normalScale.y == 1.0f
        && ri::render::software::AuditSceneTextures(probe).empty(),
        "default scalar material must remain opaque, rough, nonmetallic with intentional empty maps");
    const auto scratch = std::filesystem::temp_directory_path() / ("RawIronTextureAudit-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    { std::ofstream corrupt(scratch / "corrupt.png"); corrupt << "not an image"; }
    const auto validTexture = world.scene.GetMaterial(world.scene.GetNode(world.cubeNode).material).baseColorTexture;
    const std::array<std::string ri::scene::Material::*, 9> slots{
        &ri::scene::Material::baseColorTexture, &ri::scene::Material::normalTexture, &ri::scene::Material::ormTexture,
        &ri::scene::Material::roughnessTexture, &ri::scene::Material::metallicTexture, &ri::scene::Material::occlusionTexture,
        &ri::scene::Material::emissiveTexture, &ri::scene::Material::opacityTexture, &ri::scene::Material::detailTexture};
    for (const auto slot : slots) {
        for (const char* filename : {"missing.png", "corrupt.png"}) {
            material.*slot = filename;
            const auto audit = ri::render::software::AuditSceneTextures(probe, scratch);
            ok &= Require(audit.size() == 1 && !audit.front().error.empty()
                && ri::render::software::DescribeSceneTexture(audit.front()).find(filename) != std::string::npos,
                "missing/corrupt maps must identify the requested path and reject fallback for every slot");
        }
        (material.*slot).clear();
    }
    material.baseColorTexture = material.normalTexture = validTexture;
    auto audit = ri::render::software::AuditSceneTextures(probe);
    ok &= Require(audit.size() == 2 && audit[0].colorSpace == "sRGB" && audit[1].colorSpace == "linear"
        && audit[0].source == audit[1].source && audit[0].error.empty() && audit[1].error.empty(),
        "one source used as color and data must retain distinct interpretation");
    material.baseColorTextureFrames = {(scratch / "missing-frame.png").string()};
    audit = ri::render::software::AuditSceneTextures(probe);
    ok &= Require(audit.back().slot == "albedo-frame-0" && !audit.back().error.empty(),
        "texture animation must preflight frames beyond the currently selected image");

    StandaloneOptions options{};
    options.workspaceRoot = scratch;
    char executable[] = "gallery-test";
    char* arguments[] = {executable};
    const ri::core::CommandLine commandLine(1, arguments);
    std::string error;
    ok &= Require(!RunStandalone(options, commandLine, &error) && error.find("fallback forbidden") != std::string::npos,
        "ordinary gallery host must reject missing fixtures before opening a window");
    options.startRoom = "typo";
    ok &= Require(!RunStandalone(options, commandLine, &error) && error.find("Unknown Cube Test start room") != std::string::npos,
        "unknown room IDs must fail instead of silently starting at baseline");
    std::filesystem::remove_all(scratch);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--calibration-only") {
        const auto workspace = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
        auto calibration = ri::games::cubetest::BuildCubeTestCalibrationWorld(workspace);
        bool ok = Require(calibration.materialCalibration && calibration.portals.empty()
            && calibration.interactionProps.empty() && calibration.cubeNode == ri::scene::kInvalidHandle,
            "calibration must not include animated gallery content");
        const auto initial = calibration.scene.ComputeWorldMatrix(calibration.playerCameraNode);
        ri::games::cubetest::AnimateCubeTestWorld(calibration, 12.0);
        ok &= Require(MatricesNear(initial, calibration.scene.ComputeWorldMatrix(calibration.playerCameraNode)),
            "calibration animation must not move the camera");
        for (const auto& node : calibration.scene.Nodes()) {
            if (node.material == ri::scene::kInvalidHandle) continue;
            const auto& material = calibration.scene.GetMaterial(node.material);
            for (const auto& path : {material.baseColorTexture, material.normalTexture}) {
                if (path.empty()) continue;
                const auto image = ri::render::software::LoadRgbaImageFile(path);
                const bool albedo = node.name == "Calibration_sRGB";
                ok &= Require(image.Valid() && image.width == (albedo ? 2048 : 512)
                    && image.height == (albedo ? 1024 : 512),
                    "project-owned Three.js calibration texture must decode at its original resolution");
                const std::string expected = albedo ? "hardwood2_diffuse.jpg"
                    : node.name == "Calibration_NormalMapDirectX" ? "NormalMapDirectX.png" : "NormalMapOpenGL.png";
                ok &= Require(std::filesystem::path(path).filename() == expected,
                    "calibration must bind the original Three.js texture, not a substitute");
            }
        }
        ri::render::software::ScenePreviewOptions preview{};
        preview.width = 320;
        preview.height = 180;
        preview.fogStrength = 0.0f;
        preview.orderedDither = false;
        const auto first = ri::render::software::RenderScenePreview(calibration.scene, calibration.playerCameraNode, preview);
        const auto second = ri::render::software::RenderScenePreview(calibration.scene, calibration.playerCameraNode, preview);
        ok &= Require(first.pixels.size() == 320U * 180U * 3U && first.pixels == second.pixels,
            "calibration software frames must be complete and repeatable (not GPU validation)");

        // Exercise actual host preflight without modifying project assets or opening a window.
        const auto scratch = std::filesystem::temp_directory_path() / ("RawIronCalibration-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto assetRoot = scratch / "Games" / "CubeTest" / "assets" / "reference" / "threejs-r185" / "textures";
        std::filesystem::create_directories(assetRoot);
        ri::games::cubetest::StandaloneOptions options{};
        options.workspaceRoot = scratch;
        options.materialCalibration = true;
        char executable[] = "calibration-test";
        char savePreview[] = "--save-preview";
        char* args[] = {executable, savePreview};
        const ri::core::CommandLine commandLine(2, args);
        std::string error;
        ok &= Require(!ri::games::cubetest::RunStandalone(options, commandLine, &error)
            && error.find("Calibration texture missing, invalid") != std::string::npos,
            "missing calibration texture must fail before opening a window");
        { std::ofstream corrupt(assetRoot / "hardwood2_diffuse.jpg", std::ios::binary); corrupt << "not an image"; }
        error.clear();
        ok &= Require(!ri::games::cubetest::RunStandalone(options, commandLine, &error)
            && error.find("Calibration texture missing, invalid") != std::string::npos,
            "corrupt calibration texture must fail before opening a window");
        std::filesystem::remove_all(scratch);
        options.nativeCapturePath = "capture.bmp";
        error.clear();
        ok &= Require(!ri::games::cubetest::RunStandalone(options, commandLine, &error)
            && error.find("cannot be combined with software preview") != std::string::npos,
            "GPU capture must not silently run the software preview path");
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    ri::games::cubetest::CubeTestWorld world =
        ri::games::cubetest::BuildCubeTestWorld("Cube Test Smoke");

    if (argc == 2 && std::string_view(argv[1]) == "--gallery-contracts")
        return TestGalleryContracts(world) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--shared-authority")
        return TestSharedAuthority(world) ? EXIT_SUCCESS : EXIT_FAILURE;

    bool ok = true;
    ok &= Require(world.rootNode != ri::scene::kInvalidHandle, "world should expose a valid root node");
    ok &= Require(world.platformNode != ri::scene::kInvalidHandle, "world should expose a valid platform node");
    ok &= Require(world.cubeNode != ri::scene::kInvalidHandle, "world should expose a valid cube node");
    ok &= Require(world.playerCameraNode != ri::scene::kInvalidHandle, "world should expose a valid camera node");
    ok &= Require(!world.colliders.empty(), "world should include at least one structural collider");
    ok &= Require(world.colliders.size() >= 6U, "capability gallery should expose collider-backed test areas");
    ok &= Require(world.portals.size() == 2U * (ri::games::cubetest::CubeTestRoomGuides().size()-1U), "every adjacent gallery pair needs bidirectional portal links");
    ok &= Require(world.interactionRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the native XR interaction room");
    ok &= Require(world.interactionProps.size() == 24U
                      && world.interactionPropNodes.size() == world.interactionProps.size(),
                  "XR interaction room should bind every native prop state to a scene node");
    ok &= Require(world.projectileRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the native pooled-projectile room");
    ok &= Require(world.projectileProps.size() == 50U
                      && world.projectilePropNodes.size() == world.projectileProps.size(),
                  "projectile room should bind targets and every fixed pool slot to scene nodes");
    ok &= Require(world.teleportRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the trace-validated teleport room");
    const ri::world::InteractivePropEmissionResult fired =
        ri::games::cubetest::EmitCubeTestProjectile(
            world, {125.1f, 1.5f, 0.0f}, {1.0f, 0.0f, 0.0f});
    ok &= Require(fired.propIndex >= 18 && world.projectileProps[fired.propIndex].active,
                  "shared room API should activate a pooled projectile");
    ri::games::cubetest::CubeTestWorld authorityClientWorld =
        ri::games::cubetest::BuildCubeTestWorld("Cube Test Authority Client");
    ri::games::cubetest::CubeTestAuthorityBridge authorityHost(&world);
    ri::games::cubetest::CubeTestAuthorityBridge authorityClient(&authorityClientWorld);
    const std::vector<std::uint8_t> authorityProjectile =
        ri::games::cubetest::CubeTestAuthorityBridge::BuildProjectileCommand(
            {125.2f, 1.4f, 0.0f}, {1.0f, 0.0f, 0.0f});
    std::string authorityError;
    const std::size_t activeBeforeAuthorityCommand = static_cast<std::size_t>(std::count_if(
        world.projectileProps.begin(), world.projectileProps.end(),
        [](const ri::world::InteractivePropState& prop) { return prop.active; }));
    ok &= Require(authorityHost.HandleCommand(7U, 0U, authorityProjectile, &authorityError),
                  "authority bridge should accept a bounded projectile command");
    const std::size_t activeAfterAuthorityCommand = static_cast<std::size_t>(std::count_if(
        world.projectileProps.begin(), world.projectileProps.end(),
        [](const ri::world::InteractivePropState& prop) { return prop.active; }));
    ok &= Require(activeAfterAuthorityCommand > activeBeforeAuthorityCommand,
                  "authority command should mutate only the host physics pool");
    const auto authoritySnapshot = authorityHost.CaptureSnapshot(42U);
    ok &= Require(authoritySnapshot.has_value()
                      && authorityClient.ApplySnapshot(*authoritySnapshot, &authorityError),
                  "desktop and XR authority bridge should round-trip both dynamic pools");
    ok &= Require(authorityClientWorld.projectileProps.size() == world.projectileProps.size()
                      && authorityClientWorld.projectileProps[18].active == world.projectileProps[18].active
                      && std::abs(authorityClientWorld.projectileProps[18].position.x
                                  - world.projectileProps[18].position.x) < 1.0e-6f,
                  "authority snapshot should reproduce the host projectile state exactly");
    ok &= Require(world.spriteNode != ri::scene::kInvalidHandle, "sprite room should expose a native sprite cloud");
    ok &= Require(world.spriteFrameMeshes.size() == 32U, "sprite room should prebuild smooth frames for four layouts");
    const int initialSpriteMesh = world.scene.GetNode(world.spriteNode).mesh;
    const ri::scene::Mesh& spriteMesh = world.scene.GetMesh(initialSpriteMesh);
    ok &= Require(spriteMesh.positions.size() == 512U * 4U,
                  "sprite cloud should pack all 512 reference sprites into one mesh draw");
    ok &= Require(spriteMesh.geometryMode == ri::scene::MeshGeometryMode::CameraFacingSpriteQuads,
                  "sprite cloud should use the engine camera-facing mesh contract");
    ok &= Require(spriteMesh.billboardOffsets.size() == spriteMesh.positions.size(),
                  "sprite cloud should provide an explicit billboard-offset stream");
    ok &= Require(world.exporterInstanceBatch != ri::scene::kInvalidHandle,
                  "export room should expose a hierarchy and instance-batch sample");
    ok &= Require(world.shaderBallNode != ri::scene::kInvalidHandle,
                  "export room should import the exact Three.js ShaderBall glTF fixture");
    ok &= Require(world.coffeeModelNode != ri::scene::kInvalidHandle,
                  "export room should import the exact meshopt-compressed coffee fixture");
    const std::filesystem::path referenceModelRoot = std::filesystem::path(__FILE__).parent_path()
        / ".." / ".." / "assets" / "reference" / "threejs-r185" / "models" / "gltf";
    const ri::scene::ModelSourceValidationReport shaderBallReport =
        ri::scene::ValidateModelSource(referenceModelRoot / "ShaderBall.glb");
    ok &= Require(shaderBallReport.valid,
                  "Raw Iron should import the quantized Three.js ShaderBall fixture through its public model API");
    const ri::scene::ModelSourceValidationReport coffeeReport =
        ri::scene::ValidateModelSource(referenceModelRoot / "coffeemat.glb");
    if (!coffeeReport.valid) {
        std::cerr << "coffeemat import: " << coffeeReport.summary << '\n';
    }
    ok &= Require(coffeeReport.valid,
                  "Raw Iron should decode the meshopt-compressed Three.js coffee fixture through its public model API");
    ok &= Require(std::filesystem::path(
                      world.scene.GetMaterial(world.scene.GetNode(world.spriteNode).material).baseColorTexture)
                      .filename() == "sprite.png",
                  "sprite room should use the exact Three.js sprite fixture");
    int normalFixtureCount = 0;
    for (const ri::scene::Node& node : world.scene.Nodes()) {
        if (!node.name.starts_with("NormalComparison_")) {
            continue;
        }
        const ri::scene::Material& panelMaterial = world.scene.GetMaterial(node.material);
        ok &= Require(std::filesystem::is_regular_file(panelMaterial.normalTexture),
                      "normal-map panels should resolve exact Three.js texture fixtures");
        if (node.name.find("DirectXConverted") == std::string::npos) {
            ok &= Require(panelMaterial.normalScale.y < 0.0f,
                          "OpenGL and unconverted control need the common top-first image-basis correction");
        } else {
            ok &= Require(panelMaterial.normalScale.y > 0.0f,
                          "DirectX inversion must compose with the common image-basis correction");
        }
        ++normalFixtureCount;
    }
    ok &= Require(normalFixtureCount == 6, "normal-map room should expose standard and mirrored convention controls");

    const ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    ok &= Require(cube.material != ri::scene::kInvalidHandle, "cube should have a material");

    const ri::scene::Material& material = world.scene.GetMaterial(cube.material);
    ok &= Require(!material.baseColorTexture.empty(), "cube material should have an M-mesh base color map");
    ok &= Require(std::filesystem::path(material.baseColorTexture).filename() == "hardwood2_diffuse.jpg",
                  "baseline cube must use the original Three.js hardwood texture");
    ok &= Require(material.normalTexture.empty() && material.ormTexture.empty() && material.detailTexture.empty(),
                  "baseline must not bind unrelated LRT maps or reinterpret source images as data maps");
    ok &= Require(cube.localTransform.scale.x < 1.5f && cube.localTransform.scale.y < 1.5f,
                  "streaming cube should remain a small focused sample");
    for (const int sampleNode : {
             world.cubeNode,
             world.goldSampleNode,
             world.copperSampleNode,
             world.ironSampleNode,
             world.crystalSampleNode,
         }) {
        const ri::scene::Node& sample = world.scene.GetNode(sampleNode);
        const ri::scene::Material& sampleMaterial = world.scene.GetMaterial(sample.material);
        const auto texture = std::filesystem::path(sampleMaterial.baseColorTexture);
        ok &= Require(std::filesystem::is_regular_file(texture)
                          && texture.parent_path().filename() == "textures"
                          && texture.parent_path().parent_path().filename() == "threejs-r185"
                          && (texture.filename() == "hardwood2_diffuse.jpg" || texture.filename() == "uv_grid_opengl.jpg"),
                      "baseline albedo must resolve inside the experience's copied Three.js assets");
    }

    ri::games::cubetest::ConfigureCookedTextureCube(
        world, ri::games::cubetest::CubeTestCookedTextureSequence());
    const ri::scene::Material& cookedMaterial = world.scene.GetMaterial(cube.material);
    ok &= Require(cookedMaterial.baseColorTextureFrames.size() >= 5U,
                  "streaming cube should expose several cooked texture frames");
    ok &= Require(cookedMaterial.normalTexture.empty() && cookedMaterial.ormTexture.empty(),
                  "cooked albedo demo should use safe fallback maps instead of loose sidecars");
    const float initialYaw = cube.localTransform.rotationDegrees.y;
    const ri::math::Mat4 initialCameraWorld = world.scene.ComputeWorldMatrix(world.playerCameraNode);
    ri::games::cubetest::AnimateCubeTestWorld(world, 1.0);
    ok &= Require(world.scene.GetNode(world.cubeNode).localTransform.rotationDegrees.y > initialYaw + 30.0f,
                  "streaming cube should spin continuously");
    ri::games::cubetest::AnimateCubeTestWorld(world, 6.5);
    ok &= Require(world.scene.GetNode(world.spriteNode).mesh != initialSpriteMesh,
                  "sprite room should transition between native layouts");
    ok &= Require(MatricesNear(initialCameraWorld, world.scene.ComputeWorldMatrix(world.playerCameraNode)),
                  "world animation must never orbit or rotate the player camera");

    ri::render::software::ScenePreviewOptions previewOptions{};
    previewOptions.width = 320;
    previewOptions.height = 180;
    previewOptions.textureRoot = ri::render::software::DefaultEngineTextureRoot();
    previewOptions.clearTop = {0.58f, 0.66f, 0.72f};
    previewOptions.clearBottom = {0.32f, 0.35f, 0.36f};
    previewOptions.fogStartDepth = 18.0f;
    previewOptions.fogEndDepth = 80.0f;
    previewOptions.fogStrength = 0.28f;
    std::unordered_set<std::uint64_t> frameHashes;
    for (int frameIndex = 0; frameIndex < 8; ++frameIndex) {
        ri::games::cubetest::CubeTestWorld frameWorld =
            ri::games::cubetest::BuildCubeTestWorld("Cube Test Render Safety");
        const double seconds = static_cast<double>(frameIndex) / 12.0;
        ri::games::cubetest::AnimateCubeTestWorldJiggle(frameWorld, seconds);
        previewOptions.animationTimeSeconds = seconds;
        const ri::render::software::SoftwareImage frame = ri::render::software::RenderScenePreview(
            frameWorld.scene, frameWorld.playerCameraNode, previewOptions);
        ok &= Require(frame.width == previewOptions.width && frame.height == previewOptions.height,
                      "jiggle preview should preserve its requested dimensions");
        ok &= Require(frame.pixels.size()
                          == static_cast<std::size_t>(previewOptions.width * previewOptions.height * 3),
                      "jiggle preview should return a complete RGB frame");
        std::size_t nearBlackPixels = 0U;
        for (std::size_t offset = 0; offset + 2U < frame.pixels.size(); offset += 3U) {
            if (frame.pixels[offset] < 4U && frame.pixels[offset + 1U] < 4U
                && frame.pixels[offset + 2U] < 4U) {
                ++nearBlackPixels;
            }
        }
        ok &= Require(nearBlackPixels < static_cast<std::size_t>(frame.width * frame.height) / 100U,
                      "jiggle preview should not generate screen-filling black raster artifacts");
        frameHashes.insert(HashPixels(frame.pixels));
    }
    ok &= Require(frameHashes.size() >= 4U,
                  "jiggle preview sequence should contain multiple distinct rendered frames");

    if (argc == 2) {
        auto pack = std::make_shared<ri::content::CookedTexturePack>(
            ri::content::CookedTexturePack::Open(argv[1], "indexes/RAWIRONX32.index.json"));
        ri::games::cubetest::CubeTestWorld cookedWorld =
            ri::games::cubetest::BuildCubeTestWorld("Cube Test Cooked Streaming");
        ri::games::cubetest::ConfigureCookedTextureCube(
            cookedWorld, ri::games::cubetest::CubeTestCookedTextureSequence());
        ri::render::software::ScenePreviewOptions cookedOptions = previewOptions;
        cookedOptions.cookedTexturePack = pack;
        ri::render::software::ScenePreviewCache cookedCache{};
        std::unordered_set<std::uint64_t> cookedHashes;
        for (const double seconds : {0.0, 1.4, 2.8}) {
            cookedOptions.animationTimeSeconds = seconds;
            const ri::render::software::SoftwareImage frame = ri::render::software::RenderScenePreview(
                cookedWorld.scene, cookedWorld.playerCameraNode, cookedOptions, &cookedCache);
            cookedHashes.insert(HashPixels(frame.pixels));
        }
        ok &= Require(cookedHashes.size() == 3U,
                      "three cooked texture intervals should produce three distinct cube frames");
        ok &= Require(cookedCache.textures.size() >= 3U,
                      "software renderer should cache each requested cooked texture without extraction");
    } else if (argc != 1) {
        ok &= Require(false, "expected optional RAWIRONX32.ripak path only");
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
