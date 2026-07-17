#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/Scene.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

int main() {
    using namespace ri::scene;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (const double badDuration : {nan, -1.0, std::numeric_limits<double>::infinity()}) {
        AnimationClip invalid{.durationSeconds = badDuration};
        AnimationPlayer player(&invalid);
        player.Play();
        CHECK(!player.IsPlaying());
        player.SetTimeSeconds(10.0);
        CHECK(player.TimeSeconds() == 0.0);
        player.AdvanceSeconds(1.0);
        CHECK(player.TimeSeconds() == 0.0);
    }

    Scene scene{"animation-safety"};
    const int node = scene.CreateNode("Animated");
    AnimationClip clip{.durationSeconds = 2.0, .looping = false};
    Transform invalidTransform{};
    invalidTransform.position.x = std::numeric_limits<float>::quiet_NaN();
    clip.nodeTracks[node] = {
        {.timeSeconds = 1.0, .transform = {.position = {10.0f, 0.0f, 0.0f}}},
        {.timeSeconds = nan, .transform = {.position = {999.0f, 0.0f, 0.0f}}},
        {.timeSeconds = 0.0, .transform = {.position = {1.0f, 0.0f, 0.0f}}},
        {.timeSeconds = 0.0, .transform = {.position = {20.0f, 0.0f, 0.0f}}},
        {.timeSeconds = 0.5, .transform = invalidTransform},
    };

    ApplyAnimationClip(scene, clip, 0.5);
    CHECK(std::abs(scene.GetNode(node).localTransform.position.x - 15.0f) < 0.0001f);
    ApplyAnimationClip(scene, clip, nan);
    CHECK(scene.GetNode(node).localTransform.position.x == 20.0f);

    AnimationPlayer player(&clip);
    player.Play();
    CHECK(player.IsPlaying());
    player.AdvanceSeconds(std::numeric_limits<double>::max());
    CHECK(!player.IsPlaying());
    CHECK(player.TimeSeconds() == clip.durationSeconds);

    player.SetLooping(true);
    CHECK(player.TimeSeconds() == 0.0);
    player.Play();
    player.AdvanceSeconds(std::numeric_limits<double>::max());
    CHECK(player.IsPlaying());
    CHECK(std::isfinite(player.TimeSeconds()));
    CHECK(player.TimeSeconds() >= 0.0 && player.TimeSeconds() < clip.durationSeconds);
    player.AdvanceSeconds(std::numeric_limits<double>::infinity());
    CHECK(std::isfinite(player.TimeSeconds()));

    AnimationClip allInvalid{.durationSeconds = 1.0};
    allInvalid.nodeTracks[node] = {
        {.timeSeconds = nan, .transform = {}},
        {.timeSeconds = 0.0, .transform = invalidTransform},
    };
    scene.GetNode(node).localTransform.position = {5.0f, 5.0f, 5.0f};
    ApplyAnimationClip(scene, allInvalid, 0.0);
    CHECK(scene.GetNode(node).localTransform.position.x == 0.0f);
    CHECK(scene.GetNode(node).localTransform.scale.x == 1.0f);

    AnimationClip extreme{.durationSeconds = 1.0, .looping = false};
    extreme.nodeTracks[node] = {
        {.timeSeconds = 0.0,
         .transform = {.position = {std::numeric_limits<float>::max(), 0.0f, 0.0f}}},
        {.timeSeconds = 1.0,
         .transform = {.position = {-std::numeric_limits<float>::max(), 0.0f, 0.0f}}},
    };
    ApplyAnimationClip(scene, extreme, 0.5);
    CHECK(std::isfinite(scene.GetNode(node).localTransform.position.x));
    CHECK(std::abs(scene.GetNode(node).localTransform.position.x) < 1.0f);

    return EXIT_SUCCESS;
}
