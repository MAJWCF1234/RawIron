#include "RawIron/Games/CubeTest/CubeTestWorld.h"

#include "RawIron/Scene/Scene.h"

#include <cstdlib>
#include <iostream>

namespace {

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    const ri::games::cubetest::CubeTestWorld world =
        ri::games::cubetest::BuildCubeTestWorld("Cube Test Smoke");

    bool ok = true;
    ok &= Require(world.rootNode != ri::scene::kInvalidHandle, "world should expose a valid root node");
    ok &= Require(world.platformNode != ri::scene::kInvalidHandle, "world should expose a valid platform node");
    ok &= Require(world.cubeNode != ri::scene::kInvalidHandle, "world should expose a valid cube node");
    ok &= Require(world.playerCameraNode != ri::scene::kInvalidHandle, "world should expose a valid camera node");
    ok &= Require(!world.colliders.empty(), "world should include at least one structural collider");

    const ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    ok &= Require(cube.material != ri::scene::kInvalidHandle, "cube should have a material");

    const ri::scene::Material& material = world.scene.GetMaterial(cube.material);
    ok &= Require(!material.baseColorTexture.empty(), "cube material should have an M-mesh base color map");
    ok &= Require(!material.normalTexture.empty(), "cube material should have an M-mesh normal map");
    ok &= Require(!material.ormTexture.empty(), "cube material should have an M-mesh packed spec/ORM map");
    ok &= Require(!material.detailTexture.empty(), "cube material should have a detail map for material stress testing");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
