#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/KeyboardMovementInput.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    ri::trace::KeyboardMovementEdges edges;
    ri::trace::KeyboardMovementSample keys{.focused=true, .forward=true, .right=true, .jump=true, .sprint=true};
    auto input = ri::trace::BuildKeyboardMovementInput(keys,90,edges);
    if (input.moveForward != 1 || input.moveRight != 1 || !input.sprintHeld || !input.jumpPressed
        || std::abs(input.viewForwardWorld.x-1)>1e-5f) return EXIT_FAILURE;
    if (ri::trace::BuildKeyboardMovementInput(keys,90,edges).jumpPressed) return EXIT_FAILURE;
    keys.back = keys.left = true;
    input = ri::trace::BuildKeyboardMovementInput(keys,0,edges);
    if (input.moveForward != 0 || input.moveRight != 0) return EXIT_FAILURE;
    keys.focused = false;
    input = ri::trace::BuildKeyboardMovementInput(keys,0,edges);
    if (input.moveForward != 0 || input.moveRight != 0 || input.sprintHeld || input.jumpPressed || edges.jumpHeldLastFrame)
        return EXIT_FAILURE;
    ri::trace::TraceScene scene{};
    ri::trace::MovementControllerState state{
        .body = {
            .bounds = {
                .min = {-0.5f, 0.0f, -0.5f},
                .max = {0.5f, 1.0f, 0.5f},
            },
            .velocity = {4.0f, 0.0f, 0.0f},
        },
        .onGround = false,
    };
    ri::trace::MovementControllerOptions options{};
    options.gravity = 0.0f;
    options.fallGravityMultiplier = 1.0f;
    options.simulateStamina = false;
    options.kinematic.linearDamping = 1.0f;
    options.kinematic.angularDamping = 1.0f;
    options.kinematic.airDrag = 1.0f;
    options.kinematic.minVelocity = 0.0f;

    const ri::trace::MovementControllerResult advanced =
        ri::trace::SimulateMovementControllerStep(scene, state, {}, 0.35f, options);
    const float centerX = ri::spatial::Center(advanced.state.body.bounds).x;
    if (std::fabs(centerX - 1.4f) > 0.001f
        || advanced.sliceCount != 4U
        || std::fabs(advanced.consumedSeconds - 0.35f) > 0.0001f
        || advanced.hitSliceBudget) {
        return EXIT_FAILURE;
    }

    const ri::trace::MovementControllerResult rejected =
        ri::trace::SimulateMovementControllerStep(scene, state, {}, -1.0f, options);
    if (rejected.sliceCount != 0U || rejected.consumedSeconds != 0.0f || rejected.hitSliceBudget) {
        return EXIT_FAILURE;
    }
    const auto makePlayer = [] {
        ri::trace::MovementControllerState player;
        player.body.bounds = {{-.25f,.01f,-.25f},{.25f,1.81f,.25f}};
        player.onGround = true;
        return player;
    };
    ri::trace::TraceScene ground({{"floor",{{-100,-1,-100},{100,0,100}}}});
    ri::trace::MovementControllerOptions controls;
    controls.simulateStamina = false;
    const auto travel = [&](bool sprint) {
        auto player = makePlayer();
        ri::trace::KeyboardMovementEdges latch;
        for (int i=0;i<120;++i) {
            const auto move = ri::trace::BuildKeyboardMovementInput(
                ri::trace::KeyboardMovementSample{.focused=true,.forward=true,.sprint=sprint},0,latch);
            player = ri::trace::SimulateMovementControllerStep(ground,player,move,1.f/120,controls).state;
        }
        return ri::spatial::Center(player.body.bounds).z;
    };
    if (travel(false)<1 || travel(true)<travel(false)+.5f) {
        std::cerr << "WASD/sprint travel failed\n"; return EXIT_FAILURE;
    }
    auto jumper = makePlayer();
    float peak = 0;
    for (int i=0;i<360;++i) {
        jumper = ri::trace::SimulateMovementControllerStep(ground,jumper,
            ri::trace::MovementInput{.jumpPressed=i==0},1.f/120,controls).state;
        peak = std::max(peak,jumper.body.bounds.min.y);
    }
    if (peak<.3f || jumper.body.bounds.min.y>.1f || jumper.body.bounds.min.y<-.01f) {
        std::cerr << "Jump/landing failed\n"; return EXIT_FAILURE;
    }
    ri::trace::TraceScene walled({{"floor",{{-100,-1,-100},{100,0,100}}},
        {"wall",{{-5,0,3},{5,5,4}}}});
    auto blocked = makePlayer();
    for (int i=0;i<240;++i)
        blocked = ri::trace::SimulateMovementControllerStep(walled,blocked,
            ri::trace::MovementInput{.moveForward=1,.sprintHeld=true},1.f/120,controls).state;
    if (blocked.body.bounds.max.z>3.01f || blocked.body.bounds.max.z<2.9f) {
        std::cerr << "Sprint wall collision failed\n"; return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
