#include "RawIron/Core/ActionBindings.h"
#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Core/InputLabelFormat.h"
#include "RawIron/Core/ContentPresentation.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/FixedStepAccumulator.h"
#include "RawIron/Core/GameSimulationClock.h"
#include "RawIron/Runtime/LevelScopedSchedulers.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Core/FrameArena.h"
#include "RawIron/Core/InputPolling.h"
#include "RawIron/Core/MainLoop.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/RenderCommandStream.h"
#include "RawIron/Core/RenderRecorder.h"
#include "RawIron/Core/RenderSubmissionPlan.h"
#include "RawIron/Core/SpscRingBuffer.h"
#include "RawIron/Math/Angles.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/SpatialIndex.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/VulkanCommandList.h"
#include "RawIron/Scene/FbxLoader.h"
#include "RawIron/Scene/GltfLoader.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/LevelObjectRegistry.h"
#include "RawIron/Scene/RuntimeMeshFactory.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/SceneRenderSubmission.h"
#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneKit.h"
#include "RawIron/Scene/SceneEntityPhysics.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Trace/EntityPhysics.h"
#include "RawIron/Trace/LocomotionTuning.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/ObjectPhysics.h"
#include "RawIron/Trace/TraceScene.h"

#ifdef RAWIRON_WITH_DEV_INSPECTOR
#include "RawIron/DevInspector/DevelopmentInspector.h"
#endif

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <span>
#include <thread>
#include <vector>

#include "RawIron/Test/TestHarness.h"

namespace {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.01f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

bool Contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

std::filesystem::path FindWorkspaceRoot() {
    namespace fs = std::filesystem;

    fs::path current = fs::current_path();
    while (!current.empty()) {
        if (fs::exists(current / "CMakeLists.txt") &&
            fs::exists(current / "Source") &&
            fs::exists(current / "Assets")) {
            return current;
        }

        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    throw std::runtime_error("Could not locate the RawIron workspace root from the current test directory.");
}

bool ImageHasContrast(const ri::render::software::SoftwareImage& image) {
    if (image.pixels.empty()) {
        return false;
    }

    std::uint8_t minimum = 255;
    std::uint8_t maximum = 0;
    for (std::uint8_t value : image.pixels) {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    return static_cast<int>(maximum) - static_cast<int>(minimum) >= 18;
}

bool ImagesDiffer(const ri::render::software::SoftwareImage& lhs,
                  const ri::render::software::SoftwareImage& rhs,
                  int minimumDifferentChannels = 64) {
    if (lhs.width != rhs.width || lhs.height != rhs.height || lhs.pixels.size() != rhs.pixels.size()) {
        return true;
    }

    int different = 0;
    for (std::size_t index = 0; index < lhs.pixels.size(); ++index) {
        if (std::abs(static_cast<int>(lhs.pixels[index]) - static_cast<int>(rhs.pixels[index])) > 3) {
            ++different;
            if (different >= minimumDifferentChannels) {
                return true;
            }
        }
    }
    return false;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    Expect(NearlyEqual(actual.x, expected.x) &&
               NearlyEqual(actual.y, expected.y) &&
               NearlyEqual(actual.z, expected.z),
           label + " expected " + ri::math::ToString(expected) + " but got " + ri::math::ToString(actual));
}

void TestContentPresentation() {
    Expect(ri::core::ResolveKeyLabel("level_b_key") == "Level B Keycard",
           "ResolveKeyLabel should use authored keycard labels");
    Expect(ri::core::ResolveKeyLabel("unknown_door_id") == "Unknown Door Id",
           "ResolveKeyLabel should title-case unknown snake_case ids");

    const std::optional<ri::core::ItemPresentation> flashlight = ri::core::ResolveItemPresentation("flashlight");
    Expect(flashlight.has_value() && flashlight->name == "Flashlight" && flashlight->texture == "lite1.png"
               && flashlight->type == "tool",
           "ResolveItemPresentation should preserve flashlight authoring");

    ri::core::ItemPresentationOverrides customOverrides{};
    customOverrides.name = "Custom Name";
    const std::optional<ri::core::ItemPresentation> custom =
        ri::core::ResolveItemPresentation("orphan_pickup", customOverrides);
    Expect(custom.has_value() && custom->id == "orphan_pickup" && custom->name == "Custom Name"
               && custom->texture == "WARN_1A.png" && custom->type == "key",
           "ResolveItemPresentation should apply overrides to fallback rows");

    Expect(!ri::core::ResolveItemPresentation("").has_value(),
           "ResolveItemPresentation should reject empty ids");
    Expect(!ri::core::ResolveItemPresentation("   ").has_value(),
           "ResolveItemPresentation should reject whitespace-only ids");
    const std::optional<ri::core::ItemPresentation> trimmed =
        ri::core::ResolveItemPresentation("  flashlight  ");
    Expect(trimmed.has_value() && trimmed->id == "flashlight" && trimmed->name == "Flashlight",
           "ResolveItemPresentation should trim whitespace around item ids");
    Expect(ri::core::ResolveKeyLabel("  level_b_key\t") == "Level B Keycard",
           "ResolveKeyLabel should trim surrounding whitespace");
}

void TestCommandLineParsing() {
    std::array<char*, 9> args = {
        const_cast<char*>("RawIron.Core.Tests"),
        const_cast<char*>("--frames"),
        const_cast<char*>("12"),
        const_cast<char*>("--tick-hz=144"),
        const_cast<char*>("--headless"),
        const_cast<char*>("--root"),
        const_cast<char*>("N:\\RAWIRON"),
        const_cast<char*>("--bad-int=not-a-number"),
        const_cast<char*>("--orphan"),
    };

    const ri::core::CommandLine commandLine(static_cast<int>(args.size()), args.data());
    Expect(commandLine.HasFlag("--headless"), "CommandLine should detect present flags");
    Expect(!commandLine.HasFlag("--missing"), "CommandLine should not invent absent flags");
    Expect(commandLine.GetValue("--root").value_or("") == "N:\\RAWIRON", "CommandLine should read split option values");
    Expect(commandLine.TryGetInt("--frames").value_or(0) == 12, "CommandLine should expose parsed integer values");
    Expect(commandLine.TryGetInt("--tick-hz").value_or(0) == 144, "CommandLine should parse inline integer values");
    Expect(!commandLine.TryGetInt("--bad-int").has_value(), "CommandLine should reject malformed integers");
    Expect(!commandLine.TryGetInt("--orphan").has_value(), "CommandLine should reject missing option values as integers");
    Expect(commandLine.GetIntOr("--frames", 6) == 12, "CommandLine should parse integer values from separate args");
    Expect(commandLine.GetIntOr("--tick-hz", 30) == 144, "CommandLine should parse integer values from inline args");
    Expect(commandLine.GetIntOr("--bad-int", 30) == 30, "CommandLine should fall back on invalid integers");
    Expect(!commandLine.GetValue("--orphan").has_value(), "CommandLine should return nullopt for missing option values");
    Expect(commandLine.Args().size() == args.size(), "CommandLine should preserve the full argument list");

    const ri::core::CommandLine emptyNegativeArgc(-1, args.data());
    Expect(emptyNegativeArgc.Args().empty(), "CommandLine should treat negative argc as zero arguments");

    const ri::core::CommandLine nullArgvPositiveCount(3, nullptr);
    Expect(nullArgvPositiveCount.Args().empty(),
           "CommandLine should treat null argv as zero arguments even when argc is positive");

    std::array<char*, 4> argsWithNull = {
        const_cast<char*>("RawIron.Core.Tests"),
        nullptr,
        const_cast<char*>("--flag"),
        const_cast<char*>("--value=42"),
    };
    const ri::core::CommandLine commandLineWithNullEntry(static_cast<int>(argsWithNull.size()), argsWithNull.data());
    Expect(commandLineWithNullEntry.HasFlag("--flag"),
           "CommandLine should skip null argv entries instead of crashing");
    Expect(!commandLineWithNullEntry.GetValue("").has_value(),
           "CommandLine should reject empty option lookups");
}

void TestFrameArena() {
    ri::core::FrameArena arena(256U);
    Expect(arena.CapacityBytes() == 256U, "FrameArena should expose configured capacity");
    Expect(arena.UsedBytes() == 0U, "FrameArena should start empty");

    void* a = arena.Allocate(12U, 8U);
    Expect(a != nullptr, "FrameArena should allocate non-null memory for positive byte requests");
    Expect((reinterpret_cast<std::uintptr_t>(a) % 8U) == 0U, "FrameArena should honor alignment");
    Expect(arena.UsedBytes() >= 12U, "FrameArena should advance usage after allocation");

    const std::size_t mark = arena.Mark();
    auto* values = arena.AllocateArray<std::uint32_t>(8U);
    Expect(values != nullptr, "FrameArena should allocate typed arrays");
    for (std::size_t i = 0; i < 8U; ++i) {
        values[i] = static_cast<std::uint32_t>(i * 7U);
    }
    Expect(values[7] == 49U, "FrameArena typed allocations should be writable");

    arena.Rewind(mark);
    Expect(arena.UsedBytes() == mark, "FrameArena rewind should restore mark usage");

    arena.Reset();
    Expect(arena.UsedBytes() == 0U, "FrameArena reset should clear usage");

    bool threw = false;
    try {
        (void)arena.Allocate(300U);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    Expect(threw, "FrameArena should throw bad_alloc when capacity is exceeded");

    arena.Rewind(std::numeric_limits<std::size_t>::max());
    threw = false;
    try {
        (void)arena.Allocate(1U, 16U);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    Expect(threw, "FrameArena should throw bad_alloc when alignment math would overflow");
}

void TestSpscRingBuffer() {
    ri::core::SpscRingBuffer<int, 8> queue;
    Expect(queue.CapacityValue() == 8U, "SpscRingBuffer should report compile-time capacity");
    Expect(queue.Empty(), "SpscRingBuffer should start empty");
    Expect(!queue.Full(), "SpscRingBuffer should not start full");

    for (int value = 1; value <= 7; ++value) {
        Expect(queue.Push(value), "SpscRingBuffer should accept pushes until full");
    }
    Expect(queue.Full(), "SpscRingBuffer should report full when one slot remains for head/tail separation");
    Expect(!queue.Push(999), "SpscRingBuffer should reject pushes when full");

    const int* front = queue.Peek();
    Expect(front != nullptr && *front == 1, "SpscRingBuffer peek should return the oldest element");

    for (int expected = 1; expected <= 3; ++expected) {
        const std::optional<int> popped = queue.Pop();
        Expect(popped.has_value() && *popped == expected, "SpscRingBuffer should preserve FIFO ordering");
    }
    Expect(queue.Size() == 4U, "SpscRingBuffer size should track push/pop activity");

    Expect(queue.Push(8), "SpscRingBuffer should accept new values after pops");
    Expect(queue.Push(9), "SpscRingBuffer should wrap indices correctly");
    Expect(queue.Push(10), "SpscRingBuffer should continue operating after wrap");

    int expected = 4;
    while (!queue.Empty()) {
        const std::optional<int> popped = queue.Pop();
        Expect(popped.has_value(), "SpscRingBuffer pop should return value while queue is not empty");
        Expect(*popped == expected, "SpscRingBuffer should maintain FIFO through wrap-around");
        ++expected;
    }
    Expect(expected == 11, "SpscRingBuffer should output all queued elements");
    Expect(!queue.Pop().has_value(), "SpscRingBuffer pop should return nullopt when empty");

    queue.Push(42);
    queue.Clear();
    Expect(queue.Empty(), "SpscRingBuffer clear should reset queue state");
}

void TestSpscRingBufferConcurrent() {
    constexpr int kTotalValues = 20000;
    ri::core::SpscRingBuffer<int, 1024> queue;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::vector<int> observedValues(static_cast<std::size_t>(kTotalValues), 0);

    std::thread producer([&]() {
        for (int value = 1; value <= kTotalValues; ++value) {
            while (!queue.Push(value)) {
                std::this_thread::yield();
            }
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread consumer([&]() {
        int writeIndex = 0;
        while (writeIndex < kTotalValues) {
            const std::optional<int> value = queue.Pop();
            if (!value.has_value()) {
                std::this_thread::yield();
                continue;
            }
            observedValues[static_cast<std::size_t>(writeIndex)] = *value;
            ++writeIndex;
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    producer.join();
    consumer.join();

    Expect(produced.load(std::memory_order_relaxed) == kTotalValues,
           "Concurrent SPSC producer should enqueue every value");
    Expect(consumed.load(std::memory_order_relaxed) == kTotalValues,
           "Concurrent SPSC consumer should dequeue every value");
    for (int index = 0; index < kTotalValues; ++index) {
        Expect(observedValues[static_cast<std::size_t>(index)] == (index + 1),
               "Concurrent SPSC queue should preserve FIFO ordering");
    }
}

void TestInputPollingBuffer() {
    ri::core::InputPollingBuffer polling;
    Expect(!polling.FrameActive(), "Input polling should start with no active frame");
    Expect(polling.PendingSampleCount() == 0U, "Input polling should start with no samples");
    Expect(polling.PendingFrameCount() == 0U, "Input polling should start with no frame metadata");

    polling.BeginFrame(1000U);
    Expect(polling.FrameActive(), "Input polling should begin a frame");

    Expect(polling.PushSample(ri::core::InputSample{
               .timestampMicros = 1010U,
               .deviceId = 1U,
               .controlCode = 10U,
               .type = ri::core::InputSampleType::Digital,
               .value = 1.0f,
           }),
           "Input polling should accept first sample");
    Expect(polling.PushSample(ri::core::InputSample{
               .timestampMicros = 1020U,
               .deviceId = 1U,
               .controlCode = 20U,
               .type = ri::core::InputSampleType::Analog,
               .value = 0.5f,
           }),
           "Input polling should accept second sample");
    Expect(polling.EndFrame(1100U), "Input polling should end active frame");
    Expect(!polling.FrameActive(), "Input polling should clear active frame on end");

    const std::optional<ri::core::InputPollingFrame> frame = polling.PopFrame();
    Expect(frame.has_value(), "Input polling should produce frame metadata");
    Expect(frame->frameId == 1U, "Input polling frame IDs should start at one");
    Expect(frame->beginTimestampMicros == 1000U && frame->endTimestampMicros == 1100U,
           "Input polling should preserve frame timestamps");
    Expect(frame->sampleCount == 2U, "Input polling should track sample count per frame");

    polling.BeginFrame(6000U);
    Expect(polling.EndFrame(5500U), "Input polling should accept end timestamps before begin for clamping");
    const std::optional<ri::core::InputPollingFrame> clamped = polling.PopFrame();
    Expect(clamped.has_value() && clamped->beginTimestampMicros == 6000U && clamped->endTimestampMicros == 6000U,
           "Input polling should clamp end timestamps so they never precede begin");

    const std::optional<ri::core::InputSample> sampleA = polling.PopSample();
    const std::optional<ri::core::InputSample> sampleB = polling.PopSample();
    Expect(sampleA.has_value() && sampleA->controlCode == 10U, "Input polling should keep FIFO sample ordering");
    Expect(sampleB.has_value() && sampleB->controlCode == 20U, "Input polling should return second sample");
    Expect(!polling.PopSample().has_value(), "Input polling sample queue should empty correctly");

    for (std::size_t index = 0; index < ri::core::InputPollingBuffer::kSampleCapacity + 8U; ++index) {
        (void)polling.PushSample(ri::core::InputSample{
            .timestampMicros = 2000U + static_cast<std::uint64_t>(index),
            .controlCode = static_cast<std::uint16_t>(index & 0xFFFFU),
        });
    }
    Expect(polling.DroppedSampleCount() > 0U, "Input polling should track dropped samples when queue is saturated");

    polling.Reset();
    Expect(polling.PendingSampleCount() == 0U && polling.PendingFrameCount() == 0U,
           "Input polling reset should clear queued state");
    Expect(polling.DroppedSampleCount() == 0U && polling.DroppedFrameCount() == 0U,
           "Input polling reset should clear drop counters");
}

void TestActionBindings() {
    ri::core::ActionBindings bindings({
        {
            .actionId = "forward",
            .displayName = "Move Forward",
            .primaryInputId = "KeyW",
            .secondaryInputId = "ArrowUp",
        },
        {
            .actionId = "interact",
            .displayName = "Interact",
            .primaryInputId = "KeyE",
            .secondaryInputId = "MouseLeft",
        },
        {
            .actionId = "flashlight",
            .displayName = "Toggle Light",
            .primaryInputId = "KeyF",
        },
    });

    const ri::core::ActionBinding* interact = bindings.FindAction("interact");
    Expect(interact != nullptr && interact->primaryInputId == "KeyE",
           "Action bindings should preserve declared primary inputs");
    Expect(bindings.ResolveAction("MouseLeft").value_or("") == "interact",
           "Action bindings should resolve secondary inputs");

    Expect(bindings.Rebind("flashlight", "KeyE"),
           "Action bindings should allow rebinding to an occupied key");
    const ri::core::ActionBinding* flashlight = bindings.FindAction("flashlight");
    interact = bindings.FindAction("interact");
    Expect(flashlight != nullptr && flashlight->primaryInputId == "KeyE",
           "Action bindings should assign the new primary input to the requested action");
    Expect(interact != nullptr && interact->primaryInputId == "MouseLeft",
           "Action bindings should clear conflicting primary inputs by promoting a surviving secondary binding");
    Expect(interact->secondaryInputId.empty(),
           "Action bindings should clear promoted secondary bindings after conflict resolution");

    Expect(bindings.Rebind("interact", "Mouse2", ri::core::BindingSlot::Secondary),
           "Action bindings should accept alternate secondary inputs");
    interact = bindings.FindAction("interact");
    Expect(interact != nullptr && interact->secondaryInputId == "MouseRight",
           "Action bindings should normalize mouse aliases");

    Expect(bindings.ClearBinding("interact", ri::core::BindingSlot::Secondary),
           "Action bindings should clear secondary bindings");
    interact = bindings.FindAction("interact");
    Expect(interact != nullptr && interact->secondaryInputId.empty(),
           "Action bindings should persist cleared secondary bindings");

    Expect(ri::core::ActionBindings::FormatInputLabel("KeyQ") == "Q",
           "Action bindings should compact letter key labels");
    Expect(ri::core::ActionBindings::FormatInputLabel("ShiftRight") == "Shift",
           "Action bindings should normalize modifier labels");
    Expect(ri::core::ActionBindings::FormatInputLabel("mouse1") == "Mouse 1",
           "Action bindings should format mouse labels");
    Expect(ri::core::ActionBindings::FormatInputLabel("PageDown") == "Page Down",
           "Action bindings should format paging keys with readable spacing");
    Expect(ri::core::ActionBindings::FormatInputLabel("ArrowLeft") == "Left Arrow",
           "Action bindings should format arrow keys with directional labels");
    Expect(ri::core::KeyCodeToLabel("PageUp") == "Page Up",
           "Input label helpers should expose keycode-to-label compatibility wrappers");
}

void TestFixedStepAccumulator() {
    ri::core::FixedStepAccumulator accumulator(ri::core::FixedStepConfig{
        .fixedDeltaSeconds = 1.0 / 60.0,
        .maxCatchUpSteps = 4,
        .maxFrameDeltaSeconds = 0.10,
    });

    {
        const auto frame = accumulator.Advance(0.010);
        Expect(frame.stepCount == 0U, "Fixed-step accumulator should not step until enough time accumulates");
        Expect(frame.interpolationAlpha > 0.0, "Fixed-step accumulator should expose interpolation alpha");
    }
    {
        const auto frame = accumulator.Advance(0.010);
        Expect(frame.stepCount == 1U, "Fixed-step accumulator should emit one step around 60Hz");
        Expect(frame.stepDeltaSeconds > 0.0, "Fixed-step accumulator should expose fixed step delta");
    }
    {
        const auto frame = accumulator.Advance(1.0);
        Expect(frame.frameDeltaClamped, "Fixed-step accumulator should clamp oversized frame deltas");
        Expect(frame.stepCount <= 4U, "Fixed-step accumulator should cap catch-up step count");
    }
    {
        const double before = accumulator.SimulationTimeSeconds();
        const auto frame = accumulator.Advance(std::numeric_limits<double>::quiet_NaN());
        Expect(frame.stepCount == 0U, "Fixed-step accumulator should treat non-finite deltas as zero advance");
        Expect(before == accumulator.SimulationTimeSeconds(),
               "Fixed-step accumulator should not advance simulation time on non-finite deltas");
    }

    const double beforeReset = accumulator.SimulationTimeSeconds();
    Expect(beforeReset > 0.0, "Fixed-step accumulator should track simulation time");

    accumulator.Reset();
    Expect(NearlyEqual(static_cast<float>(accumulator.SimulationTimeSeconds()), 0.0f, 0.0001f),
           "Fixed-step accumulator reset should clear simulation time");
    Expect(NearlyEqual(static_cast<float>(accumulator.AccumulatorSeconds()), 0.0f, 0.0001f),
           "Fixed-step accumulator reset should clear accumulated remainder");

    {
        ri::core::FixedStepAccumulator fresh(ri::core::FixedStepConfig{});
        (void)fresh.Advance(0.5);
        fresh.Reset(std::numeric_limits<double>::quiet_NaN());
        Expect(fresh.SimulationTimeSeconds() == 0.0 && fresh.AccumulatorSeconds() == 0.0,
               "Fixed-step accumulator reset should coerce non-finite simulation times to zero");
    }
}

void TestGameSimulationClock() {
    ri::core::GameSimulationClock clock(ri::core::FixedStepConfig{
        .fixedDeltaSeconds = 1.0 / 60.0,
        .maxCatchUpSteps = 4,
        .maxFrameDeltaSeconds = 0.10,
    });

    const auto first = clock.Tick(0.0);
    Expect(first.realtimeSeconds == 0.0, "Game clock realtime should start at zero");
    Expect(first.realDeltaSeconds == 0.0, "Zero input delta should not advance realtime");
    Expect(first.fixed.stepCount == 0U, "Game clock should not step on zero real delta");

    const auto second = clock.Tick(1.0 / 60.0);
    Expect(second.realtimeSeconds > 0.0, "Game clock should accumulate wall time");
    Expect(second.realDeltaSeconds > 0.0, "Game clock should report measured frame delta");
    Expect(second.fixed.stepCount >= 1U, "Game clock should forward fixed steps from accumulator");

    const double realtimeBeforeNan = clock.RealtimeSeconds();
    const auto nanTick = clock.Tick(std::numeric_limits<double>::quiet_NaN());
    Expect(nanTick.realDeltaSeconds == 0.0, "Game clock should sanitize non-finite tick deltas");
    Expect(clock.RealtimeSeconds() == realtimeBeforeNan, "Game clock should not advance realtime on non-finite deltas");

    clock.Reset();
    Expect(clock.RealtimeSeconds() == 0.0, "Game clock reset should clear realtime");
    Expect(NearlyEqual(static_cast<float>(clock.SimulationTimeSeconds()), 0.0f, 0.0001f),
           "Game clock reset should clear simulation time");
}

void TestLevelScopedSchedulers() {
    ri::runtime::LevelScopedTimeoutScheduler timeouts;
    int fireCount = 0;
    (void)timeouts.ScheduleAfter(1.0, [&] { ++fireCount; }, 0.0);
    timeouts.Tick(0.5);
    Expect(fireCount == 0, "Timeout callback should not run before deadline");
    timeouts.Tick(1.0);
    Expect(fireCount == 1, "Timeout callback should run once at deadline");
    (void)timeouts.ScheduleAfter(10.0, [&] { ++fireCount; }, 1.0);
    timeouts.Clear();
    timeouts.Tick(100.0);
    Expect(fireCount == 1, "Clear should drop pending timeouts without firing");

    ri::runtime::LevelScopedIntervalScheduler intervals;
    int ticks = 0;
    (void)intervals.ScheduleEvery(1.0, [&] { ++ticks; }, 0.0);
    intervals.Tick(1.0, 1.0);
    Expect(ticks == 1, "Interval should fire after one period");
    intervals.Tick(2.0, 1.0);
    Expect(ticks == 2, "Interval should accumulate firings across ticks");
    intervals.SetPaused(true);
    intervals.Tick(100.0, 98.0);
    Expect(ticks == 2, "Paused interval scheduler should not invoke callbacks");

    ri::runtime::LevelScopedIntervalScheduler selfMutating;
    int cancelTicks = 0;
    std::uint64_t cancelToken = 0;
    cancelToken = selfMutating.ScheduleEvery(1.0, [&] {
        ++cancelTicks;
        selfMutating.Cancel(cancelToken);
    }, 0.0);
    selfMutating.Tick(1.0, 1.0);
    Expect(cancelTicks == 1, "Interval scheduler should tolerate self-cancel during callback execution");
    Expect(selfMutating.ActiveCount() == 0U, "Self-cancelled interval should be removed cleanly");

    ri::runtime::LevelScopedIntervalScheduler clearInsideCallback;
    int clearTicks = 0;
    (void)clearInsideCallback.ScheduleEvery(1.0, [&] {
        ++clearTicks;
        clearInsideCallback.Clear();
    }, 0.0);
    (void)clearInsideCallback.ScheduleEvery(1.0, [&] { clearTicks += 100; }, 0.0);
    clearInsideCallback.Tick(1.0, 1.0);
    Expect(clearTicks == 1, "Clearing inside an interval callback should prevent later staged callbacks from running");
}

void TestRuntimeCoreLifecycleAndHostAdapter() {
    struct RuntimeTrace {
        int startupCount = 0;
        int frameCount = 0;
        int pauseCount = 0;
        int resumeCount = 0;
        int shutdownCount = 0;
        int stopRequestedCount = 0;
        std::vector<std::string> phases;
    };

    class RecordingRuntimeModule final : public ri::runtime::RuntimeModule {
    public:
        explicit RecordingRuntimeModule(std::shared_ptr<RuntimeTrace> trace)
            : trace_(std::move(trace)) {}

        [[nodiscard]] std::string_view Name() const noexcept override { return "RecordingRuntimeModule"; }

        bool OnRuntimeStartup(ri::runtime::RuntimeContext& context,
                              const ri::core::CommandLine&) override {
            ++trace_->startupCount;
            context.Services().Register<RuntimeTrace>(trace_);
            return context.Phase() == ri::runtime::RuntimePhase::Loading;
        }

        bool OnRuntimeFrame(ri::runtime::RuntimeContext& context,
                            const ri::core::FrameContext& frame) override {
            ++trace_->frameCount;
            Expect(context.Frame().frameIndex == frame.frameIndex,
                   "RuntimeContext should expose the active frame snapshot before module ticks");
            return trace_->frameCount < 2;
        }

        void OnRuntimePause(ri::runtime::RuntimeContext&) override {
            ++trace_->pauseCount;
        }

        void OnRuntimeResume(ri::runtime::RuntimeContext&) override {
            ++trace_->resumeCount;
        }

        void OnRuntimeShutdown(ri::runtime::RuntimeContext&) override {
            ++trace_->shutdownCount;
        }

    private:
        std::shared_ptr<RuntimeTrace> trace_;
    };

    auto trace = std::make_shared<RuntimeTrace>();
    ri::runtime::RuntimeCore runtime(
        ri::runtime::RuntimeIdentity{
            .id = "runtime-test",
            .displayName = "Runtime Test",
            .mode = "test",
            .instanceId = {},
        },
        ri::runtime::RuntimePaths{
            .workspaceRoot = FindWorkspaceRoot(),
            .gameRoot = {},
            .saveRoot = {},
            .configRoot = {},
        });
    runtime.Context().Events().On("runtime.phase", [trace](const ri::runtime::RuntimeEvent& event) {
        const auto found = event.fields.find("to");
        if (found != event.fields.end()) {
            trace->phases.push_back(found->second);
        }
    });
    runtime.Context().Events().On("runtime.stop_requested", [trace](const ri::runtime::RuntimeEvent&) {
        ++trace->stopRequestedCount;
    });
    Expect(runtime.TryAddModule(std::make_unique<RecordingRuntimeModule>(trace)),
           "Runtime core should accept a unique named module");
    Expect(!runtime.TryAddModule(std::make_unique<RecordingRuntimeModule>(trace)),
           "Runtime core should reject duplicate module names");
    Expect(runtime.HasModule("RecordingRuntimeModule"), "Runtime core should expose mounted module lookup by name");
    const std::vector<std::string> moduleNames = runtime.ModuleNames();
    Expect(moduleNames.size() == 1U && moduleNames.front() == "RecordingRuntimeModule",
           "Runtime core should expose stable module names");

    ri::runtime::RuntimeHostAdapter host(runtime);
    char arg0[] = "rawiron-runtime-test";
    char* argv[] = {arg0};
    const ri::core::CommandLine commandLine(1, argv);

    ri::core::MainLoopOptions options{};
    options.maxFrames = 5;
    options.fixedDeltaSeconds = 1.0 / 60.0;
    options.paceToFixedDelta = false;

    const int exitCode = ri::core::RunMainLoop(host, commandLine, options);
    Expect(exitCode == 0, "Runtime host adapter should run through the core main loop");
    Expect(trace->startupCount == 1, "Runtime core should start modules once");
    Expect(trace->frameCount == 2, "Runtime module should be able to stop the host loop");
    Expect(trace->stopRequestedCount == 1, "Runtime core should emit a stop-requested event when a module stops");
    Expect(runtime.Context().StopReason() == "Runtime module requested stop: RecordingRuntimeModule",
           "Runtime context should retain the module stop reason");
    Expect(trace->shutdownCount == 1, "Runtime core should shut modules down once");
    Expect(runtime.Context().Phase() == ri::runtime::RuntimePhase::Stopped,
           "Runtime core should finish in the stopped phase");
    Expect(runtime.Context().Services().Resolve<RuntimeTrace>() == trace,
           "Runtime services should expose registered shared services by type");
    Expect(runtime.Context().Services().Contains<RuntimeTrace>(),
           "Runtime services should expose typed service presence checks");
    Expect(runtime.Context().Services().Unregister<RuntimeTrace>(),
           "Runtime services should remove registered services by type");
    Expect(!runtime.Context().Services().Contains<RuntimeTrace>(),
           "Runtime services should report removed services as absent");
    Expect(std::find(trace->phases.begin(), trace->phases.end(), "starting") != trace->phases.end() &&
               std::find(trace->phases.begin(), trace->phases.end(), "loading") != trace->phases.end() &&
               std::find(trace->phases.begin(), trace->phases.end(), "running") != trace->phases.end() &&
               std::find(trace->phases.begin(), trace->phases.end(), "stopped") != trace->phases.end(),
           "Runtime phase events should describe startup and shutdown transitions");

    auto pauseTrace = std::make_shared<RuntimeTrace>();
    ri::runtime::RuntimeCore pauseRuntime(
        ri::runtime::RuntimeIdentity{
            .id = "runtime-pause-test",
            .displayName = "Runtime Pause Test",
            .mode = "test",
            .instanceId = {},
        });
    pauseRuntime.AddModule(std::make_unique<RecordingRuntimeModule>(pauseTrace));
    Expect(pauseRuntime.Startup(commandLine), "Runtime core should start directly without the host adapter");
    Expect(pauseRuntime.Pause("unit-test"), "Runtime core should support explicit pause transitions");
    Expect(pauseRuntime.Context().Phase() == ri::runtime::RuntimePhase::Paused,
           "Runtime core should enter paused phase");
    Expect(pauseTrace->pauseCount == 1, "Runtime module should receive pause callbacks");
    Expect(pauseRuntime.Resume(), "Runtime core should support resume transitions");
    Expect(pauseRuntime.Context().Phase() == ri::runtime::RuntimePhase::Running,
           "Runtime core should return to running phase after resume");
    Expect(pauseTrace->resumeCount == 1, "Runtime module should receive resume callbacks");
    pauseRuntime.Shutdown();

    class LegacyHost final : public ri::core::Host {
    public:
        [[nodiscard]] std::string_view GetName() const noexcept override { return "LegacyHost"; }
        [[nodiscard]] std::string_view GetMode() const noexcept override { return "tool"; }

        void OnStartup(const ri::core::CommandLine&) override { ++startupCount; }
        [[nodiscard]] bool OnFrame(const ri::core::FrameContext&) override {
            ++frameCount;
            return frameCount < 3;
        }
        void OnShutdown() override { ++shutdownCount; }

        int startupCount = 0;
        int frameCount = 0;
        int shutdownCount = 0;
    };

    LegacyHost legacyHost;
    ri::runtime::RuntimeCore hostRuntime(
        ri::runtime::RuntimeIdentity{
            .id = "legacy-host-runtime",
            .displayName = "Legacy Host Runtime",
            .mode = "tool",
            .instanceId = {},
        });
    hostRuntime.AddModule(std::make_unique<ri::runtime::RuntimeHostModule>(legacyHost));
    ri::runtime::RuntimeHostAdapter legacyAdapter(hostRuntime);
    const int legacyExitCode = ri::core::RunMainLoop(legacyAdapter, commandLine, options);
    Expect(legacyExitCode == 0, "RuntimeHostModule should let legacy hosts run through RuntimeCore");
    Expect(legacyHost.startupCount == 1, "RuntimeHostModule should forward startup to the mounted host");
    Expect(legacyHost.frameCount == 3, "RuntimeHostModule should forward frames until the host stops");
    Expect(legacyHost.shutdownCount == 1, "RuntimeHostModule should forward shutdown to the mounted host");
}

void TestMainLoopSanitizesInvalidFixedDelta() {
    class RecordingHost final : public ri::core::Host {
    public:
        [[nodiscard]] std::string_view GetName() const noexcept override { return "RecordingHost"; }
        [[nodiscard]] std::string_view GetMode() const noexcept override { return "test"; }

        void OnStartup(const ri::core::CommandLine&) override { startupCount += 1; }

        [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
            frames.push_back(frame);
            return true;
        }

        void OnShutdown() override { shutdownCount += 1; }

        int startupCount = 0;
        int shutdownCount = 0;
        std::vector<ri::core::FrameContext> frames;
    };

    RecordingHost host;
    char arg0[] = "rawiron-core-tests";
    char* argv[] = {arg0};
    const ri::core::CommandLine commandLine(1, argv);

    ri::core::MainLoopOptions options{};
    options.maxFrames = 3;
    options.fixedDeltaSeconds = 0.0;
    options.paceToFixedDelta = false;

    const int exitCode = ri::core::RunMainLoop(host, commandLine, options);
    Expect(exitCode == 0, "Main loop should return success for bounded test runs");
    Expect(host.startupCount == 1, "Main loop should call host startup exactly once");
    Expect(host.shutdownCount == 1, "Main loop should call host shutdown exactly once");
    Expect(host.frames.size() == 3U, "Main loop should run the requested number of frames");

    for (std::size_t index = 0; index < host.frames.size(); ++index) {
        const ri::core::FrameContext& frame = host.frames[index];
        Expect(frame.deltaSeconds > 0.0, "Main loop should sanitize non-positive fixed delta values");
        Expect(NearlyEqual(static_cast<float>(frame.deltaSeconds), static_cast<float>(1.0 / 60.0), 0.000001f),
               "Main loop should fallback to 60Hz fixed delta");
        Expect(NearlyEqual(static_cast<float>(frame.elapsedSeconds),
                           static_cast<float>(frame.deltaSeconds * static_cast<double>(index)),
                           0.00001f),
               "Main loop elapsed seconds should remain aligned to frame index and fixed delta");
    }
}

void TestMainLoopSanitizesNonFiniteFixedDelta() {
    class RecordingHost final : public ri::core::Host {
    public:
        [[nodiscard]] std::string_view GetName() const noexcept override { return "RecordingHost"; }
        [[nodiscard]] std::string_view GetMode() const noexcept override { return "test"; }

        void OnStartup(const ri::core::CommandLine&) override {}
        [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
            frames.push_back(frame);
            return frames.size() < 2;
        }
        void OnShutdown() override {}

        std::vector<ri::core::FrameContext> frames;
    };

    RecordingHost host;
    char arg0[] = "rawiron-core-tests";
    char* argv[] = {arg0};
    const ri::core::CommandLine commandLine(1, argv);

    ri::core::MainLoopOptions options{};
    options.maxFrames = 0;
    options.fixedDeltaSeconds = std::numeric_limits<double>::quiet_NaN();
    options.paceToFixedDelta = false;

    (void)ri::core::RunMainLoop(host, commandLine, options);
    Expect(host.frames.size() == 2U, "Main loop should run until host stops");
    for (const ri::core::FrameContext& frame : host.frames) {
        Expect(std::isfinite(frame.deltaSeconds) && frame.deltaSeconds > 0.0,
               "Main loop should never expose non-finite or non-positive sim delta");
        Expect(std::isfinite(frame.realDeltaSeconds) && frame.realDeltaSeconds >= 0.0,
               "Main loop should never expose non-finite or negative realtime delta");
    }
}

void TestFixedStepAccumulatorSanitizesNonFiniteConfig() {
    ri::core::FixedStepAccumulator accumulator(ri::core::FixedStepConfig{
        .fixedDeltaSeconds = std::numeric_limits<double>::infinity(),
        .maxCatchUpSteps = 2,
        .maxFrameDeltaSeconds = std::numeric_limits<double>::quiet_NaN(),
    });
    Expect(NearlyEqual(static_cast<float>(accumulator.Advance(1.0 / 60.0).stepDeltaSeconds),
                       static_cast<float>(1.0 / 60.0), 0.000001f),
           "Fixed-step accumulator should replace non-finite config with stable defaults");
}

void TestRenderCommandStream() {
    ri::core::RenderCommandStream stream;
    stream.Reserve(512U);

    const ri::core::ClearColorCommand clear{
        .r = 0.1f,
        .g = 0.2f,
        .b = 0.3f,
        .a = 1.0f,
    };
    stream.EmitSorted(ri::core::RenderCommandType::ClearColor,
                      clear,
                      ri::core::PackRenderSortKey(/*pass*/ 0U, /*pipeline*/ 0U, /*material*/ 0U, /*depth*/ 0U));

    ri::core::SetViewProjectionCommand viewProjection{};
    viewProjection.viewProjection[0] = 2.0f;
    viewProjection.viewProjection[5] = 2.0f;
    viewProjection.viewProjection[10] = 0.5f;
    stream.EmitSorted(ri::core::RenderCommandType::SetViewProjection,
                      viewProjection,
                      ri::core::PackRenderSortKey(/*pass*/ 0U, /*pipeline*/ 1U, /*material*/ 0U, /*depth*/ 0U));

    ri::core::DrawMeshCommand draw{};
    draw.meshHandle = 4;
    draw.materialHandle = 2;
    draw.firstIndex = 12U;
    draw.indexCount = 36U;
    draw.instanceCount = 3U;
    draw.model[12] = 10.0f;
    draw.model[13] = 2.0f;
    draw.model[14] = -6.0f;
    stream.EmitSorted(ri::core::RenderCommandType::DrawMesh,
                      draw,
                      ri::core::PackRenderSortKey(/*pass*/ 1U, /*pipeline*/ 2U, /*material*/ 3U, /*depth*/ 40U));

    ri::core::DrawMeshCommand drawFront = draw;
    drawFront.meshHandle = 6;
    drawFront.materialHandle = 1;
    drawFront.model[12] = -2.0f;
    // Emit out-of-order key intentionally; sorted packet order should place this before the previous draw.
    stream.EmitSorted(ri::core::RenderCommandType::DrawMesh,
                      drawFront,
                      ri::core::PackRenderSortKey(/*pass*/ 1U, /*pipeline*/ 2U, /*material*/ 1U, /*depth*/ 10U));

    Expect(stream.CommandCount() == 4U, "Render command stream should track emitted packet count");
    Expect(stream.SizeBytes() > 0U, "Render command stream should store serialized bytes");

    ri::core::RenderCommandReader reader(stream.Bytes());
    ri::core::RenderCommandView view{};

    Expect(reader.Next(view), "Render command reader should decode first packet");
    Expect(view.header.type == ri::core::RenderCommandType::ClearColor, "First packet should be clear-color command");
    ri::core::ClearColorCommand readClear{};
    Expect(view.ReadPayload(readClear), "Clear-color packet payload should decode");
    Expect(NearlyEqual(readClear.r, 0.1f) && NearlyEqual(readClear.g, 0.2f) &&
               NearlyEqual(readClear.b, 0.3f) && NearlyEqual(readClear.a, 1.0f),
           "Clear-color payload values should round-trip");

    Expect(reader.Next(view), "Render command reader should decode second packet");
    Expect(view.header.type == ri::core::RenderCommandType::SetViewProjection,
           "Second packet should be set-view-projection command");
    ri::core::SetViewProjectionCommand readViewProjection{};
    Expect(view.ReadPayload(readViewProjection), "Set-view-projection payload should decode");
    Expect(NearlyEqual(readViewProjection.viewProjection[0], 2.0f) &&
               NearlyEqual(readViewProjection.viewProjection[10], 0.5f),
           "Set-view-projection payload values should round-trip");

    Expect(reader.Next(view), "Render command reader should decode third packet");
    Expect(view.header.type == ri::core::RenderCommandType::DrawMesh, "Third packet should be draw command");
    ri::core::DrawMeshCommand readDraw{};
    Expect(view.ReadPayload(readDraw), "Draw command payload should decode");
    Expect(readDraw.meshHandle == 4 && readDraw.materialHandle == 2 && readDraw.indexCount == 36U,
           "Draw command integer payload values should round-trip");
    Expect(NearlyEqual(readDraw.model[12], 10.0f) &&
               NearlyEqual(readDraw.model[13], 2.0f) &&
               NearlyEqual(readDraw.model[14], -6.0f),
           "Draw command transform payload should round-trip");

    Expect(reader.Next(view), "Render command reader should decode fourth packet");
    Expect(view.header.type == ri::core::RenderCommandType::DrawMesh, "Fourth packet should be draw command");
    ri::core::DrawMeshCommand readDrawSecond{};
    Expect(view.ReadPayload(readDrawSecond), "Second draw command payload should decode");
    Expect(readDrawSecond.meshHandle == 6 && readDrawSecond.materialHandle == 1,
           "Second draw command payload values should round-trip");

    Expect(!reader.Next(view), "Render command reader should stop after the final packet");
    Expect(reader.Exhausted(), "Render command reader should report exhausted stream after full decode");

    const std::vector<std::size_t> sortedOrder = stream.BuildSortedPacketOrder();
    Expect(sortedOrder.size() == 4U, "Render command stream should provide a sorted packet order for all commands");

    ri::core::RenderCommandView sortedView{};
    Expect(stream.ReadPacket(sortedOrder[0], sortedView), "Sorted packet read should decode first sorted command");
    Expect(sortedView.header.type == ri::core::RenderCommandType::ClearColor,
           "Sorted packet order should keep clear commands first by key");
    Expect(stream.ReadPacket(sortedOrder[1], sortedView), "Sorted packet read should decode second sorted command");
    Expect(sortedView.header.type == ri::core::RenderCommandType::SetViewProjection,
           "Sorted packet order should keep camera commands before draw commands by key");
    Expect(stream.ReadPacket(sortedOrder[2], sortedView), "Sorted packet read should decode third sorted command");
    ri::core::DrawMeshCommand sortedDrawA{};
    Expect(sortedView.ReadPayload(sortedDrawA), "Sorted draw payload should decode");
    Expect(sortedDrawA.materialHandle == 1,
           "Sorted packet order should prioritize lower material bucket key first");
    Expect(stream.ReadPacket(sortedOrder[3], sortedView), "Sorted packet read should decode fourth sorted command");
    ri::core::DrawMeshCommand sortedDrawB{};
    Expect(sortedView.ReadPayload(sortedDrawB), "Sorted draw payload should decode");
    Expect(sortedDrawB.materialHandle == 2,
           "Sorted packet order should place later material bucket key after earlier one");

    // Truncated packet streams must fail gracefully instead of reading beyond bounds.
    const std::span<const std::uint8_t> fullBytes = stream.Bytes();
    const std::span<const std::uint8_t> truncated(fullBytes.data(), fullBytes.size() - 2U);
    ri::core::RenderCommandReader truncatedReader(truncated);
    Expect(truncatedReader.Next(view), "Truncated stream should still decode complete leading packets");
    Expect(truncatedReader.Next(view), "Truncated stream should decode second complete packet");
    Expect(truncatedReader.Next(view), "Truncated stream should decode third complete packet");
    Expect(!truncatedReader.Next(view), "Truncated stream should reject incomplete trailing packet");
}

void TestRenderCommandReaderRejectsOversizedPayload() {
    ri::core::RenderCommandHeader bad{};
    bad.type = ri::core::RenderCommandType::ClearColor;
    bad.sizeBytes = 0xFFFF;
    bad.sortKey = 0;
    bad.sequence = 0;
    std::vector<std::uint8_t> bytes(sizeof(bad));
    std::memcpy(bytes.data(), &bad, sizeof(bad));
    ri::core::RenderCommandReader reader(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
    ri::core::RenderCommandView view{};
    Expect(!reader.Next(view), "Render command reader should reject declared payloads larger than the stream");
    Expect(reader.Exhausted(), "Render command reader should pin the cursor after a decode failure");
}

void TestJsonWriteTextFileRoundTrip() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "rawiron_jsonscan_write_roundtrip.txt";
    const std::string_view payload = "{\"ok\":true,\"n\":1}";
    Expect(ri::core::detail::WriteTextFile(path, payload), "WriteTextFile should succeed for a temp path");
    const std::string readBack = ri::core::detail::ReadTextFile(path);
    Expect(readBack == payload, "WriteTextFile persisted bytes should round-trip exactly");
    std::error_code ec;
    fs::remove(path, ec);
}

void TestReadTextFileMissingPathReturnsEmpty() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "rawiron_readtext_missing_no_such_file.txt";
    std::error_code ec;
    fs::remove(path, ec);
    Expect(ri::core::detail::ReadTextFile(path).empty(), "ReadTextFile should return empty for missing paths");
}

void TestRenderRecorderStopsOnSinkFailure() {
    class FailingSink final : public ri::core::RenderCommandSink {
    public:
        bool BeginBatch(const ri::core::RenderSubmissionBatch&) override { return true; }
        void EndBatch(const ri::core::RenderSubmissionBatch&) override {}

        bool RecordClearColor(const ri::core::ClearColorCommand&, const ri::core::RenderCommandHeader&) override {
            return true;
        }

        bool RecordSetViewProjection(const ri::core::SetViewProjectionCommand&,
                                     const ri::core::RenderCommandHeader&) override {
            return false;
        }

        bool RecordDrawMesh(const ri::core::DrawMeshCommand&, const ri::core::RenderCommandHeader&) override {
            return true;
        }

        bool RecordUnknown(const ri::core::RenderCommandView&) override { return false; }
    };

    ri::core::RenderCommandStream stream;
    stream.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{.r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 0U));
    stream.EmitSorted(
        ri::core::RenderCommandType::SetViewProjection,
        ri::core::SetViewProjectionCommand{},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 1U));

    const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(stream);
    FailingSink sink;
    ri::core::RenderRecorderStats stats{};
    const bool ok = ri::core::ExecuteRenderSubmissionPlan(stream, plan, sink, &stats);
    Expect(!ok, "Render recorder should fail when the sink rejects a command");
    Expect(stats.batchesVisited == 1U && stats.commandsVisited == 2U && stats.clearCommands == 1U
               && stats.setViewProjectionCommands == 0U,
           "Render recorder should flush partial stats when execution stops mid-batch");
}

void TestVulkanClearColorSanitizesNonFinite() {
    ri::render::vulkan::VulkanCommandListSink sink(false);
    const ri::core::RenderSubmissionBatch batch{.passIndex = 0, .pipelineBucket = 1, .firstOrderIndex = 0, .commandCount = 1};
    Expect(sink.BeginBatch(batch), "Vulkan command list sink should accept batch begin");
    const bool recorded = sink.RecordClearColor(
        ri::core::ClearColorCommand{
            .r = std::numeric_limits<float>::quiet_NaN(),
            .g = 0.25f,
            .b = std::numeric_limits<float>::infinity(),
            .a = 1.0f,
        },
        ri::core::RenderCommandHeader{});
    Expect(recorded, "Clear-color record should succeed");
    sink.EndBatch(batch);
    const std::vector<ri::render::vulkan::VulkanRenderOp>& ops = sink.Operations();
    Expect(ops.size() == 3U && ops[1].type == ri::render::vulkan::VulkanRenderOpType::ClearColor,
           "Clear-color op should be recorded between batch markers");
    Expect(NearlyEqual(ops[1].clearColor[0], 0.0f) && NearlyEqual(ops[1].clearColor[1], 0.25f)
               && NearlyEqual(ops[1].clearColor[2], 0.0f) && NearlyEqual(ops[1].clearColor[3], 1.0f),
           "Vulkan sink should coerce non-finite clear color channels to safe finite values");
}

void TestSpatialIndexMetrics() {
    ri::spatial::BspSpatialIndex index;
    index.Rebuild({
        ri::spatial::SpatialEntry{
            .id = "box_a",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-1.0f, -1.0f, -1.0f},
                .max = ri::math::Vec3{1.0f, 1.0f, 1.0f},
            },
        },
        ri::spatial::SpatialEntry{
            .id = "box_b",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{3.0f, -1.0f, -1.0f},
                .max = ri::math::Vec3{5.0f, 1.0f, 1.0f},
            },
        },
    });

    auto metrics = index.Metrics();
    Expect(metrics.rebuildCount >= 1U, "Spatial index should track rebuild count");
    Expect(metrics.lastRebuildEntryCount == 2U, "Spatial index should track last rebuild entry count");

    const std::vector<std::string> boxHits = index.QueryBox(ri::spatial::Aabb{
        .min = ri::math::Vec3{-2.0f, -2.0f, -2.0f},
        .max = ri::math::Vec3{2.0f, 2.0f, 2.0f},
    });
    Expect(boxHits.size() == 1U && boxHits.front() == "box_a", "Spatial index query box should return expected hit");

    const std::vector<std::string> rayHits = index.QueryRay(
        ri::math::Vec3{-10.0f, 0.0f, 0.0f},
        ri::math::Vec3{1.0f, 0.0f, 0.0f},
        20.0f);
    Expect(rayHits.size() == 2U, "Spatial index query ray should return both intersected entries");

    const std::size_t rayQueriesAfterValid = index.Metrics().rayQueries;
    const std::vector<std::string> nanOriginHits = index.QueryRay(
        ri::math::Vec3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
        ri::math::Vec3{1.0f, 0.0f, 0.0f},
        20.0f);
    Expect(nanOriginHits.empty(), "Spatial index query ray should return no hits for non-finite origins");
    Expect(index.Metrics().rayQueries == rayQueriesAfterValid + 1U,
           "Spatial index should count attempted ray queries including rejected non-finite rays");

    metrics = index.Metrics();
    Expect(metrics.boxQueries >= 1U, "Spatial index should count box queries");
    Expect(metrics.rayQueries >= 1U, "Spatial index should count ray queries");

    index.ResetMetrics();
    metrics = index.Metrics();
    Expect(metrics.boxQueries == 0U && metrics.rayQueries == 0U,
           "Spatial index metrics reset should clear query counters");
    Expect(metrics.rebuildCount >= 1U,
           "Spatial index metrics reset should keep rebuild counters for lifecycle tracking");
}

void TestRenderSubmissionPlan() {
    ri::core::RenderCommandStream stream;

    stream.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{.r = 0.05f, .g = 0.08f, .b = 0.1f, .a = 1.0f},
        ri::core::PackRenderSortKey(/*pass*/ 0U, /*pipeline*/ 1U, /*material*/ 0U, /*depth*/ 0U));

    ri::core::SetViewProjectionCommand view{};
    view.viewProjection[0] = 1.25f;
    stream.EmitSorted(
        ri::core::RenderCommandType::SetViewProjection,
        view,
        ri::core::PackRenderSortKey(/*pass*/ 0U, /*pipeline*/ 1U, /*material*/ 0U, /*depth*/ 1U));

    ri::core::DrawMeshCommand drawA{};
    drawA.meshHandle = 100;
    drawA.materialHandle = 3;
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        drawA,
        ri::core::PackRenderSortKey(/*pass*/ 1U, /*pipeline*/ 2U, /*material*/ 3U, /*depth*/ 40U));

    ri::core::DrawMeshCommand drawB = drawA;
    drawB.meshHandle = 101;
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        drawB,
        ri::core::PackRenderSortKey(/*pass*/ 1U, /*pipeline*/ 2U, /*material*/ 1U, /*depth*/ 10U));

    ri::core::DrawMeshCommand drawC = drawA;
    drawC.meshHandle = 102;
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        drawC,
        ri::core::PackRenderSortKey(/*pass*/ 1U, /*pipeline*/ 7U, /*material*/ 1U, /*depth*/ 2U));

    ri::core::DrawMeshCommand drawD = drawA;
    drawD.meshHandle = 103;
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        drawD,
        ri::core::PackRenderSortKey(/*pass*/ 2U, /*pipeline*/ 1U, /*material*/ 0U, /*depth*/ 0U));

    const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(stream);
    Expect(plan.packetOrder.size() == 6U, "Render submission plan should include every packet in sorted order");
    Expect(plan.batches.size() == 4U, "Render submission plan should batch by pass and pipeline");

    Expect(plan.batches[0].passIndex == 0U && plan.batches[0].pipelineBucket == 1U && plan.batches[0].commandCount == 2U,
           "First batch should contain pass-0 pipeline-1 commands");
    Expect(plan.batches[1].passIndex == 1U && plan.batches[1].pipelineBucket == 2U && plan.batches[1].commandCount == 2U,
           "Second batch should contain pass-1 pipeline-2 commands");
    Expect(plan.batches[2].passIndex == 1U && plan.batches[2].pipelineBucket == 7U && plan.batches[2].commandCount == 1U,
           "Third batch should contain pass-1 pipeline-7 commands");
    Expect(plan.batches[3].passIndex == 2U && plan.batches[3].pipelineBucket == 1U && plan.batches[3].commandCount == 1U,
           "Fourth batch should contain pass-2 pipeline-1 commands");

    ri::core::RenderCommandView viewPacket{};
    Expect(stream.ReadPacket(plan.packetOrder[plan.batches[1].firstOrderIndex], viewPacket),
           "Submission plan packet indices should map back to readable packets");
    ri::core::DrawMeshCommand firstPipeline2{};
    Expect(viewPacket.ReadPayload(firstPipeline2), "Submission plan should point at decodable draw payloads");
    Expect(firstPipeline2.meshHandle == 101,
           "Sorted submission plan should preserve expected per-key ordering inside batches");

    std::vector<int> iteratedMeshHandles;
    bool visited = ri::core::ForEachSubmissionBatchCommand(
        stream,
        plan,
        1U,
        [&](const ri::core::RenderCommandView& view, std::size_t orderIndex, std::size_t packetIndex) {
            (void)orderIndex;
            (void)packetIndex;
            ri::core::DrawMeshCommand draw{};
            if (!view.ReadPayload(draw)) {
                return false;
            }
            iteratedMeshHandles.push_back(draw.meshHandle);
            return true;
        });
    Expect(visited, "Batch iteration should succeed for valid submission-plan batch indices");
    Expect(iteratedMeshHandles.size() == 2U && iteratedMeshHandles[0] == 101 && iteratedMeshHandles[1] == 100,
           "Batch iteration should yield draw commands in sorted order for the target batch");

    std::size_t earlyStopCount = 0U;
    visited = ri::core::ForEachSubmissionBatchCommand(
        stream,
        plan,
        1U,
        [&](const ri::core::RenderCommandView&, std::size_t, std::size_t) {
            earlyStopCount += 1U;
            return false;
        });
    Expect(visited && earlyStopCount == 1U,
           "Batch iteration should allow early visitor-driven stop without failing");
    Expect(!ri::core::ForEachSubmissionBatchCommand(stream, plan, 99U, [](const ri::core::RenderCommandView&, std::size_t, std::size_t) {
                return true;
            }),
           "Batch iteration should fail for out-of-range submission-plan batch indices");

    ri::core::RenderSubmissionPlan overflowPlan{};
    overflowPlan.packetOrder = {0U};
    overflowPlan.batches.push_back(ri::core::RenderSubmissionBatch{
        .passIndex = 0U,
        .pipelineBucket = 0U,
        .firstOrderIndex = std::numeric_limits<std::size_t>::max(),
        .commandCount = 2U,
    });
    Expect(!ri::core::ForEachSubmissionBatchCommand(stream, overflowPlan, 0U, [](const ri::core::RenderCommandView&, std::size_t, std::size_t) {
                return true;
            }),
           "Batch iteration should reject overflowed firstOrderIndex/commandCount ranges");
}

void TestRenderRecorderExecution() {
    class CapturingSink final : public ri::core::RenderCommandSink {
    public:
        bool BeginBatch(const ri::core::RenderSubmissionBatch& batch) override {
            batchPasses.push_back(batch.passIndex);
            return true;
        }

        void EndBatch(const ri::core::RenderSubmissionBatch&) override {}

        bool RecordClearColor(const ri::core::ClearColorCommand&, const ri::core::RenderCommandHeader&) override {
            clearCount += 1U;
            operations.push_back("clear");
            return true;
        }

        bool RecordSetViewProjection(const ri::core::SetViewProjectionCommand&,
                                     const ri::core::RenderCommandHeader&) override {
            setViewProjectionCount += 1U;
            operations.push_back("set_view_projection");
            return true;
        }

        bool RecordDrawMesh(const ri::core::DrawMeshCommand& command, const ri::core::RenderCommandHeader&) override {
            drawHandles.push_back(command.meshHandle);
            operations.push_back("draw");
            return true;
        }

        bool RecordUnknown(const ri::core::RenderCommandView&) override {
            operations.push_back("unknown");
            return false;
        }

        std::vector<std::uint8_t> batchPasses;
        std::vector<std::string> operations;
        std::vector<int> drawHandles;
        std::size_t clearCount = 0;
        std::size_t setViewProjectionCount = 0;
    };

    ri::core::RenderCommandStream stream;
    stream.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{.r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 0U));
    stream.EmitSorted(
        ri::core::RenderCommandType::SetViewProjection,
        ri::core::SetViewProjectionCommand{},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 1U));
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        ri::core::DrawMeshCommand{.meshHandle = 22, .materialHandle = 7, .firstIndex = 0U, .indexCount = 36U, .instanceCount = 1U},
        ri::core::PackRenderSortKey(1U, 3U, 2U, 12U));
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        ri::core::DrawMeshCommand{.meshHandle = 18, .materialHandle = 5, .firstIndex = 0U, .indexCount = 36U, .instanceCount = 1U},
        ri::core::PackRenderSortKey(1U, 3U, 1U, 10U));

    const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(stream);
    CapturingSink sink;
    ri::core::RenderRecorderStats stats{};
    const bool ok = ri::core::ExecuteRenderSubmissionPlan(stream, plan, sink, &stats);
    Expect(ok, "Render recorder execution should succeed for valid command streams and sinks");
    Expect(stats.batchesVisited == 2U, "Render recorder should visit each submission batch");
    Expect(stats.commandsVisited == 4U, "Render recorder should visit all commands in submission order");
    Expect(stats.clearCommands == 1U && stats.setViewProjectionCommands == 1U && stats.drawCommands == 2U,
           "Render recorder stats should count command types");
    Expect(sink.batchPasses.size() == 2U && sink.batchPasses[0] == 0U && sink.batchPasses[1] == 1U,
           "Render recorder should report batch pass ordering to sink");
    Expect(sink.operations.size() == 4U && sink.operations[0] == "clear" && sink.operations[1] == "set_view_projection",
           "Render recorder should dispatch non-draw commands in sorted order");
    Expect(sink.drawHandles.size() == 2U && sink.drawHandles[0] == 18 && sink.drawHandles[1] == 22,
           "Render recorder should dispatch draw commands sorted by submission key");
}

void TestSceneRenderSubmission() {
    ri::scene::Scene scene("Submission");
    const int root = scene.CreateNode("Root");

    const int litMaterial = scene.AddMaterial(ri::scene::Material{
        .name = "LitMaterial",
        .shadingModel = ri::scene::ShadingModel::Lit,
        .baseColor = ri::math::Vec3{0.7f, 0.75f, 0.9f},
    });
    const int transparentMaterial = scene.AddMaterial(ri::scene::Material{
        .name = "Glass",
        .shadingModel = ri::scene::ShadingModel::Lit,
        .baseColor = ri::math::Vec3{0.4f, 0.7f, 0.9f},
        .opacity = 0.4f,
        .transparent = true,
    });

    const int cubeMesh = scene.AddMesh(ri::scene::Mesh{
        .name = "CubeMesh",
        .primitive = ri::scene::PrimitiveType::Cube,
        .vertexCount = 8,
        .indexCount = 36,
    });
    const int planeMesh = scene.AddMesh(ri::scene::Mesh{
        .name = "PlaneMesh",
        .primitive = ri::scene::PrimitiveType::Plane,
        .vertexCount = 4,
        .indexCount = 6,
    });

    const int litNode = scene.CreateNode("LitCube", root);
    scene.GetNode(litNode).localTransform.position = ri::math::Vec3{0.0f, 0.5f, 5.0f};
    scene.AttachMesh(litNode, cubeMesh, litMaterial);

    const int transparentNode = scene.CreateNode("GlassPanel", root);
    scene.GetNode(transparentNode).localTransform.position = ri::math::Vec3{0.0f, 0.0f, 8.0f};
    scene.AttachMesh(transparentNode, planeMesh, transparentMaterial);

    const int instanceBatch = scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "CrateBatch",
        .parent = root,
        .mesh = cubeMesh,
        .material = litMaterial,
        .transforms = {},
    });
    scene.AddMeshInstance(instanceBatch, ri::scene::Transform{
        .position = ri::math::Vec3{2.0f, 0.0f, 6.0f},
        .rotationDegrees = ri::math::Vec3{0.0f, 10.0f, 0.0f},
        .scale = ri::math::Vec3{0.5f, 0.5f, 0.5f},
    });
    scene.AddMeshInstance(instanceBatch, ri::scene::Transform{
        .position = ri::math::Vec3{-2.0f, 0.0f, 7.0f},
        .rotationDegrees = ri::math::Vec3{0.0f, -15.0f, 0.0f},
        .scale = ri::math::Vec3{0.6f, 0.6f, 0.6f},
    });

    const int hiddenNode = scene.CreateNode("BehindCamera", root);
    scene.GetNode(hiddenNode).localTransform.position = ri::math::Vec3{0.0f, 0.0f, -2.0f};
    scene.AttachMesh(hiddenNode, planeMesh, litMaterial);
    const int farOccludedNode = scene.CreateNode("FarOccluded", root);
    scene.GetNode(farOccludedNode).localTransform.position = ri::math::Vec3{0.0f, 0.5f, 20.0f};
    scene.AttachMesh(farOccludedNode, cubeMesh, litMaterial);

    const int cameraHandle = scene.AddCamera(ri::scene::Camera{
        .name = "MainCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 70.0f,
        .nearClip = 0.1f,
        .farClip = 250.0f,
    });
    const int cameraNode = scene.CreateNode("Camera", root);
    scene.GetNode(cameraNode).localTransform.position = ri::math::Vec3{0.0f, 1.0f, 0.0f};
    scene.AttachCamera(cameraNode, cameraHandle);

    const std::optional<int> resolvedCamera = ri::scene::ResolveSubmissionCameraNode(scene);
    Expect(resolvedCamera.has_value() && *resolvedCamera == cameraNode,
           "Scene render submission should resolve the authored camera node");

    const ri::math::Mat4 viewProjection = ri::scene::ComputeCameraViewProjection(scene, cameraNode, 16.0f / 9.0f);
    Expect(!NearlyEqual(viewProjection.m[0][0], 0.0f) && !NearlyEqual(viewProjection.m[1][1], 0.0f),
           "Scene render submission should build a non-degenerate view-projection matrix");

    ri::scene::SceneRenderSubmissionOptions options{};
    options.viewportWidth = 1600;
    options.viewportHeight = 900;
    options.clearColor = ri::math::Vec3{0.08f, 0.09f, 0.12f};
    const ri::scene::SceneRenderSubmission submission =
        ri::scene::BuildSceneRenderSubmission(scene, cameraNode, options);

    Expect(submission.stats.cameraNodeHandle == cameraNode,
           "Scene render submission should report the resolved camera handle");
    Expect(submission.stats.drawCommandCount == 5U,
           "Scene render submission should emit draw commands for visible nodes and mesh instances");
    Expect(submission.stats.skippedNodes >= 1U,
           "Scene render submission should report skipped nodes for non-renderable or clipped geometry");
    Expect(submission.commands.CommandCount() == 7U,
           "Scene render submission should emit clear, view, visible node draws, and visible instance draws");

    const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(submission.commands);
    Expect(plan.batches.size() == 3U,
           "Scene render submission should batch utility, lit, and transparent draws separately");

    ri::render::vulkan::VulkanCommandListSink sink;
    ri::core::RenderRecorderStats stats{};
    const bool executed = ri::core::ExecuteRenderSubmissionPlan(submission.commands, plan, sink, &stats);
    Expect(executed, "Scene render submission should execute through the Vulkan command sink");
    Expect(stats.clearCommands == 1U && stats.setViewProjectionCommands == 1U && stats.drawCommands == 5U,
           "Scene render submission should preserve command-type counts through recorder execution");

    const std::vector<ri::render::vulkan::VulkanRenderOp>& operations = sink.Operations();
    Expect(operations.size() == 13U,
           "Scene render submission should expand into begin/end markers and concrete Vulkan-side ops");
    Expect(operations[0].type == ri::render::vulkan::VulkanRenderOpType::BeginBatch
               && operations[1].type == ri::render::vulkan::VulkanRenderOpType::ClearColor
               && operations[2].type == ri::render::vulkan::VulkanRenderOpType::SetViewProjection,
           "Scene render submission should begin with utility batch operations");
    Expect(NearlyEqual(operations[2].viewProjection[0], viewProjection.m[0][0])
               && NearlyEqual(operations[2].viewProjection[5], viewProjection.m[1][1]),
           "Scene render submission should preserve view-projection payloads into Vulkan ops");
    Expect(operations[4].type == ri::render::vulkan::VulkanRenderOpType::BeginBatch
               && operations[5].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh
               && operations[5].meshHandle == cubeMesh,
           "Scene render submission should feed lit geometry into the Vulkan command list");
    Expect(NearlyEqual(operations[5].model[12], 0.0f) && NearlyEqual(operations[5].model[14], 5.0f),
           "Scene render submission should preserve node model transforms into Vulkan ops");
    Expect(operations[6].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh
               && operations[7].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh,
           "Scene render submission should include mesh-instance draws in the lit batch");
    Expect(NearlyEqual(operations[6].model[12], 2.0f) || NearlyEqual(operations[7].model[12], 2.0f),
           "Scene render submission should preserve mesh-instance transforms into Vulkan ops");
    Expect(operations[10].type == ri::render::vulkan::VulkanRenderOpType::BeginBatch
               && operations[11].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh
               && operations[11].meshHandle == planeMesh,
           "Scene render submission should route transparent geometry into its own batch");

    ri::scene::SceneRenderSubmissionOptions horizonOptions = options;
    horizonOptions.enableFarHorizon = true;
    horizonOptions.farHorizonStartDistance = 6.0f;
    horizonOptions.farHorizonEndDistance = 8.0f;
    horizonOptions.farHorizonMaxDistance = 200.0f;
    horizonOptions.farHorizonMaxNodeStride = 3U;
    horizonOptions.farHorizonMaxInstanceStride = 4U;
    horizonOptions.farHorizonCullTransparent = true;
    const ri::scene::SceneRenderSubmission horizonSubmission =
        ri::scene::BuildSceneRenderSubmission(scene, cameraNode, horizonOptions);
    Expect(horizonSubmission.stats.drawCommandCount == 2U,
           "Far horizon mode should keep near geometry while aggressively reducing distant draw submissions");
    Expect(horizonSubmission.commands.CommandCount() == 4U,
           "Far horizon mode should shrink command stream size when distance-thinning is enabled");
    const ri::core::RenderSubmissionPlan horizonPlan =
        ri::core::BuildRenderSubmissionPlan(horizonSubmission.commands);
    Expect(horizonPlan.batches.size() == 2U,
           "Far horizon mode should drop distant transparent batches when configured");

    ri::scene::SceneRenderSubmissionOptions occlusionOptions = options;
    occlusionOptions.enableCoarseOcclusion = true;
    occlusionOptions.coarseOcclusionGridWidth = 8;
    occlusionOptions.coarseOcclusionGridHeight = 6;
    occlusionOptions.coarseOcclusionDepthBias = 0.2f;
    const ri::scene::SceneRenderSubmission occlusionSubmission =
        ri::scene::BuildSceneRenderSubmission(scene, cameraNode, occlusionOptions);
    Expect(occlusionSubmission.stats.drawCommandCount <= submission.stats.drawCommandCount - 1U,
           "Coarse occlusion should skip at least one far opaque draw behind a near occluder");
}

void TestPhotoModeCamera() {
    using ri::scene::PhotoModeCameraOverrides;
    Expect(!ri::scene::PhotoModeFieldOfViewActive(PhotoModeCameraOverrides{}),
           "Default photo mode overrides should be inactive");
    Expect(!ri::scene::PhotoModeFieldOfViewActive(PhotoModeCameraOverrides{.enabled = true}),
           "Enabled photo mode without FOV delta should be inactive");
    Expect(ri::scene::PhotoModeFieldOfViewActive(
               PhotoModeCameraOverrides{.enabled = true, .fieldOfViewDegreesOverride = 48.0f}),
           "Positive FOV override should count as active photo mode");

    Expect(NearlyEqual(ri::scene::ResolvePhotoModeFieldOfViewDegrees(60.0f, PhotoModeCameraOverrides{}), 60.0f),
           "Disabled photo mode should preserve authored FOV");
    Expect(NearlyEqual(ri::scene::ResolvePhotoModeFieldOfViewDegrees(
                   60.0f,
                   PhotoModeCameraOverrides{.enabled = true, .fieldOfViewDegreesOverride = 48.0f}),
               48.0f),
           "Photo mode override should replace authored FOV");
    Expect(NearlyEqual(ri::scene::ResolvePhotoModeFieldOfViewDegrees(
                   40.0f,
                   PhotoModeCameraOverrides{.enabled = true, .fieldOfViewScale = 1.5f}),
               60.0f),
           "Photo mode scale should multiply authored FOV when no override is set");

    constexpr float kWideAspect = 16.0f / 9.0f;
    const float horizontalAsVertical = ri::scene::ResolvePhotoModeFieldOfViewDegrees(
        60.0f,
        PhotoModeCameraOverrides{.enabled = true,
                                 .fieldOfViewDegreesOverride = 90.0f,
                                 .fieldOfViewOverrideIsHorizontal = true},
        kWideAspect);
    Expect(NearlyEqual(horizontalAsVertical, 58.715f, 0.08f),
           "Horizontal FOV override should convert to vertical FOV using aspect ratio");

    ri::scene::Scene scene("PhotoMode");
    const int root = scene.CreateNode("Root");
    const int cameraHandle = scene.AddCamera(ri::scene::Camera{
        .name = "MainCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 60.0f,
        .nearClip = 0.1f,
        .farClip = 500.0f,
    });
    const int cameraNode = scene.CreateNode("Camera", root);
    scene.AttachCamera(cameraNode, cameraHandle);

    const float aspect = 16.0f / 9.0f;
    const ri::math::Mat4 baseline = ri::scene::ComputeCameraViewProjection(scene, cameraNode, aspect, nullptr);
    const PhotoModeCameraOverrides wide{.enabled = true, .fieldOfViewDegreesOverride = 100.0f};
    const ri::math::Mat4 widened = ri::scene::ComputeCameraViewProjection(scene, cameraNode, aspect, &wide);
    Expect(!NearlyEqual(baseline.m[1][1], widened.m[1][1]),
           "Photo mode should change vertical projection scale relative to baseline");
}

void TestCameraConfinementVolume() {
    ri::math::Mat4 inverse{};
    Expect(ri::math::TryInvertAffineMat4(ri::math::IdentityMatrix(), inverse),
           "Affine invert should succeed on identity");
    Expect(NearlyEqual(inverse.m[0][0], 1.0f) && NearlyEqual(inverse.m[1][1], 1.0f)
               && NearlyEqual(inverse.m[2][2], 1.0f) && NearlyEqual(inverse.m[3][3], 1.0f),
           "Inverse of identity should preserve the diagonal");

    ri::scene::Scene scene("Confine");
    const int root = scene.CreateNode("Root");
    const int volNode = scene.CreateNode("Volume", root);
    const int volHandle = scene.AddCameraConfinementVolume(ri::scene::CameraConfinementVolume{
        .name = "Reveal",
        .halfExtents = ri::math::Vec3{1.0f, 1.0f, 1.0f},
        .behavior = ri::scene::CameraConfinementBehavior::RegionClamp,
        .priority = 1,
    });
    scene.AttachCameraConfinementVolume(volNode, volHandle);

    Expect(ri::scene::CollectCameraConfinementVolumeNodes(scene).size() == 1U,
           "Should collect nodes carrying camera confinement volumes");
    Expect(ri::scene::IsWorldPointInsideCameraConfinementVolume(scene, volNode, ri::math::Vec3{0.5f, 0.5f, 0.5f}),
           "Interior sample should be inside the local axis-aligned box");
    Expect(!ri::scene::IsWorldPointInsideCameraConfinementVolume(scene, volNode, ri::math::Vec3{4.0f, 0.0f, 0.0f}),
           "Far exterior sample should be outside");

    scene.GetCameraConfinementVolume(volHandle).active = false;
    Expect(!ri::scene::IsWorldPointInsideCameraConfinementVolume(scene, volNode, ri::math::Vec3{0.0f, 0.0f, 0.0f}),
           "Inactive volumes should read as outside");
    scene.GetCameraConfinementVolume(volHandle).active = true;

    const int volNodeB = scene.CreateNode("VolumeB", root);
    const int volHandleB = scene.AddCameraConfinementVolume(ri::scene::CameraConfinementVolume{
        .name = "LargeHighPriority",
        .halfExtents = ri::math::Vec3{10.0f, 10.0f, 10.0f},
        .priority = 50,
    });
    scene.AttachCameraConfinementVolume(volNodeB, volHandleB);

    const std::optional<int> dominant =
        ri::scene::ResolveDominantCameraConfinementVolumeNodeAtWorldPoint(scene, ri::math::Vec3{0.0f, 0.0f, 0.0f});
    Expect(dominant.has_value() && *dominant == volNodeB,
           "Higher priority volume should win when both contain the sample");

    scene.GetNode(volNode).localTransform.rotationDegrees.y = 90.0f;
    Expect(ri::scene::IsWorldPointInsideCameraConfinementVolume(scene, volNode, ri::math::Vec3{1.0f, 0.0f, 0.0f}),
           "Rotated volume nodes should transform world samples into local box space");
}

void TestSceneHierarchy() {
    ri::scene::Scene scene("Hierarchy");
    const int root = scene.CreateNode("Root");
    const int child = scene.CreateNode("Child", root);
    const int grandChild = scene.CreateNode("GrandChild", child);

    scene.GetNode(root).localTransform.position = ri::math::Vec3{1.0f, 2.0f, 3.0f};
    scene.GetNode(child).localTransform.position = ri::math::Vec3{0.0f, 1.0f, 0.0f};
    scene.GetNode(grandChild).localTransform.position = ri::math::Vec3{2.0f, 0.0f, 0.0f};

    ExpectVec3(scene.ComputeWorldPosition(child), ri::math::Vec3{1.0f, 3.0f, 3.0f}, "Child world position");
    ExpectVec3(scene.ComputeWorldPosition(grandChild), ri::math::Vec3{3.0f, 3.0f, 3.0f}, "Grandchild world position");

    Expect(!scene.SetParent(root, grandChild), "Cycle prevention should reject parenting root under grandchild");
    Expect(scene.SetParent(grandChild, ri::scene::kInvalidHandle), "Grandchild should be re-parentable to the root");
    Expect(ri::scene::CollectRootNodes(scene).size() == 2U, "Re-parented scene should have two root nodes");
}

void TestSceneReparentingBookkeeping() {
    ri::scene::Scene scene("Reparenting");
    const int rootA = scene.CreateNode("RootA");
    const int rootB = scene.CreateNode("RootB");
    const int child = scene.CreateNode("Child", rootA);

    Expect(scene.GetNode(rootA).children.size() == 1U, "Initial parent should own the child");
    Expect(scene.SetParent(child, rootB), "Child should re-parent to another root");
    Expect(scene.GetNode(rootA).children.empty(), "Old parent should drop the child when it is re-parented");
    Expect(scene.GetNode(rootB).children.size() == 1U && scene.GetNode(rootB).children.front() == child,
           "New parent should receive the child");
    Expect(scene.SetParent(child, rootB), "Re-parenting to the same parent should remain a valid no-op");
    Expect(scene.GetNode(rootB).children.size() == 1U, "Re-parenting to the same parent should not duplicate child links");
}

void TestLevelObjectRegistryAndRuntimeMeshFactory() {
    ri::scene::LevelObjectRegistry registry{};
    registry.Register("door_a", 4);
    Expect(registry.Size() == 1U, "Registry should track one id");
    Expect(registry.TryResolveNode("door_a").value_or(-999) == 4, "Resolve should return registered handle");

    const std::optional<ri::scene::LevelObjectRegistry::WeakRef> weak = registry.CaptureWeak("door_a");
    Expect(weak.has_value() && !weak->id.empty() && weak->generation > 0U, "Weak capture should succeed");

    registry.Register("door_a", 7);
    Expect(registry.TryResolveNode("door_a").value_or(-999) == 7, "Replace should update handle");
    Expect(!registry.TryResolveWeak(*weak).has_value(), "Stale weak ref should fail after replace");

    const std::optional<ri::scene::LevelObjectRegistry::WeakRef> weak2 = registry.CaptureWeak("door_a");
    Expect(registry.TryResolveWeak(*weak2).value_or(-999) == 7, "Fresh weak ref should resolve");

    registry.Unregister("door_a");
    Expect(registry.Size() == 0U, "Unregister should clear id");
    Expect(!registry.TryResolveNode("door_a").has_value(), "Resolve should fail after unregister");

    ri::scene::Scene scene("RegistryRebuild");
    const int root = scene.CreateNode("root");
    const int a = scene.CreateNode("pickup_gem", root);
    const int b = scene.CreateNode("pickup_coin", root);
    (void)b;
    registry.RebuildFromNamedSceneNodes(scene);
    Expect(registry.TryResolveNode("pickup_gem").value_or(-999) == a, "Rebuild should map node names");

    registry.Register("extra", 99999);
    Expect(registry.PruneInvalidHandles(scene) >= 1U, "Prune should drop out-of-range handles");

    ri::scene::RuntimePrimitiveParams params{};
    params.nodeName = "FactoryCube";
    params.primitiveKind = "BoX";
    params.parent = root;
    params.registryId = "logic_cube";
    const int factoryNode = ri::scene::InstantiateRuntimePrimitive(scene, params, &registry);
    Expect(factoryNode != ri::scene::kInvalidHandle, "Factory should create a node");
    Expect(registry.TryResolveNode("logic_cube").value_or(-999) == factoryNode, "Factory should register id");

    Expect(ri::scene::ParsePrimitiveKindLoose("SPHERE") == ri::scene::PrimitiveType::Sphere, "Loose parse sphere");
    Expect(ri::scene::ParsePrimitiveKindLoose("unknown", ri::scene::PrimitiveType::Plane) == ri::scene::PrimitiveType::Plane,
           "Loose parse should fall back");
}

void TestHelperBuilders() {
    ri::scene::Scene scene("Helpers");
    const int root = scene.CreateNode("World");

    ri::scene::GridHelperOptions grid{};
    grid.parent = root;
    grid.size = -5.0f;
    const int gridNode = ri::scene::AddGridHelper(scene, grid);
    const ri::scene::Node& gridRef = scene.GetNode(gridNode);
    Expect(gridRef.mesh != ri::scene::kInvalidHandle, "Grid helper should attach a mesh");
    Expect(gridRef.material != ri::scene::kInvalidHandle, "Grid helper should attach a material");
    Expect(scene.GetNode(gridNode).localTransform.scale.x > 0.0f, "Grid helper size should be sanitized to positive");

    ri::scene::AxesHelperOptions axes{};
    axes.parent = root;
    axes.axisLength = -2.0f;
    axes.axisThickness = -0.1f;
    const ri::scene::AxesHelperHandles axisHandles = ri::scene::AddAxesHelper(scene, axes);
    Expect(axisHandles.root != ri::scene::kInvalidHandle, "Axes helper root should be created");
    Expect(scene.GetNode(axisHandles.xAxis).parent == axisHandles.root, "X axis should be parented to axes root");
    Expect(scene.GetNode(axisHandles.xAxis).localTransform.scale.x > 0.0f, "Axis length should be sanitized");
    Expect(scene.GetNode(axisHandles.yAxis).localTransform.scale.y > 0.0f, "Axis thickness should be sanitized");

    ri::scene::OrbitCameraOptions orbit{};
    orbit.parent = root;
    orbit.orbit.target = ri::math::Vec3{0.0f, 1.0f, 2.0f};
    orbit.orbit.distance = -4.0f;
    orbit.orbit.yawDegrees = 90.0f;
    orbit.orbit.pitchDegrees = -20.0f;
    const ri::scene::OrbitCameraHandles orbitHandles = ri::scene::AddOrbitCamera(scene, orbit);

    Expect(scene.GetNode(orbitHandles.cameraNode).camera != ri::scene::kInvalidHandle, "Orbit camera node should attach a camera");
    Expect(scene.GetNode(orbitHandles.cameraNode).localTransform.position.z > 0.0f, "Orbit camera distance should be sanitized");
    ExpectVec3(scene.GetNode(orbitHandles.root).localTransform.position, ri::math::Vec3{0.0f, 1.0f, 2.0f}, "Orbit target position");
}

void TestSceneDescriptionAndCounts() {
    ri::scene::Scene scene("Describe");
    const int root = scene.CreateNode("Root");
    const int child = scene.CreateNode("Child", root);

    const int material = scene.AddMaterial(ri::scene::Material{
        .name = "RootMaterial",
        .shadingModel = ri::scene::ShadingModel::Lit,
        .baseColor = ri::math::Vec3{0.7f, 0.7f, 0.7f},
    });
    const int mesh = scene.AddMesh(ri::scene::Mesh{
        .name = "RootMesh",
        .primitive = ri::scene::PrimitiveType::Cube,
        .vertexCount = 24,
        .indexCount = 36,
        .positions = {},
        .indices = {},
    });
    const int camera = scene.AddCamera(ri::scene::Camera{
        .name = "InspectCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 75.0f,
        .nearClip = 0.1f,
        .farClip = 200.0f,
    });
    const int light = scene.AddLight(ri::scene::Light{
        .name = "InspectLight",
        .type = ri::scene::LightType::Point,
        .color = ri::math::Vec3{1.0f, 1.0f, 1.0f},
        .intensity = 3.0f,
        .range = 8.0f,
    });

    scene.AttachMesh(root, mesh, material);
    scene.AttachCamera(child, camera);
    scene.AttachLight(child, light);
    const int instanceBatch = scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "Crates",
        .parent = root,
        .mesh = mesh,
        .material = material,
        .transforms = {},
    });
    scene.AddMeshInstance(instanceBatch, ri::scene::Transform{
        .position = ri::math::Vec3{-1.0f, 0.0f, 0.0f},
    });
    scene.AddMeshInstance(instanceBatch, ri::scene::Transform{
        .position = ri::math::Vec3{1.0f, 0.0f, 0.0f},
    });

    Expect(scene.MaterialCount() == 1U, "Scene should track material counts");
    Expect(scene.MeshCount() == 1U, "Scene should track mesh counts");
    Expect(scene.CameraCount() == 1U, "Scene should track camera counts");
    Expect(scene.LightCount() == 1U, "Scene should track light counts");
    Expect(scene.CameraConfinementVolumeCount() == 0U, "Scene should track camera confinement volume counts");
    Expect(scene.MeshInstanceBatchCount() == 1U, "Scene should track mesh-instance batch counts");
    Expect(scene.MeshInstanceCount() == 2U, "Scene should track mesh-instance transform counts");
    Expect(scene.GetMaterial(material).name == "RootMaterial", "Scene should return attached materials");
    Expect(scene.GetMesh(mesh).name == "RootMesh", "Scene should return attached meshes");
    Expect(scene.GetCamera(camera).name == "InspectCamera", "Scene should return attached cameras");
    Expect(scene.GetLight(light).name == "InspectLight", "Scene should return attached lights");

    const std::string description = scene.Describe();
    Expect(Contains(description, "Scene: Describe"), "Describe should include the scene name");
    Expect(
        Contains(description,
                 "Nodes=2 Meshes=1 Materials=1 Cameras=1 Lights=1 CameraConfinementVolumes=0"),
        "Describe should include scene resource counts");
    Expect(Contains(description, "- Root"), "Describe should include the root node");
    Expect(Contains(description, "mesh=RootMesh(cube) material=RootMaterial(lit)"),
           "Describe should include attached mesh and material details");
    Expect(Contains(description, "camera=InspectCamera(perspective) light=InspectLight(point)"),
           "Describe should include attached camera and light details");
}

void TestSceneMeshInstancePreviewRendering() {
    ri::scene::Scene scene("InstancePreview");
    const int root = scene.CreateNode("World");
    const int cameraNode = scene.CreateNode("Camera", root);
    const int lightNode = scene.CreateNode("Light", root);

    const int camera = scene.AddCamera(ri::scene::Camera{
        .name = "PreviewCam",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 65.0f,
        .nearClip = 0.1f,
        .farClip = 200.0f,
    });
    scene.AttachCamera(cameraNode, camera);
    scene.GetNode(cameraNode).localTransform.position = ri::math::Vec3{0.0f, 1.8f, -6.0f};

    const int light = scene.AddLight(ri::scene::Light{
        .name = "Sun",
        .type = ri::scene::LightType::Directional,
        .intensity = 2.8f,
    });
    scene.AttachLight(lightNode, light);
    scene.GetNode(lightNode).localTransform.rotationDegrees = ri::math::Vec3{-35.0f, 25.0f, 0.0f};

    const int mesh = scene.AddMesh(ri::scene::Mesh{
        .name = "InstancedCube",
        .primitive = ri::scene::PrimitiveType::Cube,
        .positions = {},
        .indices = {},
    });
    const int material = scene.AddMaterial(ri::scene::Material{
        .name = "InstancedCubeMat",
        .baseColor = ri::math::Vec3{0.42f, 0.68f, 0.92f},
    });
    const int batch = scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "CubeBatch",
        .parent = root,
        .mesh = mesh,
        .material = material,
        .transforms = {},
    });
    for (int index = 0; index < 9; ++index) {
        const int x = index % 3;
        const int z = index / 3;
        scene.AddMeshInstance(batch, ri::scene::Transform{
            .position = ri::math::Vec3{-1.6f + (static_cast<float>(x) * 1.6f), 0.0f, 3.5f + (static_cast<float>(z) * 1.6f)},
            .scale = ri::math::Vec3{0.6f, 0.6f, 0.6f},
        });
    }

    ri::render::software::ScenePreviewOptions options{};
    options.width = 256;
    options.height = 192;
    const ri::render::software::SoftwareImage image = ri::render::software::RenderScenePreview(scene, cameraNode, options);
    Expect(ImageHasContrast(image), "Software preview should render mesh-instance batches with visible contrast");
}

void TestSceneUtilities() {
    ri::scene::StarterScene starter = ri::scene::BuildStarterScene("Utilities");
    const ri::scene::Scene& scene = starter.scene;

    const std::optional<int> foundCrate = ri::scene::FindNodeByName(scene, "Crate");
    Expect(foundCrate.has_value(), "FindNodeByName should find crate");
    Expect(*foundCrate == starter.handles.crate, "FindNodeByName should return the crate handle");

    const std::string path = ri::scene::DescribeNodePath(scene, starter.handles.orbitCamera.cameraNode);
    Expect(path == "World/OrbitRig/OrbitSwivel/MainCamera", "Orbit camera path should be stable");
    Expect(ri::scene::DescribeNodePath(scene, starter.handles.root) == "World", "Root node path should resolve cleanly");
    Expect(!ri::scene::FindNodeByName(scene, "MissingNode").has_value(), "FindNodeByName should report absent nodes");

    Expect(ri::scene::CollectRootNodes(scene).size() == 1U, "Starter scene should have one root node");
    Expect(ri::scene::CollectRenderableNodes(scene).size() == 6U, "Starter scene should expose six renderable nodes");
    Expect(ri::scene::CollectCameraNodes(scene).size() == 1U, "Starter scene should expose one camera node");
    Expect(ri::scene::CollectLightNodes(scene).size() == 2U, "Starter scene should expose two light nodes");

    const std::vector<int> orbitSubtree = ri::scene::CollectNodeSubtree(scene, starter.handles.orbitCamera.root);
    Expect(orbitSubtree.size() == 3U, "Orbit subtree should include rig, swivel, and camera");
    Expect(orbitSubtree.front() == starter.handles.orbitCamera.root, "Subtree traversal should start with the requested root");
    Expect(orbitSubtree.back() == starter.handles.orbitCamera.cameraNode, "Orbit subtree should end at the camera node");

    const std::vector<int> orbitDescendants = ri::scene::CollectDescendantNodes(scene, starter.handles.orbitCamera.root);
    Expect(orbitDescendants.size() == 2U, "CollectDescendantNodes should exclude the requested root");
    Expect(orbitDescendants.front() == starter.handles.orbitCamera.swivel, "Orbit descendants should start with the swivel");
    Expect(orbitDescendants.back() == starter.handles.orbitCamera.cameraNode, "Orbit descendants should include the camera node");

    const std::optional<int> orbitRootAncestor = ri::scene::FindAncestorByName(
        scene,
        starter.handles.orbitCamera.cameraNode,
        "OrbitRig");
    Expect(orbitRootAncestor.has_value() && *orbitRootAncestor == starter.handles.orbitCamera.root,
           "FindAncestorByName should resolve the orbit rig from the camera node");
    Expect(!ri::scene::FindAncestorByName(scene, starter.handles.root, "OrbitRig").has_value(),
           "FindAncestorByName should return nullopt when no ancestor matches");
    Expect(ri::scene::CollectNodeSubtree(scene, ri::scene::kInvalidHandle).empty(),
           "CollectNodeSubtree should reject invalid handles");
}

void TestBoundsAndCameraUtilities() {
    ri::scene::Scene scene("Bounds");
    const int root = scene.CreateNode("Root");

    ri::scene::PrimitiveNodeOptions rotatedCube{};
    rotatedCube.nodeName = "RotatedCube";
    rotatedCube.parent = root;
    rotatedCube.primitive = ri::scene::PrimitiveType::Cube;
    rotatedCube.transform.rotationDegrees = ri::math::Vec3{0.0f, 90.0f, 0.0f};
    rotatedCube.transform.scale = ri::math::Vec3{2.0f, 1.0f, 4.0f};
    const int rotatedCubeHandle = ri::scene::AddPrimitiveNode(scene, rotatedCube);

    ri::scene::PrimitiveNodeOptions childSphere{};
    childSphere.nodeName = "ChildSphere";
    childSphere.parent = root;
    childSphere.primitive = ri::scene::PrimitiveType::Sphere;
    childSphere.transform.position = ri::math::Vec3{4.0f, 0.5f, 0.0f};
    childSphere.transform.scale = ri::math::Vec3{1.0f, 1.0f, 1.0f};
    const int childSphereHandle = ri::scene::AddPrimitiveNode(scene, childSphere);

    const std::optional<ri::scene::WorldBounds> cubeBounds = ri::scene::ComputeNodeWorldBounds(scene, rotatedCubeHandle, false);
    Expect(cubeBounds.has_value(), "ComputeNodeWorldBounds should return bounds for primitive nodes");
    ExpectVec3(cubeBounds->min, ri::math::Vec3{-2.0f, -0.5f, -1.0f}, "Rotated cube bounds min");
    ExpectVec3(cubeBounds->max, ri::math::Vec3{2.0f, 0.5f, 1.0f}, "Rotated cube bounds max");

    const std::optional<ri::scene::WorldBounds> rootBounds = ri::scene::ComputeNodeWorldBounds(scene, root);
    Expect(rootBounds.has_value(), "ComputeNodeWorldBounds should include child renderables when requested");
    ExpectVec3(rootBounds->min, ri::math::Vec3{-2.0f, -0.5f, -1.0f}, "Combined root bounds min");
    ExpectVec3(rootBounds->max, ri::math::Vec3{4.5f, 1.0f, 1.0f}, "Combined root bounds max");
    ExpectVec3(ri::scene::GetBoundsCenter(*rootBounds), ri::math::Vec3{1.25f, 0.25f, 0.0f}, "Combined root bounds center");

    const std::optional<ri::scene::WorldBounds> missingBounds = ri::scene::ComputeCombinedWorldBounds(scene, {ri::scene::kInvalidHandle});
    Expect(!missingBounds.has_value(), "ComputeCombinedWorldBounds should return nullopt when nothing contributes bounds");

    const std::optional<ri::scene::OrbitCameraState> orbitFromPosition = ri::scene::ComputeOrbitCameraStateFromPosition(
        ri::math::Vec3{0.0f, 1.0f, -5.0f},
        ri::math::Vec3{0.0f, 1.0f, 0.0f});
    Expect(orbitFromPosition.has_value(), "ComputeOrbitCameraStateFromPosition should resolve valid orbit state");
    Expect(NearlyEqual(orbitFromPosition->distance, 5.0f), "Orbit camera state should recover camera distance");
    Expect(NearlyEqual(orbitFromPosition->yawDegrees, 180.0f), "Orbit camera state should recover yaw from camera offset");
    Expect(NearlyEqual(orbitFromPosition->pitchDegrees, 0.0f), "Orbit camera state should recover pitch from camera offset");

    ri::scene::OrbitCameraOptions orbitOptions{};
    orbitOptions.parent = root;
    orbitOptions.camera = ri::scene::Camera{
        .name = "TestCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 70.0f,
        .nearClip = 0.1f,
        .farClip = 100.0f,
    };
    orbitOptions.orbit = ri::scene::OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 0.0f, 0.0f},
        .distance = 3.0f,
        .yawDegrees = 135.0f,
        .pitchDegrees = -15.0f,
    };
    ri::scene::OrbitCameraHandles orbitHandles = ri::scene::AddOrbitCamera(scene, orbitOptions);

    const std::optional<ri::scene::OrbitCameraState> framedState = ri::scene::ComputeFramedOrbitCameraState(
        scene,
        scene.GetCamera(orbitHandles.camera),
        {rotatedCubeHandle, childSphereHandle},
        orbitHandles.orbit,
        1.5f);
    Expect(framedState.has_value(), "ComputeFramedOrbitCameraState should frame valid node bounds");
    ExpectVec3(framedState->target, ri::math::Vec3{1.25f, 0.25f, 0.0f}, "Framed orbit target");
    Expect(framedState->distance > 3.0f, "Framed orbit state should move the camera back enough to fit the content");
    Expect(NearlyEqual(framedState->yawDegrees, 135.0f) && NearlyEqual(framedState->pitchDegrees, -15.0f),
           "Framed orbit state should preserve the requested viewing angle");

    Expect(ri::scene::FrameNodesWithOrbitCamera(scene, orbitHandles, {rotatedCubeHandle, childSphereHandle}, 1.5f),
           "FrameNodesWithOrbitCamera should apply a framed orbit state");
    ExpectVec3(scene.GetNode(orbitHandles.root).localTransform.position, framedState->target, "Applied framed orbit target");
    Expect(NearlyEqual(scene.GetNode(orbitHandles.cameraNode).localTransform.position.z, framedState->distance),
           "Applied framed orbit distance");
}

void TestRaycastUtilities() {
    ri::scene::StarterScene starter = ri::scene::BuildStarterScene("Raycast");

    const ri::scene::Ray crateRay{
        .origin = ri::math::Vec3{0.4f, 0.5f, -5.0f},
        .direction = ri::math::Vec3{0.0f, 0.0f, 1.0f},
    };
    const std::optional<ri::scene::RaycastHit> crateHit = ri::scene::RaycastSceneNearest(starter.scene, crateRay);
    Expect(crateHit.has_value(), "RaycastSceneNearest should hit the crate");
    Expect(crateHit->node == starter.handles.crate, "Nearest raycast hit should resolve to the crate node");
    ExpectVec3(crateHit->normal, ri::math::Vec3{0.0f, 0.0f, -1.0f}, "Crate hit normal");

    const ri::scene::Ray gridRay{
        .origin = ri::math::Vec3{4.0f, 2.0f, 0.0f},
        .direction = ri::math::Vec3{0.0f, -1.0f, 0.0f},
    };
    const std::optional<ri::scene::RaycastHit> gridHit = ri::scene::RaycastSceneNearest(starter.scene, gridRay);
    Expect(gridHit.has_value(), "RaycastSceneNearest should hit the grid");
    Expect(gridHit->node == starter.handles.grid, "Downward ray should resolve to the grid node");
    ExpectVec3(gridHit->position, ri::math::Vec3{4.0f, -0.5f, 0.0f}, "Grid hit position");

    const ri::scene::Ray missRay{
        .origin = ri::math::Vec3{50.0f, 50.0f, 50.0f},
        .direction = ri::math::Vec3{1.0f, 0.0f, 0.0f},
    };
    Expect(!ri::scene::RaycastSceneNearest(starter.scene, missRay).has_value(), "RaycastSceneNearest should return nullopt for misses");
    Expect(!ri::scene::RaycastNode(starter.scene, ri::scene::kInvalidHandle, crateRay).has_value(),
           "RaycastNode should reject invalid node handles");

    const std::vector<ri::scene::RaycastHit> hits = ri::scene::RaycastSceneAll(starter.scene, crateRay);
    Expect(hits.size() == 1U, "Forward crate ray should only hit one starter-scene primitive");
    Expect(hits.front().node == starter.handles.crate, "RaycastSceneAll should sort nearest hits first");
}

void TestStarterSceneAnimation() {
    ri::scene::StarterScene starter = ri::scene::BuildStarterScene("Animated");
    const ri::math::Vec3 before = starter.scene.ComputeWorldPosition(starter.handles.orbitCamera.cameraNode);
    ri::scene::AnimateStarterScene(starter, 1.0);
    const ri::math::Vec3 after = starter.scene.ComputeWorldPosition(starter.handles.orbitCamera.cameraNode);

    Expect(!NearlyEqual(before.x, after.x) || !NearlyEqual(before.y, after.y) || !NearlyEqual(before.z, after.z),
           "Starter scene animation should move the orbit camera");
}

void TestAnimationClipAndPlayer() {
    ri::scene::Scene scene("Anim");
    const int node = scene.CreateNode("Animated");
    scene.GetNode(node).localTransform.position = ri::math::Vec3{0.0f, 0.0f, 0.0f};

    ri::scene::AnimationClip clip{};
    clip.name = "MoveAndTurn";
    clip.durationSeconds = 2.0;
    clip.looping = true;
    clip.nodeTracks[node] = {
        ri::scene::TransformKeyframe{
            .timeSeconds = 0.0,
            .transform = ri::scene::Transform{
                .position = ri::math::Vec3{0.0f, 0.0f, 0.0f},
                .rotationDegrees = ri::math::Vec3{0.0f, 0.0f, 0.0f},
                .scale = ri::math::Vec3{1.0f, 1.0f, 1.0f},
            },
        },
        ri::scene::TransformKeyframe{
            .timeSeconds = 2.0,
            .transform = ri::scene::Transform{
                .position = ri::math::Vec3{2.0f, 0.0f, 0.0f},
                .rotationDegrees = ri::math::Vec3{0.0f, 180.0f, 0.0f},
                .scale = ri::math::Vec3{1.0f, 1.0f, 1.0f},
            },
        },
    };

    ri::scene::ApplyAnimationClip(scene, clip, 1.0);
    ExpectVec3(scene.GetNode(node).localTransform.position, ri::math::Vec3{1.0f, 0.0f, 0.0f}, "Animation clip midpoint position");
    Expect(NearlyEqual(scene.GetNode(node).localTransform.rotationDegrees.y, 90.0f),
           "Animation clip midpoint yaw should interpolate");

    ri::scene::AnimationPlayer player(&clip);
    player.Play(true);
    player.AdvanceSeconds(0.5);
    player.Apply(scene);
    ExpectVec3(scene.GetNode(node).localTransform.position, ri::math::Vec3{0.5f, 0.0f, 0.0f},
               "Animation player should advance and apply clip transforms");
    player.AdvanceSeconds(2.0);
    player.Apply(scene);
    Expect(scene.GetNode(node).localTransform.position.x < 1.0f,
           "Looping animation player should wrap time across clip duration");
}

std::vector<std::uint8_t> FloatTriangleBinary() {
    std::vector<std::uint8_t> bytes(44U);
    constexpr float vertices[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    std::memcpy(bytes.data(), vertices, sizeof(vertices));
    constexpr std::uint16_t indices[3] = {0, 1, 2};
    std::memcpy(bytes.data() + 36U, indices, sizeof(indices));
    return bytes;
}

void WriteGlbFile(const std::filesystem::path& path, const std::string& json, const std::vector<std::uint8_t>& bin) {
    std::vector<std::uint8_t> jsonChunk(json.begin(), json.end());
    while (jsonChunk.size() % 4U != 0U) {
        jsonChunk.push_back(static_cast<std::uint8_t>(' '));
    }
    std::vector<std::uint8_t> binChunk = bin;
    while (binChunk.size() % 4U != 0U) {
        binChunk.push_back(0);
    }

    const std::uint32_t totalLength =
        12U + 8U + static_cast<std::uint32_t>(jsonChunk.size()) + 8U + static_cast<std::uint32_t>(binChunk.size());

    std::ofstream stream(path, std::ios::binary);
    Expect(stream.is_open(), "Should open GLB path for writing");

    const auto writeU32 = [&stream](std::uint32_t value) {
        const auto writeByte = [&stream](std::uint8_t byte) {
            stream.put(static_cast<char>(byte));
        };
        writeByte(static_cast<std::uint8_t>(value & 0xffU));
        writeByte(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
        writeByte(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
        writeByte(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    };

    writeU32(0x46546C67U);
    writeU32(2U);
    writeU32(totalLength);
    writeU32(static_cast<std::uint32_t>(jsonChunk.size()));
    writeU32(0x4E4F534AU);
    stream.write(reinterpret_cast<const char*>(jsonChunk.data()), static_cast<std::streamsize>(jsonChunk.size()));
    writeU32(static_cast<std::uint32_t>(binChunk.size()));
    writeU32(0x004E4942U);
    stream.write(reinterpret_cast<const char*>(binChunk.data()), static_cast<std::streamsize>(binChunk.size()));
}

void TestGltfTriangleImport() {
    namespace fs = std::filesystem;
    const fs::path workspaceRoot = FindWorkspaceRoot();
    const fs::path scratchDir = workspaceRoot / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path gltfPath = scratchDir / "minimal_triangle.gltf";

    {
        std::ofstream stream(gltfPath, std::ios::binary);
        Expect(stream.is_open(), "Should open scratch glTF path for writing");
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"TriangleNode","mesh":0}],)"
            R"("meshes":[{"name":"TriMesh","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}]})";
    }

    ri::scene::Scene scene("GltfImport");
    std::string importError;
    const int rootHandle = ri::scene::ImportGltfToScene(scene,
                                                        gltfPath,
                                                        ri::scene::GltfImportOptions{
                                                            .parent = ri::scene::kInvalidHandle,
                                                            .wrapperNodeName = "GltfImport",
                                                            .sceneIndex = 0,
                                                        },
                                                        importError);
    Expect(rootHandle != ri::scene::kInvalidHandle, "glTF import should succeed: " + importError);

    const std::optional<int> triangleNode = ri::scene::FindNodeByName(scene, "TriangleNode");
    Expect(triangleNode.has_value(), "Imported scene should contain TriangleNode");
    Expect(scene.MeshCount() == 1U, "Imported glTF should create one mesh");
    const ri::scene::Mesh& mesh = scene.GetMesh(0);
    Expect(mesh.vertexCount == 3, "Triangle mesh should have three vertices");
    Expect(mesh.indexCount == 3, "Triangle mesh should have three indices");
    ExpectVec3(mesh.positions[0], ri::math::Vec3{0.0f, 0.0f, 0.0f}, "First vertex position");
    ExpectVec3(mesh.positions[1], ri::math::Vec3{1.0f, 0.0f, 0.0f}, "Second vertex position");
    ExpectVec3(mesh.positions[2], ri::math::Vec3{0.0f, 1.0f, 0.0f}, "Third vertex position");

    const std::string description = scene.Describe();
    Expect(Contains(description, "TriangleNode"), "Describe should include the imported node name");
    Expect(Contains(description, "GltfImport"), "Describe should include the wrapper node");
}

void TestGltfRejectsOutOfRangeIndices() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path gltfPath = scratchDir / "invalid_indices_triangle.gltf";

    {
        std::ofstream stream(gltfPath, std::ios::binary);
        Expect(stream.is_open(), "Should open invalid-index glTF path for writing");
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"BadTriangleNode","mesh":0}],)"
            R"("meshes":[{"name":"BadTriMesh","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAMABAAFAAAA"}]})";
    }

    ri::scene::Scene scene("InvalidIndexImport");
    std::string importError;
    const int rootHandle = ri::scene::ImportGltfToScene(
        scene,
        gltfPath,
        ri::scene::GltfImportOptions{
            .parent = ri::scene::kInvalidHandle,
            .wrapperNodeName = "InvalidIndexImport",
            .sceneIndex = 0,
        },
        importError);
    Expect(rootHandle == ri::scene::kInvalidHandle, "glTF import should fail on out-of-range indices");
    Expect(Contains(importError, "out of range"), "Import error should explain out-of-range index failure");
}

void TestGltfMinimalGlb() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path glbPath = scratchDir / "minimal_triangle.glb";

    const std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"GlbTri","mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"buffers":[{"byteLength":44}]})";
    WriteGlbFile(glbPath, json, FloatTriangleBinary());

    ri::scene::Scene scene("GlbImport");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(scene,
                                        glbPath,
                                        ri::scene::GltfImportOptions{.wrapperNodeName = "Root"},
                                        err) != ri::scene::kInvalidHandle,
           "GLB import should succeed: " + err);
    Expect(ri::scene::FindNodeByName(scene, "GlbTri").has_value(), "GLB scene should contain GlbTri node");
    Expect(scene.MeshCount() == 1U, "GLB should produce one mesh");
}

void TestGltfTwoScenesIndex() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Saved" / "TestScratch" / "two_scenes.gltf";
    fs::create_directories(path.parent_path());
    {
        std::ofstream stream(path, std::ios::binary);
        Expect(stream.is_open(), "Should open two-scene glTF for writing");
        stream << R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[0]},{"nodes":[1]}],"scene":0,"nodes":[{"name":"SceneA","mesh":0},{"name":"SceneB","mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}]})";
    }

    ri::scene::Scene scene("TwoScenes");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(scene,
                                        path,
                                        ri::scene::GltfImportOptions{.wrapperNodeName = "W", .sceneIndex = 1},
                                        err) != ri::scene::kInvalidHandle,
           err);
    Expect(!ri::scene::FindNodeByName(scene, "SceneA").has_value(), "Scene A root should not be imported");
    Expect(ri::scene::FindNodeByName(scene, "SceneB").has_value(), "Scene B root should be imported");
}

void TestGltfNormalizedUbytePositions() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Saved" / "TestScratch" / "ubyte_norm.gltf";
    fs::create_directories(path.parent_path());
    {
        std::ofstream stream(path, std::ios::binary);
        stream << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"NormTri","mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5121,"count":3,"type":"VEC3","normalized":true,"max":[1,1,0],"min":[0,0,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":9},{"buffer":0,"byteOffset":10,"byteLength":6}],"buffers":[{"byteLength":16,"uri":"data:application/octet-stream;base64,AAAA/wAAAP8AAAAAAQACAA=="}]})";
    }
    ri::scene::Scene scene("Norm");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(scene, path, ri::scene::GltfImportOptions{}, err) != ri::scene::kInvalidHandle, err);
    const ri::scene::Mesh& mesh = scene.GetMesh(0);
    ExpectVec3(mesh.positions[0], ri::math::Vec3{0.0f, 0.0f, 0.0f}, "ubyte norm vertex 0");
    ExpectVec3(mesh.positions[1], ri::math::Vec3{1.0f, 0.0f, 0.0f}, "ubyte norm vertex 1");
    ExpectVec3(mesh.positions[2], ri::math::Vec3{0.0f, 1.0f, 0.0f}, "ubyte norm vertex 2");
}

void TestGltfPerspectiveCameraImport() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Saved" / "TestScratch" / "cam.gltf";
    fs::create_directories(path.parent_path());
    {
        std::ofstream stream(path, std::ios::binary);
        stream << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"CamNode","camera":0}],"cameras":[{"type":"perspective","perspective":{"yfov":0.7853981633974483,"znear":0.05,"zfar":200.0}}]})";
    }
    ri::scene::Scene scene("CamGltf");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(
               scene,
               path,
               ri::scene::GltfImportOptions{.wrapperNodeName = "Wrap", .importCameras = true},
               err) != ri::scene::kInvalidHandle,
           err);
    Expect(scene.CameraCount() == 1U, "Should import one perspective camera");
    const ri::scene::Camera& camera = scene.GetCamera(0);
    Expect(NearlyEqual(camera.fieldOfViewDegrees, 45.0f), "yfov should convert to 45 degrees");
    Expect(NearlyEqual(camera.nearClip, 0.05f), "znear should be preserved");
    Expect(NearlyEqual(camera.farClip, 200.0f), "zfar should be preserved");
    Expect(camera.projection == ri::scene::ProjectionType::Perspective, "Camera should stay perspective");
}

void TestGltfMaterialFactorsImport() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Saved" / "TestScratch" / "material_factors.gltf";
    fs::create_directories(path.parent_path());
    {
        std::ofstream stream(path, std::ios::binary);
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"MaterialNode","mesh":0}],)"
            R"("meshes":[{"name":"MatMesh","primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)"
            R"("materials":[{)"
            R"("name":"FactorMaterial",)"
            R"("doubleSided":true,)"
            R"("emissiveFactor":[0.2,0.4,0.6],)"
            R"("alphaMode":"BLEND",)"
            R"("alphaCutoff":0.33,)"
            R"("pbrMetallicRoughness":{"baseColorFactor":[0.25,0.5,0.75,0.6],"metallicFactor":0.7,"roughnessFactor":0.2,)"
            R"("baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":1}},)"
            R"("normalTexture":{"index":2},"emissiveTexture":{"index":3},"occlusionTexture":{"index":4}}],)"
            R"("textures":[{"source":0},{"source":1},{"source":2},{"source":3},{"source":4}],)"
            R"("images":[)"
            R"({"uri":"textures/factor_basecolor.png"},)"
            R"({"uri":"textures/factor_orm.png"},)"
            R"({"uri":"textures/factor_normal.png"},)"
            R"({"uri":"textures/factor_emissive.png"},)"
            R"({"uri":"textures/factor_ao.png"}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}]})";
    }

    ri::scene::Scene scene("MaterialImport");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(scene, path, ri::scene::GltfImportOptions{}, err) != ri::scene::kInvalidHandle, err);
    Expect(scene.MaterialCount() == 1U, "Material import should create one material");
    const ri::scene::Material& material = scene.GetMaterial(0);
    Expect(material.name == "FactorMaterial", "Material import should preserve glTF material names");
    ExpectVec3(material.baseColor, ri::math::Vec3{0.25f, 0.5f, 0.75f}, "Imported base color");
    ExpectVec3(material.emissiveColor, ri::math::Vec3{0.2f, 0.4f, 0.6f}, "Imported emissive color");
    Expect(NearlyEqual(material.metallic, 0.7f), "Imported metallic factor should match glTF");
    Expect(NearlyEqual(material.roughness, 0.2f), "Imported roughness factor should match glTF");
    Expect(NearlyEqual(material.opacity, 0.6f), "Imported opacity should match baseColor alpha");
    Expect(NearlyEqual(material.alphaCutoff, 0.33f), "Imported alpha cutoff should match glTF");
    Expect(material.doubleSided, "Imported material should preserve double-sided flag");
    Expect(material.transparent, "BLEND alpha mode should set transparent materials");
    Expect(material.baseColorTexture == "textures/factor_basecolor.png",
           "Imported material should capture glTF base-color texture URI");
    Expect(material.ormTexture == "textures/factor_orm.png",
           "Imported material should capture glTF ORM texture URI");
    Expect(material.normalTexture == "textures/factor_normal.png",
           "Imported material should capture glTF normal texture URI");
    Expect(material.emissiveTexture == "textures/factor_emissive.png",
           "Imported material should capture glTF emissive texture URI");
    Expect(material.opacityTexture == "textures/factor_basecolor.png",
           "Transparent glTF materials should default opacity texture to base-color alpha");
    Expect(material.occlusionTexture == "textures/factor_ao.png",
           "Imported material should capture glTF occlusion texture URI");
}

void TestGltfSparsePositionImport() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path path = scratchDir / "sparse_positions.gltf";
    const fs::path binPath = scratchDir / "sparse_positions.bin";

    {
        std::vector<std::uint8_t> bytes(64, 0U);
        bytes[36] = 1U; bytes[37] = 0U;
        bytes[38] = 2U; bytes[39] = 0U;

        const auto writeF32 = [&bytes](std::size_t offset, float value) {
            std::uint8_t packed[4]{};
            std::memcpy(packed, &value, sizeof(float));
            for (std::size_t i = 0; i < 4; ++i) {
                bytes[offset + i] = packed[i];
            }
        };
        writeF32(40, 1.0f); writeF32(44, 0.0f); writeF32(48, 0.0f);
        writeF32(52, 0.0f); writeF32(56, 1.0f); writeF32(60, 0.0f);

        std::ofstream binStream(binPath, std::ios::binary);
        Expect(binStream.is_open(), "Should open sparse glTF bin path for writing");
        binStream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    {
        std::ofstream stream(path, std::ios::binary);
        Expect(stream.is_open(), "Should open sparse glTF path for writing");
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"SparseTri","mesh":0}],)"
            R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","sparse":{"count":2,"indices":{"bufferView":1,"byteOffset":0,"componentType":5123},"values":{"bufferView":2,"byteOffset":0}}},)"
            R"({"bufferView":3,"byteOffset":0,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},)"
            R"({"buffer":0,"byteOffset":36,"byteLength":4},)"
            R"({"buffer":0,"byteOffset":40,"byteLength":24},)"
            R"({"buffer":1,"byteOffset":0,"byteLength":6}],)"
            R"("buffers":[{"byteLength":64,"uri":"sparse_positions.bin"},{"byteLength":6,"uri":"data:application/octet-stream;base64,AAABAAIA"}]})";
    }

    ri::scene::Scene scene("Sparse");
    std::string err;
    Expect(ri::scene::ImportGltfToScene(scene, path, ri::scene::GltfImportOptions{}, err) != ri::scene::kInvalidHandle, err);
    const ri::scene::Mesh& mesh = scene.GetMesh(0);
    ExpectVec3(mesh.positions[0], ri::math::Vec3{0.0f, 0.0f, 0.0f}, "sparse vertex 0");
    ExpectVec3(mesh.positions[1], ri::math::Vec3{1.0f, 0.0f, 0.0f}, "sparse vertex 1");
    ExpectVec3(mesh.positions[2], ri::math::Vec3{0.0f, 1.0f, 0.0f}, "sparse vertex 2");
}

void TestAddGltfModelNodeAppliesTransform() {
    namespace fs = std::filesystem;
    const fs::path gltfPath = FindWorkspaceRoot() / "Saved" / "TestScratch" / "minimal_triangle.gltf";
    fs::create_directories(gltfPath.parent_path());
    if (!fs::exists(gltfPath)) {
        std::ofstream stream(gltfPath, std::ios::binary);
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"TriangleNode","mesh":0}],)"
            R"("meshes":[{"name":"TriMesh","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}]})";
    }

    ri::scene::Scene scene("ModelNode");
    std::string err;
    const int wrapper = ri::scene::AddGltfModelNode(
        scene,
        ri::scene::GltfModelOptions{
            .sourcePath = gltfPath,
            .wrapperNodeName = "PlaceIt",
            .transform =
                ri::scene::Transform{
                    .position = ri::math::Vec3{3.0f, -1.0f, 2.0f},
                },
        },
        &err);
    Expect(wrapper != ri::scene::kInvalidHandle, err);
    ExpectVec3(scene.ComputeWorldPosition(wrapper), ri::math::Vec3{3.0f, -1.0f, 2.0f}, "AddGltfModelNode wrapper world position");
}

void TestFbxImportScene() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Assets" / "Source" / "models" / "scenekit_cube_ascii.fbx";

    ri::scene::Scene scene("FbxImport");
    std::string err;
    Expect(ri::scene::ImportFbxToScene(scene,
                                       path,
                                       ri::scene::FbxImportOptions{.wrapperNodeName = "FbxImport"},
                                       err) != ri::scene::kInvalidHandle,
           "FBX import should succeed: " + err);
    Expect(scene.MeshCount() >= 1U, "FBX import should create at least one mesh");
    Expect(scene.MaterialCount() >= 1U, "FBX import should create at least one material");
    Expect(ri::scene::FindNodeByName(scene, "Cube").has_value(), "FBX scene should contain the Cube node");

    const ri::scene::Mesh& mesh = scene.GetMesh(0);
    Expect(mesh.vertexCount > 0, "FBX mesh should provide vertices");
    Expect(mesh.indexCount > 0, "FBX mesh should provide triangle indices");
    const std::string description = scene.Describe();
    Expect(Contains(description, "Cube"), "Describe should include imported FBX node names");
}

void TestPsxWaterPackageNativeSceneProof() {
    namespace fs = std::filesystem;
    const fs::path packageRoot = FindWorkspaceRoot() / "Assets" / "Packages" / "PSX_Water";
    const fs::path manifestPath = packageRoot / "package.ri_package.json";
    const fs::path texturePath = packageRoot / "T_PSX_Caustics_Atlas.png";
    const fs::path nativeModelPath = packageRoot / "models" / "psx_water_surface.ri_model.json";
    const fs::path materialPath = packageRoot / "materials" / "psx_water.ri_material.json";
    const fs::path scriptPath = packageRoot / "scripts" / "psx_water.riscript";

    const std::optional<ri::content::AssetPackageManifest> manifest =
        ri::content::LoadAssetPackageManifest(manifestPath);
    Expect(manifest.has_value(), "PSX water package manifest should load");
    Expect(ri::content::ValidateAssetPackageManifest(*manifest, packageRoot).valid,
           "PSX water package manifest should validate");
    Expect(manifest->assets.size() >= 4U, "PSX water package should list texture, model, material, and script assets");

    bool foundTexture = false;
    bool foundModel = false;
    bool foundMaterial = false;
    bool foundScript = false;
    for (const ri::content::AssetPackageEntry& asset : manifest->assets) {
        foundTexture = foundTexture || (asset.id == "t_psx_caustics_atlas_png" && asset.type == "texture");
        foundModel = foundModel || (asset.id == "psx_water_surface_model" && asset.type == "model");
        foundMaterial = foundMaterial || (asset.id == "psx_water_material" && asset.type == "material");
        foundScript = foundScript || (asset.id == "psx_water_script" && asset.type == "script");
    }
    Expect(foundTexture && foundModel && foundMaterial && foundScript,
           "PSX water package should expose every reconstructed RawIron asset as a manifest entry");
    Expect(fs::exists(texturePath), "PSX water package should include a portable caustics texture");

    const std::string materialJson = ri::core::detail::ReadTextFile(materialPath);
    const std::string scriptText = ri::core::detail::ReadTextFile(scriptPath);
    Expect(Contains(materialJson, "\"blendMode\": \"translucent\""), "Native material should preserve translucent blending");
    Expect(Contains(materialJson, "\"shadingModel\": \"unlit\""), "Native material should preserve unlit shading");
    Expect(Contains(materialJson, "\"frameCount\": 147"), "Native material should preserve flipbook frame count");
    Expect(Contains(ri::core::detail::ReadTextFile(nativeModelPath), "\"primitive\": \"custom\""),
           "Native model should provide a reconstructed custom water mesh");
    Expect(Contains(ri::core::detail::ReadTextFile(nativeModelPath), "\"uv\""),
           "Native model should provide reconstructed UVs for atlas sampling");
    Expect(Contains(scriptText, "set_material_texture_frame"),
           "Native script should drive the flipbook atlas frame");

    ri::scene::Scene scene("PSXWaterEmptyScene");
    const int root = scene.CreateNode("World");
    const int cameraNode = scene.CreateNode("Camera", root);
    const int lightNode = scene.CreateNode("FillLight", root);
    const int camera = scene.AddCamera(ri::scene::Camera{
        .name = "ProofCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 55.0f,
        .nearClip = 0.1f,
        .farClip = 100.0f,
    });
    scene.AttachCamera(cameraNode, camera);
    scene.GetNode(cameraNode).localTransform.position = ri::math::Vec3{0.0f, 1.0f, -3.5f};

    const int light = scene.AddLight(ri::scene::Light{
        .name = "SoftFill",
        .type = ri::scene::LightType::Directional,
        .intensity = 1.5f,
    });
    scene.AttachLight(lightNode, light);
    scene.GetNode(lightNode).localTransform.rotationDegrees = ri::math::Vec3{-50.0f, 15.0f, 0.0f};

    const int waterMaterial = scene.AddMaterial(ri::scene::Material{
        .name = "psx_water",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.72f, 0.92f, 1.0f},
        .baseColorTexture = "T_PSX_Caustics_Atlas.png",
        .baseColorTextureAtlasColumns = 7,
        .baseColorTextureAtlasRows = 21,
        .baseColorTextureAtlasFrameCount = 147,
        .baseColorTextureAtlasFramesPerSecond = 12.0f,
        .emissiveColor = ri::math::Vec3{0.18f, 0.32f, 0.42f},
        .opacity = 0.88f,
        .doubleSided = true,
        .transparent = true,
        .emissiveTexture = "T_PSX_Caustics_Atlas.png",
        .opacityTexture = "T_PSX_Caustics_Atlas.png",
    });

    const int nativeWaterMesh = scene.AddMesh(ri::scene::Mesh{
        .name = "psx_water_surface",
        .primitive = ri::scene::PrimitiveType::Custom,
        .vertexCount = 4,
        .indexCount = 6,
        .positions =
            {
                ri::math::Vec3{-1.0f, -1.0f, 0.0f},
                ri::math::Vec3{1.0f, -1.0f, 0.0f},
                ri::math::Vec3{1.0f, 1.0f, 0.0f},
                ri::math::Vec3{-1.0f, 1.0f, 0.0f},
            },
        .texCoords =
            {
                ri::math::Vec2{0.0f, 1.0f},
                ri::math::Vec2{1.0f, 1.0f},
                ri::math::Vec2{1.0f, 0.0f},
                ri::math::Vec2{0.0f, 0.0f},
            },
        .indices = {0, 1, 2, 0, 2, 3},
    });
    const int nativeWaterNode = scene.CreateNode("PSXWaterNativeSurface", root);
    scene.GetNode(nativeWaterNode).localTransform.position = ri::math::Vec3{0.0f, 0.0f, 3.6f};
    scene.GetNode(nativeWaterNode).localTransform.scale = ri::math::Vec3{2.5f, 2.5f, 2.5f};
    scene.AttachMesh(nativeWaterNode, nativeWaterMesh, waterMaterial);

    Expect(scene.MeshCount() == 1U, "PSX water scene should contain the reconstructed native surface model");

    ri::render::software::ScenePreviewOptions options{};
    options.width = 320;
    options.height = 240;
    options.textureRoot = packageRoot;
    options.clearTop = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.clearBottom = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.fogColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.pointSampleTextures = true;
    options.orderedDither = false;
    options.animationTimeSeconds = 0.0;
    const ri::render::software::SoftwareImage firstFrame =
        ri::render::software::RenderScenePreview(scene, cameraNode, options);
    Expect(ImageHasContrast(firstFrame), "PSX water proof scene should render visible water");

    options.animationTimeSeconds = 1.0;
    const ri::render::software::SoftwareImage secondFrame =
        ri::render::software::RenderScenePreview(scene, cameraNode, options);
    Expect(ImagesDiffer(firstFrame, secondFrame),
           "PSX water proof scene should advance atlas frames through native material animation");

    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    Expect(ri::render::software::SaveBmp(firstFrame, (scratchDir / "psx_water_native_scene_frame0.bmp").string()),
           "PSX water proof should write a first-frame BMP preview");
    Expect(ri::render::software::SaveBmp(secondFrame, (scratchDir / "psx_water_native_scene_frame1.bmp").string()),
           "PSX water proof should write an animated-frame BMP preview");
}

void TestAddFbxModelNodeAppliesTransform() {
    namespace fs = std::filesystem;
    const fs::path path = FindWorkspaceRoot() / "Assets" / "Source" / "models" / "scenekit_cube_ascii.fbx";

    ri::scene::Scene scene("FbxModelNode");
    std::string err;
    const int wrapper = ri::scene::AddFbxModelNode(
        scene,
        ri::scene::FbxModelOptions{
            .sourcePath = path,
            .wrapperNodeName = "PlaceFbx",
            .transform =
                ri::scene::Transform{
                    .position = ri::math::Vec3{4.0f, 1.5f, -2.0f},
                },
        },
        &err);
    Expect(wrapper != ri::scene::kInvalidHandle, "AddFbxModelNode should succeed: " + err);
    ExpectVec3(scene.ComputeWorldPosition(wrapper),
               ri::math::Vec3{4.0f, 1.5f, -2.0f},
               "AddFbxModelNode wrapper world position");
}

void TestWavefrontObjMeshUsesLibraryImporter() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path objPath = scratchDir / "library_obj.obj";
    const fs::path mtlPath = scratchDir / "library_obj.mtl";

    {
        std::ofstream mtl(mtlPath, std::ios::binary);
        Expect(mtl.is_open(), "Should open OBJ material library for writing");
        mtl << "newmtl Red\nKd 1.0 0.0 0.0\n\nnewmtl Blue\nKd 0.0 0.0 1.0\n";
    }
    {
        std::ofstream obj(objPath, std::ios::binary);
        Expect(obj.is_open(), "Should open OBJ path for writing");
        obj << "mtllib library_obj.mtl\n"
               "o First\n"
               "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "usemtl Red\n"
               "f 1 2 3\n"
               "o Second\n"
               "v 2 0 0\n"
               "v 3 0 0\n"
               "v 2 1 0\n"
               "usemtl Blue\n"
               "f 4 5 6\n";
    }

    std::string err;
    const std::optional<ri::scene::Mesh> mesh = ri::scene::LoadWavefrontObjMesh(objPath, err);
    Expect(mesh.has_value(), "OBJ import should succeed through ufbx: " + err);
    Expect(mesh->vertexCount == 6, "OBJ import should flatten both OBJ objects into one mesh");
    Expect(mesh->indexCount == 6, "OBJ import should triangulate both faces");
    ExpectVec3(mesh->positions[0], ri::math::Vec3{0.0f, 0.0f, 0.0f}, "OBJ vertex 0");
    ExpectVec3(mesh->positions[3], ri::math::Vec3{2.0f, 0.0f, 0.0f}, "OBJ vertex 3");
}

void TestAddWavefrontObjNodePreservesCustomMaterialOptions() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);
    const fs::path objPath = scratchDir / "node_obj.obj";

    {
        std::ofstream obj(objPath, std::ios::binary);
        Expect(obj.is_open(), "Should open OBJ path for node test");
        obj << "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "f 1 2 3\n";
    }

    ri::scene::Scene scene("ObjModelNode");
    std::string err;
    const int node = ri::scene::AddWavefrontObjNode(
        scene,
        ri::scene::ModelNodeOptions{
            .sourcePath = objPath,
            .nodeName = "ObjNode",
            .transform =
                ri::scene::Transform{
                    .position = ri::math::Vec3{-2.0f, 0.5f, 7.0f},
                },
            .materialName = "ObjCustomMaterial",
            .shadingModel = ri::scene::ShadingModel::Unlit,
            .baseColor = ri::math::Vec3{0.2f, 0.7f, 0.4f},
            .transparent = true,
        },
        &err);
    Expect(node != ri::scene::kInvalidHandle, "AddWavefrontObjNode should succeed: " + err);
    ExpectVec3(scene.ComputeWorldPosition(node),
               ri::math::Vec3{-2.0f, 0.5f, 7.0f},
               "AddWavefrontObjNode world position");
    Expect(scene.MaterialCount() == 1U, "AddWavefrontObjNode should still author a single custom material");
    const ri::scene::Material& material = scene.GetMaterial(0);
    Expect(material.name == "ObjCustomMaterial", "OBJ node should preserve custom material name");
    Expect(material.shadingModel == ri::scene::ShadingModel::Unlit, "OBJ node should preserve custom shading model");
    ExpectVec3(material.baseColor, ri::math::Vec3{0.2f, 0.7f, 0.4f}, "OBJ node should preserve custom base color");
    Expect(material.transparent, "OBJ node should preserve custom transparency");
}

void TestAddModelNodeDispatchesByExtension() {
    namespace fs = std::filesystem;
    const fs::path scratchDir = FindWorkspaceRoot() / "Saved" / "TestScratch";
    fs::create_directories(scratchDir);

    const fs::path objPath = scratchDir / "dispatch_obj.obj";
    {
        std::ofstream obj(objPath, std::ios::binary);
        Expect(obj.is_open(), "Should open OBJ path for dispatch test");
        obj << "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "f 1 2 3\n";
    }

    const fs::path gltfPath = FindWorkspaceRoot() / "Saved" / "TestScratch" / "minimal_triangle.gltf";
    if (!fs::exists(gltfPath)) {
        std::ofstream stream(gltfPath, std::ios::binary);
        stream <<
            R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
            R"("nodes":[{"name":"TriangleNode","mesh":0}],)"
            R"("meshes":[{"name":"TriMesh","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
            R"("accessors":[)"
            R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]},)"
            R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
            R"("bufferViews":[)"
            R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
            R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA="}]})";
    }

    const fs::path fbxPath = FindWorkspaceRoot() / "Assets" / "Source" / "models" / "scenekit_cube_ascii.fbx";

    {
        ri::scene::Scene scene("DispatchObj");
        std::string err;
        const int handle = ri::scene::AddModelNode(
            scene,
            ri::scene::ImportedModelOptions{
                .sourcePath = objPath,
                .nodeName = "DispatchObj",
                .transform = ri::scene::Transform{.position = ri::math::Vec3{1.0f, 2.0f, 3.0f}},
            },
            &err);
        Expect(handle != ri::scene::kInvalidHandle, "AddModelNode should dispatch OBJ: " + err);
        ExpectVec3(scene.ComputeWorldPosition(handle), ri::math::Vec3{1.0f, 2.0f, 3.0f}, "Dispatch OBJ transform");
    }

    {
        ri::scene::Scene scene("DispatchGltf");
        std::string err;
        const int handle = ri::scene::AddModelNode(
            scene,
            ri::scene::ImportedModelOptions{
                .sourcePath = gltfPath,
                .nodeName = "DispatchGltf",
                .transform = ri::scene::Transform{.position = ri::math::Vec3{-1.0f, 4.0f, 2.0f}},
            },
            &err);
        Expect(handle != ri::scene::kInvalidHandle, "AddModelNode should dispatch glTF: " + err);
        ExpectVec3(scene.ComputeWorldPosition(handle), ri::math::Vec3{-1.0f, 4.0f, 2.0f}, "Dispatch glTF transform");
    }

    {
        ri::scene::Scene scene("DispatchFbx");
        std::string err;
        const int handle = ri::scene::AddModelNode(
            scene,
            ri::scene::ImportedModelOptions{
                .sourcePath = fbxPath,
                .nodeName = "DispatchFbx",
                .transform = ri::scene::Transform{.position = ri::math::Vec3{2.5f, 1.0f, -4.0f}},
            },
            &err);
        Expect(handle != ri::scene::kInvalidHandle, "AddModelNode should dispatch FBX: " + err);
        ExpectVec3(scene.ComputeWorldPosition(handle), ri::math::Vec3{2.5f, 1.0f, -4.0f}, "Dispatch FBX transform");
    }

    {
        ri::scene::Scene scene("DispatchUnsupported");
        std::string err;
        Expect(ri::scene::AddModelNode(
                   scene,
                   ri::scene::ImportedModelOptions{
                       .sourcePath = scratchDir / "unsupported.txt",
                   },
                   &err) == ri::scene::kInvalidHandle,
               "AddModelNode should reject unsupported extensions");
        Expect(!err.empty(), "AddModelNode should report an error for unsupported extensions");
    }
}

void TestSceneKitMilestones() {
    const std::filesystem::path workspaceRoot = FindWorkspaceRoot();
    ri::scene::SceneKitMilestoneCallbacks callbacks{};
    callbacks.renderValidator = [](const std::string&,
                                   const ri::scene::Scene& scene,
                                   int cameraNode,
                                   std::string& detail) {
        ri::render::software::ScenePreviewOptions options{};
        options.width = 384;
        options.height = 320;
        const ri::render::software::SoftwareImage image =
            ri::render::software::RenderScenePreview(scene, cameraNode, options);
        const bool rendered = ImageHasContrast(image);
        detail += rendered ? " | rendered" : " | render lacked contrast";
        return rendered;
    };

    const std::vector<ri::scene::SceneKitMilestoneResult> results = ri::scene::RunSceneKitMilestoneChecks(
        ri::scene::SceneKitMilestoneOptions{
            .assetRoot = workspaceRoot / "Assets" / "Source",
        },
        callbacks);

    Expect(results.size() == 10U, "Scene Kit should expose the requested ten example checks");
    Expect(ri::scene::AllSceneKitMilestonesPassed(results), "All Scene Kit milestone checks should pass");
    Expect(results[0].slug == "scene_controls_orbit" && results[0].passed, "Orbit controls example should pass");
    Expect(results[1].slug == "scene_geometry_cube" && results[1].passed, "Geometry cube example should pass");
    Expect(results[2].slug == "scene_interactive_cubes" && Contains(results[2].detail, "PickTargetCube"),
           "Interactive cubes should resolve the pick target");
    Expect(results[5].slug == "scene_loader_gltf" && Contains(results[5].detail, "scenekit_triangle.gltf"),
           "glTF loader example should use the native Scene Kit triangle asset");
    Expect(results[9].slug == "scene_audio_orientation" && Contains(results[9].detail, "listener with 2 sources"),
           "Audio orientation example should report a listener/source preview");
}

void TestAdvancePlayerStamina() {
    ri::trace::MovementControllerOptions opt{};
    opt.staminaMax = 100.0f;
    opt.staminaDrainPerSecond = 25.0f;
    opt.staminaRegenPerSecond = 15.0f;
    const float dt = 1.0f / 60.0f;

    float drained = 100.0f;
    ri::trace::AdvancePlayerStamina(
        drained, true, true, ri::trace::MovementStance::Standing, dt, opt);
    Expect(drained < 100.0f, "Sprint with move input should drain stamina");

    float regen = 10.0f;
    for (int i = 0; i < 360; ++i) {
        ri::trace::AdvancePlayerStamina(
            regen, false, false, ri::trace::MovementStance::Standing, dt, opt);
    }
    Expect(regen >= 99.0f, "Idle regen should approach stamina max");

    ri::trace::MovementControllerOptions capOpt{};
    capOpt.staminaMax = 50.0f;
    capOpt.staminaDrainPerSecond = 25.0f;
    capOpt.staminaRegenPerSecond = 15.0f;
    float capped = 50.0f;
    ri::trace::AdvancePlayerStamina(
        capped, false, false, ri::trace::MovementStance::Standing, 10.0f, capOpt);
    Expect(NearlyEqual(capped, 50.0f), "Regen should not exceed configured stamina max");

    const float base = 88.0f;
    const float computed = ri::trace::ComputeAdvancedPlayerStamina(
        base, true, true, ri::trace::MovementStance::Standing, dt, opt);
    float mutated = base;
    ri::trace::AdvancePlayerStamina(
        mutated, true, true, ri::trace::MovementStance::Standing, dt, opt);
    Expect(NearlyEqual(computed, mutated), "ComputeAdvancedPlayerStamina should match AdvancePlayerStamina");
    Expect(NearlyEqual(base, 88.0f), "ComputeAdvancedPlayerStamina must not mutate the input value");
}

void TestYawAngles() {
    using ri::math::NormalizeYawRadians;
    using ri::math::NormalizeYawRadiansLegacyIterative;
    using ri::math::Pi;
    using ri::math::ShortestYawDeltaRadians;
    using ri::math::StepYawToward;
    using ri::math::TwoPi;
    using ri::math::YawShortestDeltaMode;

    Expect(NearlyEqual(NormalizeYawRadians(0.0f), 0.0f), "Zero yaw should normalize to zero");
    Expect(NearlyEqual(NormalizeYawRadians(Pi()), Pi(), 0.0001f), "Pi should remain Pi under remainder wrap");

    const float spun = 1000.0f * TwoPi() + 0.31f;
    Expect(NearlyEqual(NormalizeYawRadians(spun), 0.31f, 0.002f),
           "Large multi-turn angles should wrap in O(1) without drift");

    const auto full = StepYawToward(0.0f, 0.5f, 2.0f);
    Expect(NearlyEqual(full.newYaw, 0.5f, 0.0001f) && NearlyEqual(full.alignment, 1.0f, 0.0001f),
           "StepYawToward should reach target when cap allows");

    const auto partial = StepYawToward(0.0f, 1.0f, 0.1f);
    Expect(NearlyEqual(partial.newYaw, 0.1f, 0.0001f), "StepYawToward should clamp step magnitude");
    Expect(partial.alignment < 1.0f && partial.alignment > 0.5f,
           "Partial step should leave a non-trivial remaining yaw (cos(0.9 rad) ~ 0.62)");

    const auto badDesired = StepYawToward(0.2f, std::numeric_limits<float>::quiet_NaN(), 0.5f);
    Expect(NearlyEqual(badDesired.newYaw, 0.2f, 0.0001f) && NearlyEqual(badDesired.alignment, 1.0f, 0.0001f),
           "Non-finite desired yaw should leave heading unchanged");

    auto legacyShortestDelta = [](float fromRadians, float toRadians) {
        constexpr float pi = Pi();
        constexpr float twoPi = TwoPi();
        float delta = toRadians - fromRadians;
        while (delta > pi) {
            delta -= twoPi;
        }
        while (delta < -pi) {
            delta += twoPi;
        }
        return delta;
    };
    struct Sample {
        float from;
        float to;
    };
    const Sample samples[] = {
        {0.1f, 2.4f},
        {-1.9f, 2.2f},
        {3.0f, -3.1f},
        {100.0f, -50.0f},
    };
    for (const Sample& s : samples) {
        Expect(NearlyEqual(NormalizeYawRadians(s.to - s.from), legacyShortestDelta(s.from, s.to), 0.0002f),
               "IEEE remainder shortest yaw delta should match legacy iterative wrap for representative angles");
    }

    Expect(NearlyEqual(ShortestYawDeltaRadians(-1.0f, 1.0f, YawShortestDeltaMode::IeeeRemainder),
                       ShortestYawDeltaRadians(-1.0f, 1.0f, YawShortestDeltaMode::LegacyIterativePi),
                       0.0002f),
           "Both yaw delta modes should agree on representative headings");

    Expect(NearlyEqual(NormalizeYawRadiansLegacyIterative(9.42f),
                       legacyShortestDelta(0.0f, 9.42f),
                       0.0002f),
           "NormalizeYawRadiansLegacyIterative should match extracted legacy loop");

    const auto stepIeee = StepYawToward(0.0f, 1.2f, 0.05f, YawShortestDeltaMode::IeeeRemainder);
    const auto stepLegacy = StepYawToward(0.0f, 1.2f, 0.05f, YawShortestDeltaMode::LegacyIterativePi);
    Expect(NearlyEqual(stepIeee.newYaw, stepLegacy.newYaw, 0.0001f)
           && NearlyEqual(stepIeee.alignment, stepLegacy.alignment, 0.0001f),
           "StepYawToward modes should match when both paths agree on delta normalization");
}

void TestMovementControllerOptionalStamina() {
    ri::trace::MovementControllerOptions optOff{};
    optOff.simulateStamina = false;
    float s = 40.0f;
    ri::trace::AdvancePlayerStamina(
        s, true, true, ri::trace::MovementStance::Standing, 1.0f, optOff);
    Expect(s == 40.0f, "AdvancePlayerStamina should no-op when simulateStamina is false");

    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
    });
    const float dt = 1.0f / 60.0f;
    ri::trace::MovementControllerOptions opts{};
    opts.simulateStamina = false;

    ri::trace::MovementControllerState exhausted{};
    exhausted.onGround = true;
    exhausted.stamina = 0.0f;
    exhausted.body.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
        .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
    };

    float sprintVzOff = 0.0f;
    {
        ri::trace::MovementControllerState st = exhausted;
        for (int i = 0; i < 36; ++i) {
            st = ri::trace::SimulateMovementControllerStep(
                     traceScene,
                     st,
                     ri::trace::MovementInput{
                         .moveForward = 1.0f,
                         .sprintHeld = true,
                     },
                     dt,
                     opts)
                     .state;
        }
        Expect(st.stamina == 0.0f, "Stamina should stay fixed when simulation is disabled");
        sprintVzOff = st.body.velocity.z;
    }

    ri::trace::MovementControllerOptions optsOn = opts;
    optsOn.simulateStamina = true;
    float sprintVzOn = 0.0f;
    {
        ri::trace::MovementControllerState st = exhausted;
        for (int i = 0; i < 36; ++i) {
            st = ri::trace::SimulateMovementControllerStep(
                     traceScene,
                     st,
                     ri::trace::MovementInput{
                         .moveForward = 1.0f,
                         .sprintHeld = true,
                     },
                     dt,
                     optsOn)
                     .state;
        }
        sprintVzOn = st.body.velocity.z;
    }
    Expect(sprintVzOff > sprintVzOn + 0.5f,
           "Without stamina simulation, sprint should stay available even at zero stamina");
}

void TestLocomotionTuning() {
    ri::trace::LocomotionTuning tuning = ri::trace::DefaultLocomotionTuning();
    Expect(NearlyEqual(tuning.walkSpeed, 5.0f) && NearlyEqual(tuning.jumpForce, 9.6f) && NearlyEqual(tuning.maxStepHeight, 0.7f),
           "Locomotion defaults should match the legacy prototype constants");

    ri::trace::LocomotionTuningPatch patch{};
    patch.walkSpeed = 10.0f;
    patch.sprintSpeed = 3.0f;
    ri::trace::ApplyLocomotionTuningPatch(tuning, patch);
    Expect(NearlyEqual(tuning.walkSpeed, 10.0f) && NearlyEqual(tuning.sprintSpeed, 10.0f),
           "Sprint speed should be clamped to at least walk speed after patches");

    ri::trace::LocomotionTuningPatch junk{};
    junk.gravity = std::numeric_limits<float>::quiet_NaN();
    const float gravityBefore = tuning.gravity;
    ri::trace::ApplyLocomotionTuningPatch(tuning, junk);
    Expect(NearlyEqual(tuning.gravity, gravityBefore),
           "Non-finite patch entries should be ignored");

    const ri::trace::MovementControllerOptions options = ri::trace::ToMovementControllerOptions(ri::trace::DefaultLocomotionTuning());
    Expect(NearlyEqual(options.maxGroundSpeed, 5.0f) && NearlyEqual(options.maxSprintGroundSpeed, 8.0f)
               && NearlyEqual(options.jumpSpeed, 9.6f) && NearlyEqual(options.gravity, 32.0f)
               && NearlyEqual(options.maxAirSpeed, 8.0f) && options.simulateStamina,
           "Locomotion tuning should map cleanly into movement controller options (stamina stays opt-in default on)");
}

void TestMovementControllerStep() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
        ri::trace::TraceCollider{
            .id = "wall",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{4.0f, 0.0f, -2.0f},
                .max = ri::math::Vec3{5.0f, 4.0f, 2.0f},
            },
            .structural = true,
        },
    });

    ri::trace::MovementControllerState state{};
    state.onGround = true;
    state.body.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
        .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
    };

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 30; ++i) {
        const auto step = ri::trace::SimulateMovementControllerStep(
            traceScene,
            state,
            ri::trace::MovementInput{
                .moveForward = 1.0f,
            },
            dt);
        state = step.state;
    }
    Expect(state.body.velocity.z > 4.0f, "Movement controller should accelerate strongly on ground");

    const float jumpStartY = state.body.bounds.min.y;
    auto jump = ri::trace::SimulateMovementControllerStep(
        traceScene,
        state,
        ri::trace::MovementInput{
            .moveForward = 1.0f,
            .jumpPressed = true,
        },
        dt);
    Expect(jump.state.body.velocity.y > 1.0f, "Jump should produce upward vertical velocity");
    Expect(!jump.state.onGround, "Jump should leave the ground state");

    auto inAir = jump.state;
    for (int i = 0; i < 20; ++i) {
        const auto step = ri::trace::SimulateMovementControllerStep(
            traceScene,
            inAir,
            ri::trace::MovementInput{
                .moveForward = 1.0f,
                .moveRight = 1.0f,
            },
            dt);
        inAir = step.state;
    }
    Expect(inAir.body.bounds.min.y > jumpStartY, "Air movement should preserve upward travel shortly after jump");
    Expect(inAir.body.velocity.x > 0.2f, "Air control should steer horizontal velocity");
}

void TestMovementControllerCoyoteBufferAndShortHop() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
    });

    const float dt = 1.0f / 60.0f;

    {
        ri::trace::MovementControllerState st{};
        st.onGround = true;
        st.jumpBufferTimeRemaining = 0.12f;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
            .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
        };
        ri::trace::MovementControllerOptions opt{};
        opt.jumpBufferTimeSeconds = 0.16f;
        const auto step = ri::trace::SimulateMovementControllerStep(traceScene, st, {}, dt, opt);
        Expect(step.state.body.velocity.y > 1.0f,
               "Buffered jump input should still fire once the controller is grounded");
    }

    {
        ri::trace::MovementControllerOptions opt{};
        opt.jumpBufferTimeSeconds = 0.0f;
        opt.coyoteTimeSeconds = 0.2f;
        ri::trace::MovementControllerState st{};
        st.onGround = false;
        st.coyoteTimeRemaining = 0.15f;
        st.jumpPressedLastFrame = false;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 0.5f, -0.25f},
            .max = ri::math::Vec3{0.25f, 2.32f, 0.25f},
        };
        const auto step = ri::trace::SimulateMovementControllerStep(
            traceScene,
            st,
            ri::trace::MovementInput{.jumpPressed = true},
            dt,
            opt);
        Expect(step.state.body.velocity.y > 1.0f,
               "Coyote time should allow an instant jump shortly after leaving support");
    }

    {
        ri::trace::MovementControllerOptions opt{};
        opt.lowJumpGravityMultiplier = 2.0f;
        auto measurePeak = [&](bool shortJump) {
            ri::trace::MovementControllerState st{};
            st.onGround = true;
            st.body.bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
                .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
            };
            st = ri::trace::SimulateMovementControllerStep(
                       traceScene,
                       st,
                       ri::trace::MovementInput{
                           .jumpPressed = true,
                           .applyShortJumpGravity = shortJump,
                       },
                       dt,
                       opt)
                       .state;
            float peak = st.body.bounds.max.y;
            for (int i = 0; i < 90; ++i) {
                const bool rising = st.body.velocity.y > 0.0f;
                st = ri::trace::SimulateMovementControllerStep(
                         traceScene,
                         st,
                         ri::trace::MovementInput{.applyShortJumpGravity = shortJump && rising},
                         dt,
                         opt)
                         .state;
                peak = std::max(peak, st.body.bounds.max.y);
            }
            return peak;
        };
        const float fullPeak = measurePeak(false);
        const float shortPeak = measurePeak(true);
        Expect(shortPeak < fullPeak - 0.12f,
               "Short-jump gravity scaling should produce a lower apex than a full hold");
    }
}

void TestMovementControllerStanceAndSprint() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
    });

    const float dt = 1.0f / 60.0f;
    ri::trace::MovementControllerOptions opts{};

    auto runForward = [&](ri::trace::MovementStance stance, bool sprint, float stamina) {
        ri::trace::MovementControllerState st{};
        st.onGround = true;
        st.stance = stance;
        st.stamina = stamina;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
            .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
        };
        for (int i = 0; i < 36; ++i) {
            st = ri::trace::SimulateMovementControllerStep(
                     traceScene,
                     st,
                     ri::trace::MovementInput{
                         .moveForward = 1.0f,
                         .sprintHeld = sprint,
                     },
                     dt,
                     opts)
                     .state;
        }
        return st.body.velocity.z;
    };

    const float walkVz = runForward(ri::trace::MovementStance::Standing, false, 100.0f);
    const float crouchVz = runForward(ri::trace::MovementStance::Crouching, false, 100.0f);
    const float sprintVz = runForward(ri::trace::MovementStance::Standing, true, 100.0f);
    Expect(walkVz > crouchVz + 0.5f, "Crouching should cap ground speed below standing walk");
    Expect(sprintVz > walkVz + 0.5f, "Sprint should exceed walk speed while stamina remains");

    ri::trace::MovementControllerState drain{};
    drain.onGround = true;
    drain.stamina = 100.0f;
    drain.body.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
        .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
    };
    for (int i = 0; i < 120; ++i) {
        drain = ri::trace::SimulateMovementControllerStep(
                    traceScene,
                    drain,
                    ri::trace::MovementInput{.moveForward = 1.0f, .sprintHeld = true},
                    dt,
                    opts)
                    .state;
    }
    Expect(drain.stamina < 60.0f, "Sprinting with movement input should drain stamina over time");
}

void TestKinematicFallMultiplierAndTerminalVelocity() {
    const std::vector<ri::trace::TraceCollider> noColliders{};
    ri::trace::TraceScene traceScene(noColliders);

    ri::trace::KinematicBodyState start{};
    start.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 120.0f, -0.25f},
        .max = ri::math::Vec3{0.25f, 121.0f, 0.25f},
    };

    const ri::trace::KinematicStepResult lightFall = ri::trace::SimulateKinematicBodyStep(
        traceScene,
        start,
        1.0f / 60.0f,
        ri::trace::KinematicPhysicsOptions{.fallGravityMultiplier = 1.0f, .ignoreColliderId = {}});
    const ri::trace::KinematicStepResult heavyFall = ri::trace::SimulateKinematicBodyStep(
        traceScene,
        start,
        1.0f / 60.0f,
        ri::trace::KinematicPhysicsOptions{.fallGravityMultiplier = 2.0f, .ignoreColliderId = {}});
    Expect(heavyFall.state.velocity.y < lightFall.state.velocity.y - 0.05f,
           "Fall gravity multiplier should accelerate descent more strongly when above 1");

    ri::trace::KinematicBodyState drop = start;
    ri::trace::KinematicPhysicsOptions capped{
        .fallGravityMultiplier = 1.0f,
        .maxFallSpeed = 6.0f,
        .ignoreColliderId = {},
    };
    for (int i = 0; i < 180; ++i) {
        drop = ri::trace::SimulateKinematicBodyStep(traceScene, drop, 1.0f / 60.0f, capped).state;
    }
    Expect(std::fabs(drop.velocity.y + 6.0f) < 0.2f,
           "Max fall speed should clamp downward velocity in sustained airtime");
}

void TestKinematicDurationSliceBudgetFlag() {
    const std::vector<ri::trace::TraceCollider> noColliders{};
    ri::trace::TraceScene traceScene(noColliders);

    ri::trace::KinematicBodyState start{};
    start.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 120.0f, -0.25f},
        .max = ri::math::Vec3{0.25f, 121.0f, 0.25f},
    };

    ri::trace::KinematicAdvanceStats stats{};
    const ri::trace::KinematicStepResult stepped = ri::trace::SimulateKinematicBodyForDuration(
        traceScene,
        start,
        50.0f,
        ri::trace::KinematicPhysicsOptions{.ignoreColliderId = {}},
        ri::trace::KinematicVolumeModifiers{},
        ri::trace::KinematicConstraintState{},
        &stats);

    Expect(stats.sliceCount == 128U, "Duration stepping should respect the hard safety slice budget");
    Expect(stats.hitSliceBudget, "Duration stepping should report when requested time exceeds the safety budget");
    Expect(stats.consumedSeconds < 50.0f,
           "Duration stepping should report partial consumption when the safety slice budget is exceeded");
    Expect(stepped.state.bounds.min.y < start.bounds.min.y,
           "Duration stepping should still advance simulation state before hitting the safety slice budget");
}

void TestMovementControllerTraversalOptIn() {
    const std::vector<ri::trace::TraceCollider> noColliders{};
    ri::trace::TraceScene airScene(noColliders);

    ri::trace::MovementControllerOptions opts{};
    opts.traversalClimbSpeed = 4.0f;
    opts.coyoteTimeSeconds = 0.0f;

    ri::trace::MovementControllerState st{};
    st.onGround = false;
    st.body.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 6.0f, -0.25f},
        .max = ri::math::Vec3{0.25f, 7.0f, 0.25f},
    };

    const float dt = 1.0f / 60.0f;
    const float y0 = st.body.bounds.min.y;
    for (int i = 0; i < 12; ++i) {
        st = ri::trace::SimulateMovementControllerStep(
                 airScene,
                 st,
                 ri::trace::MovementInput{.traversalClimbAxis = 1.0f},
                 dt,
                 opts)
                 .state;
    }
    Expect(st.body.bounds.min.y > y0 + 0.35f,
           "Opt-in traversal climb should raise the hull without relying on gravity");

    ri::trace::MovementControllerOptions off{};
    off.coyoteTimeSeconds = 0.0f;
    ri::trace::MovementControllerState neutral{};
    neutral.onGround = false;
    neutral.body.bounds = st.body.bounds;
    neutral.body.velocity = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    const auto noTrav =
        ri::trace::SimulateMovementControllerStep(airScene, neutral, {}, dt, off).state;
    Expect(noTrav.body.velocity.y < -0.2f,
           "With traversal disabled (default), airborne bodies should still accumulate gravity");
}

void TestMovementControllerProtoMovementExtensions() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
    });

    const float dt = 1.0f / 60.0f;
    ri::trace::MovementControllerOptions opts{};

    {
        ri::trace::MovementControllerState st{};
        st.onGround = true;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 0.02f, -0.25f},
            .max = ri::math::Vec3{0.25f, 1.82f, 0.25f},
        };
        for (int i = 0; i < 40; ++i) {
            st = ri::trace::SimulateMovementControllerStep(
                     traceScene,
                     st,
                     ri::trace::MovementInput{
                         .moveForward = 1.0f,
                         .viewForwardWorld = {1.0f, 0.0f, 0.0f},
                     },
                     dt,
                     opts)
                     .state;
        }
        Expect(st.body.velocity.x > 3.5f && std::fabs(st.body.velocity.z) < 2.0f,
               "Camera-relative view forward should steer horizontal move along +X when authored");
    }

    {
        const std::vector<ri::trace::TraceCollider> noColliders{};
        ri::trace::TraceScene airScene(noColliders);
        ri::trace::MovementControllerOptions airOpts{};
        airOpts.coyoteTimeSeconds = 0.14f;
        ri::trace::MovementControllerState st{};
        st.onGround = false;
        st.coyoteTimeRemaining = 0.12f;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 4.0f, -0.25f},
            .max = ri::math::Vec3{0.25f, 5.0f, 0.25f},
        };
        const auto first = ri::trace::SimulateMovementControllerStep(airScene, st, {}, dt, airOpts);
        Expect(std::fabs(first.state.body.velocity.y) < 0.08f,
               "Coyote window should suppress gravity like the prototype hang window");
        ri::trace::MovementControllerState mid = first.state;
        for (int i = 0; i < 20; ++i) {
            mid = ri::trace::SimulateMovementControllerStep(airScene, mid, {}, dt, airOpts).state;
        }
        Expect(mid.body.velocity.y < -1.0f,
               "After coyote expires, standard gravity integration should resume");
    }

    {
        const std::vector<ri::trace::TraceCollider> noColliders{};
        ri::trace::TraceScene airScene(noColliders);
        ri::trace::MovementControllerOptions airOpts{};
        airOpts.coyoteTimeSeconds = 0.0f;
        ri::trace::KinematicVolumeModifiers vol{};
        vol.flow = ri::math::Vec3{5.0f, 0.0f, 0.0f};
        ri::trace::MovementControllerState st{};
        st.onGround = false;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 10.0f, -0.25f},
            .max = ri::math::Vec3{0.25f, 11.0f, 0.25f},
        };
        for (int i = 0; i < 8; ++i) {
            st = ri::trace::SimulateMovementControllerStep(airScene, st, {}, dt, airOpts, vol).state;
        }
        Expect(st.body.velocity.x > 0.4f,
               "Kinematic volume flow should accumulate through the movement controller step");
    }

    {
        const std::vector<ri::trace::TraceCollider> noColliders{};
        ri::trace::TraceScene airScene(noColliders);
        ri::trace::MovementControllerOptions airOpts{};
        airOpts.airControl = 0.7f;
        airOpts.airTurnResponsiveness = 1.1f;
        ri::trace::MovementControllerState st{};
        st.onGround = false;
        st.body.velocity = ri::math::Vec3{6.0f, 0.0f, 0.0f};
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 4.0f, -0.25f},
            .max = ri::math::Vec3{0.25f, 5.0f, 0.25f},
        };
        for (int i = 0; i < 10; ++i) {
            st = ri::trace::SimulateMovementControllerStep(
                     airScene,
                     st,
                     ri::trace::MovementInput{
                         .moveForward = 1.0f,
                         .viewForwardWorld = {0.0f, 0.0f, 1.0f},
                     },
                     dt,
                     airOpts)
                     .state;
        }
        Expect(st.body.velocity.z > 0.8f,
               "Air turn responsiveness should let in-air steering rapidly bend velocity toward new input");
    }

    {
        ri::trace::TraceScene wallScene({
            ri::trace::TraceCollider{
                .id = "wall",
                .bounds = ri::spatial::Aabb{
                    .min = ri::math::Vec3{0.55f, 0.0f, -2.0f},
                    .max = ri::math::Vec3{0.75f, 3.0f, 2.0f},
                },
                .structural = true,
            },
        });
        ri::trace::MovementControllerOptions wallOpts{};
        wallOpts.enableWallJump = true;
        wallOpts.coyoteTimeSeconds = 0.0f;
        wallOpts.jumpBufferTimeSeconds = 0.0f;
        wallOpts.wallJumpProbeDistance = 0.7f;
        wallOpts.wallJumpAwaySpeed = 5.8f;
        wallOpts.wallJumpVerticalSpeed = 7.2f;
        ri::trace::MovementControllerState st{};
        st.onGround = false;
        st.jumpPressedLastFrame = false;
        st.body.bounds = ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 1.2f, -0.25f},
            .max = ri::math::Vec3{0.25f, 3.0f, 0.25f},
        };
        st.body.velocity = ri::math::Vec3{1.5f, -0.4f, 0.0f};
        const auto wallJump = ri::trace::SimulateMovementControllerStep(
            wallScene,
            st,
            ri::trace::MovementInput{
                .moveForward = 1.0f,
                .viewForwardWorld = {1.0f, 0.0f, 0.0f},
                .jumpPressed = true,
            },
            dt,
            wallOpts);
        Expect(wallJump.state.body.velocity.y > 4.0f && wallJump.state.body.velocity.x < -0.5f,
               "Airborne jump near a vertical wall should trigger a wall-jump impulse away from the wall");
    }
}

void TestMovementControllerGroundProbeJump() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
        },
    });

    ri::trace::MovementControllerOptions opt{};
    opt.jumpBufferTimeSeconds = 0.0f;
    opt.coyoteTimeSeconds = 0.0f;
    opt.groundProbeJumpMaxDown = 0.45f;

    ri::trace::MovementControllerState st{};
    st.onGround = false;
    st.coyoteTimeRemaining = 0.0f;
    st.jumpPressedLastFrame = false;
    st.body.bounds = ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.25f, 0.32f, -0.25f},
        .max = ri::math::Vec3{0.25f, 2.14f, 0.25f},
    };

    const auto step = ri::trace::SimulateMovementControllerStep(
        traceScene,
        st,
        ri::trace::MovementInput{.jumpPressed = true},
        1.0f / 60.0f,
        opt);
    Expect(step.state.body.velocity.y > 1.0f,
           "Ground probe jump should allow leaving a small gap without coyote or touch contact");
}

void TestTraceSceneMetrics() {
    ri::trace::TraceScene traceScene({
        ri::trace::TraceCollider{
            .id = "ground",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-50.0f, -1.0f, -50.0f},
                .max = ri::math::Vec3{50.0f, 0.0f, 50.0f},
            },
            .structural = true,
            .dynamic = false,
        },
        ri::trace::TraceCollider{
            .id = "dynamic_box",
            .bounds = ri::spatial::Aabb{
                .min = ri::math::Vec3{-1.0f, 0.0f, -1.0f},
                .max = ri::math::Vec3{1.0f, 2.0f, 1.0f},
            },
            .structural = false,
            .dynamic = true,
        },
    });

    auto metrics = traceScene.Metrics();
    Expect(metrics.colliderCount == 2U, "TraceScene should report total collider count");
    Expect(metrics.staticColliderCount == 1U && metrics.dynamicColliderCount == 1U,
           "TraceScene should split static and dynamic collider counts");
    Expect(metrics.structuralStaticColliderCount == 1U,
           "TraceScene should track structural static collider count");

    (void)traceScene.QueryCollidablesForBox(ri::spatial::Aabb{
        .min = ri::math::Vec3{-2.0f, -2.0f, -2.0f},
        .max = ri::math::Vec3{2.0f, 2.0f, 2.0f},
    });
    (void)traceScene.QueryCollidablesForRay(
        ri::math::Vec3{0.0f, 1.0f, -10.0f},
        ri::math::Vec3{0.0f, 0.0f, 1.0f},
        30.0f);
    (void)traceScene.TraceBox(ri::spatial::Aabb{
        .min = ri::math::Vec3{-0.5f, -0.5f, -0.5f},
        .max = ri::math::Vec3{0.5f, 0.5f, 0.5f},
    });
    (void)traceScene.TraceRay(
        ri::math::Vec3{0.0f, 1.0f, -10.0f},
        ri::math::Vec3{0.0f, 0.0f, 1.0f},
        30.0f);
    (void)traceScene.TraceSweptBox(
        ri::spatial::Aabb{
            .min = ri::math::Vec3{-0.25f, 0.1f, -2.0f},
            .max = ri::math::Vec3{0.25f, 1.8f, -1.5f},
        },
        ri::math::Vec3{0.0f, 0.0f, 3.0f});

    metrics = traceScene.Metrics();
    Expect(metrics.boxQueries >= 1U && metrics.rayQueries >= 1U,
           "TraceScene should count broadphase query calls");
    Expect(metrics.traceBoxQueries >= 1U && metrics.traceRayQueries >= 1U && metrics.sweptBoxQueries >= 1U,
           "TraceScene should count trace query calls");
    Expect(metrics.staticCandidates >= 1U && metrics.dynamicCandidates >= 1U,
           "TraceScene should track static and dynamic candidate usage");

    traceScene.ResetMetrics();
    metrics = traceScene.Metrics();
    Expect(metrics.boxQueries == 0U && metrics.rayQueries == 0U &&
               metrics.sweptBoxQueries == 0U &&
               metrics.traceBoxQueries == 0U &&
               metrics.traceRayQueries == 0U,
           "TraceScene metric reset should clear query counters");
    Expect(metrics.staticCandidates == 0U && metrics.dynamicCandidates == 0U,
           "TraceScene metric reset should clear candidate counters");
    Expect(metrics.colliderCount == 2U,
           "TraceScene metric reset should preserve collider inventory counts");
}

void TestEntityPhysicsBatch() {
    using ri::spatial::Aabb;
    using ri::trace::KinematicBodyState;
    using ri::trace::KinematicEntitySlot;
    using ri::trace::KinematicPhysicsOptions;
    using ri::trace::ObjectPhysicsBatchOptions;
    using ri::trace::StepKinematicEntityBatch;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    TraceScene scene({
        TraceCollider{.id = "floor",
                      .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}},
                      .structural = true,
                      .dynamic = false},
        TraceCollider{.id = "crate_ent_a",
                      .bounds = Aabb{.min = {-0.3f, 0.0f, -0.3f}, .max = {0.3f, 0.6f, 0.3f}},
                      .structural = true,
                      .dynamic = true},
    });

    std::vector<KinematicEntitySlot> entities{};
    KinematicEntitySlot first{};
    first.entityId = "physical_prop_alpha";
    first.objectColliderId = "crate_ent_a";
    first.state =
        KinematicBodyState{.bounds = Aabb{.min = {-0.3f, 0.0f, -0.3f}, .max = {0.3f, 0.6f, 0.3f}},
                           .velocity = ri::math::Vec3{0.5f, 0.0f, 0.0f}};
    entities.push_back(first);

    ObjectPhysicsBatchOptions batchOpts{.enableSleep = false};
    const auto result =
        StepKinematicEntityBatch(scene, entities, 1.0f / 60.0f, KinematicPhysicsOptions{}, {}, {}, batchOpts);
    Expect(result.simulatedCount == 1U && entities.size() == 1U,
           "Entity batch should integrate each awake kinematic slot");
    Expect(entities.front().entityId == "physical_prop_alpha",
           "StepKinematicEntityBatch should preserve gameplay entity ids");
}

void TestPickupHoldAndThrowObjectPhysics() {
    using ri::spatial::Aabb;
    using ri::trace::HeldObjectState;
    using ri::trace::KinematicObjectSlot;
    using ri::trace::ObjectCarryOptions;
    using ri::trace::ThrowHeldKinematicObject;
    using ri::trace::TryPickupNearestKinematicObject;
    using ri::trace::UpdateHeldKinematicObject;

    std::vector<KinematicObjectSlot> objects{};
    KinematicObjectSlot near{};
    near.objectColliderId = "crate_near";
    near.state.bounds = Aabb{.min = {0.8f, 0.1f, 1.1f}, .max = {1.2f, 0.5f, 1.5f}};
    objects.push_back(near);

    KinematicObjectSlot far{};
    far.objectColliderId = "crate_far";
    far.state.bounds = Aabb{.min = {-0.2f, 0.1f, 3.8f}, .max = {0.2f, 0.5f, 4.2f}};
    objects.push_back(far);

    HeldObjectState held{};
    ObjectCarryOptions options{};
    options.maxPickupDistance = 2.0f;
    options.minPickupAimDot = 0.7f;
    options.holdDistance = 1.5f;
    options.holdHeightOffset = 0.3f;
    options.holdFollowResponsiveness = 100.0f; // snap for deterministic test
    options.throwSpeed = 12.0f;
    options.throwUpwardBoost = 1.0f;
    options.throwInheritHolderVelocityScale = 0.5f;

    const ri::math::Vec3 holderPosition{0.0f, 0.0f, 0.0f};
    const ri::math::Vec3 holderForward{0.0f, 0.0f, 1.0f};
    const bool picked = TryPickupNearestKinematicObject(objects, holderPosition, holderForward, held, options);
    Expect(picked, "Pickup helper should acquire a nearby object in front of the holder");
    Expect(held.heldObjectIndex.has_value() && *held.heldObjectIndex == 0U,
           "Pickup helper should choose the nearest valid front-facing object");

    const bool heldUpdated =
        UpdateHeldKinematicObject(objects, 1.0f / 60.0f, holderPosition, holderForward, held, options);
    Expect(heldUpdated, "Hold helper should update the carried object's anchor");

    const ri::math::Vec3 heldCenter = ri::spatial::Center(objects[0].state.bounds);
    Expect(std::fabs(heldCenter.z - 1.5f) < 0.05f && std::fabs(heldCenter.y - 0.3f) < 0.05f,
           "Held object should follow in front of the player with configured offset");
    Expect(ri::math::LengthSquared(objects[0].state.velocity) < 1e-8f,
           "Held object should suppress residual linear velocity while carried");

    const ri::math::Vec3 holderVelocity{2.0f, 0.0f, 0.0f};
    const bool thrown = ThrowHeldKinematicObject(objects, holderForward, holderVelocity, held, options);
    Expect(thrown, "Throw helper should release and impulse the carried object");
    Expect(!held.heldObjectIndex.has_value(), "Throw helper should clear held state after release");
    Expect(std::fabs(objects[0].state.velocity.z - 12.0f) < 0.05f
               && std::fabs(objects[0].state.velocity.y - 1.0f) < 0.05f
               && std::fabs(objects[0].state.velocity.x - 1.0f) < 0.05f,
           "Throw helper should apply forward, upward, and holder-inherited velocity components");
}

void TestSceneEntityPhysicsApply() {
    ri::scene::Scene scene{"SceneEntityPhysics"};
    const int root = scene.CreateNode("root");

    ri::trace::KinematicBodyState grounded{};
    grounded.bounds =
        ri::spatial::Aabb{.min = {1.0f, 0.0f, 2.0f}, .max = {3.0f, 2.0f, 4.0f}}; // center (2,1,3)

    Expect(ri::scene::ApplyKinematicBodyStateWorldCenterToSceneNode(scene, root, grounded),
           "Scene sync should accept a finite kinematic bounds volume");
    const ri::math::Vec3 world = scene.ComputeWorldPosition(root);
    Expect(std::abs(world.x - 2.0f) < 1.0e-4f && std::abs(world.y - 1.0f) < 1.0e-4f
               && std::abs(world.z - 3.0f) < 1.0e-4f,
           "Scene pivot should track the kinematic bounds center for root nodes");

    const int parent = scene.CreateNode("parent", ri::scene::kInvalidHandle);
    scene.GetNode(parent).localTransform.position = ri::math::Vec3{10.0f, 0.0f, 0.0f};
    const int child = scene.CreateNode("child", parent);

    Expect(ri::scene::ApplyKinematicBodyStateWorldCenterToSceneNode(scene, child, grounded),
           "Scene sync should succeed for parented kinematic targets");
    const ri::math::Vec3 childWorld = scene.ComputeWorldPosition(child);
    Expect(std::abs(childWorld.x - 2.0f) < 1.0e-3f && std::abs(childWorld.y - 1.0f) < 1.0e-3f
               && std::abs(childWorld.z - 3.0f) < 1.0e-3f,
           "Parented nodes should resolve local translation so the world pivot matches bounds center");
}

#ifdef RAWIRON_WITH_DEV_INSPECTOR
void TestDevelopmentInspector() {
    ri::dev::DevelopmentInspector inspector(
        ri::dev::InspectorConfig{.enabled = true, .allowDevelopmentCommands = true});
    inspector.RegisterSnapshotSource("frame", [] { return std::string("{\"ms\":5.1}"); });
    Expect(inspector.SnapshotSourceIds().size() == 1U && inspector.SnapshotSourceIds().front() == "frame",
           "DevelopmentInspector should expose registered snapshot ids");

    const std::string snapshot = inspector.BuildSnapshotJson();
    Expect(Contains(snapshot, "\"version\":2") && Contains(snapshot, "\"seq\":") && Contains(snapshot, "frame")
               && Contains(snapshot, "5.1"),
           "DevelopmentInspector should bundle snapshot sources into schema v2 JSON");

    inspector.RegisterSnapshotSource("bad", []() -> std::string { throw std::runtime_error("boom"); });
    const std::string errSnap = inspector.BuildSnapshotJson();
    Expect(Contains(errSnap, "bad") && Contains(errSnap, "boom"),
           "DevelopmentInspector should isolate snapshot source exceptions into error payloads");

    std::vector<std::string> transportLines;
    inspector.SetTransport(std::make_shared<ri::dev::CallbackInspectorTransport>(
        [&](const std::string& line) { transportLines.push_back(line); }));
    inspector.PostDiagnostic(ri::dev::InspectorChannel::Log, "ping");
    inspector.Pump();
    Expect(transportLines.size() == 1U && Contains(transportLines.front(), "\"version\":2")
               && Contains(transportLines.front(), "\"seq\":") && Contains(transportLines.front(), "ping"),
           "Pump should emit schema v2 NDJSON diagnostics");

    inspector.RegisterCommandHandler("test.", [](std::string_view name, std::string_view argsJson) {
        if (name != "test.cmd") {
            return std::string{};
        }
        if (argsJson.empty()) {
            return std::string{"ok"};
        }
        return std::string{"has-args"};
    });
    Expect(inspector.TryHandleCommand("test.cmd") == "ok",
           "DevelopmentInspector should dispatch bare commands");
    Expect(inspector.TryHandleCommand("test.cmd {\"x\":3}") == "has-args",
           "DevelopmentInspector should pass JSON tails as argsJson");

    inspector.SetConfig(ri::dev::InspectorConfig{.enabled = true,
                                                 .allowDevelopmentCommands = false,
                                                 .maxBufferedDiagnostics = 8192});
    Expect(inspector.TryHandleCommand("test.cmd").empty(),
           "DevelopmentInspector should reject commands when allowDevelopmentCommands is false");

    ri::dev::DevelopmentInspector capped(
        ri::dev::InspectorConfig{.enabled = true, .maxBufferedDiagnostics = 2});
    capped.PostDiagnostic(ri::dev::InspectorChannel::Telemetry, "a");
    capped.PostDiagnostic(ri::dev::InspectorChannel::Telemetry, "b");
    capped.PostDiagnostic(ri::dev::InspectorChannel::Telemetry, "c");
    Expect(capped.DiagnosticsDroppedCount() >= 1U,
           "DevelopmentInspector should drop oldest diagnostics when the buffer is full");

    const auto command = ri::dev::BuildInspectorBrowserLaunchCommand(
        ri::dev::InspectorBrowserLaunchOptions{
            .url = "http://127.0.0.1:4173/index.html",
            .browserPath = "C:/Program Files/Google/Chrome/Application/chrome.exe",
            .kioskMode = true,
            .dryRun = true,
        });
    Expect(command.has_value() && Contains(*command, "127.0.0.1") && Contains(*command, "kiosk"),
           "Inspector browser launcher should produce a deterministic kiosk launch command");
    std::string dryRunCommand;
    Expect(ri::dev::LaunchInspectorBrowser(
               ri::dev::InspectorBrowserLaunchOptions{
                   .url = "http://127.0.0.1:4173/index.html",
                   .browserPath = "chrome",
                   .kioskMode = false,
                   .dryRun = true,
               },
               nullptr,
               &dryRunCommand)
               && !dryRunCommand.empty(),
           "Inspector browser launcher should support dry-run command capture");
}
#endif

} // namespace

namespace {

const ri::test::TestCase kCoreTests[] = {
    {"TestCommandLineParsing", TestCommandLineParsing},
    {"TestContentPresentation", TestContentPresentation},
    {"TestFrameArena", TestFrameArena},
    {"TestSpscRingBuffer", TestSpscRingBuffer},
    {"TestSpscRingBufferConcurrent", TestSpscRingBufferConcurrent},
    {"TestInputPollingBuffer", TestInputPollingBuffer},
    {"TestActionBindings", TestActionBindings},
    {"TestFixedStepAccumulator", TestFixedStepAccumulator},
    {"TestGameSimulationClock", TestGameSimulationClock},
    {"TestLevelScopedSchedulers", TestLevelScopedSchedulers},
    {"TestRuntimeCoreLifecycleAndHostAdapter", TestRuntimeCoreLifecycleAndHostAdapter},
    {"TestMainLoopSanitizesInvalidFixedDelta", TestMainLoopSanitizesInvalidFixedDelta},
    {"TestMainLoopSanitizesNonFiniteFixedDelta", TestMainLoopSanitizesNonFiniteFixedDelta},
    {"TestFixedStepAccumulatorSanitizesNonFiniteConfig", TestFixedStepAccumulatorSanitizesNonFiniteConfig},
    {"TestRenderCommandStream", TestRenderCommandStream},
    {"TestRenderCommandReaderRejectsOversizedPayload", TestRenderCommandReaderRejectsOversizedPayload},
    {"TestJsonWriteTextFileRoundTrip", TestJsonWriteTextFileRoundTrip},
    {"TestReadTextFileMissingPathReturnsEmpty", TestReadTextFileMissingPathReturnsEmpty},
    {"TestRenderRecorderStopsOnSinkFailure", TestRenderRecorderStopsOnSinkFailure},
    {"TestVulkanClearColorSanitizesNonFinite", TestVulkanClearColorSanitizesNonFinite},
    {"TestSpatialIndexMetrics", TestSpatialIndexMetrics},
    {"TestRenderSubmissionPlan", TestRenderSubmissionPlan},
    {"TestRenderRecorderExecution", TestRenderRecorderExecution},
    {"TestSceneRenderSubmission", TestSceneRenderSubmission},
    {"TestPhotoModeCamera", TestPhotoModeCamera},
    {"TestCameraConfinementVolume", TestCameraConfinementVolume},
    {"TestSceneHierarchy", TestSceneHierarchy},
    {"TestSceneReparentingBookkeeping", TestSceneReparentingBookkeeping},
    {"TestLevelObjectRegistryAndRuntimeMeshFactory", TestLevelObjectRegistryAndRuntimeMeshFactory},
    {"TestHelperBuilders", TestHelperBuilders},
    {"TestSceneDescriptionAndCounts", TestSceneDescriptionAndCounts},
    {"TestSceneUtilities", TestSceneUtilities},
    {"TestBoundsAndCameraUtilities", TestBoundsAndCameraUtilities},
    {"TestRaycastUtilities", TestRaycastUtilities},
    {"TestSceneMeshInstancePreviewRendering", TestSceneMeshInstancePreviewRendering},
    {"TestStarterSceneAnimation", TestStarterSceneAnimation},
    {"TestAnimationClipAndPlayer", TestAnimationClipAndPlayer},
    {"TestGltfTriangleImport", TestGltfTriangleImport},
    {"TestGltfRejectsOutOfRangeIndices", TestGltfRejectsOutOfRangeIndices},
    {"TestGltfMinimalGlb", TestGltfMinimalGlb},
    {"TestGltfTwoScenesIndex", TestGltfTwoScenesIndex},
    {"TestGltfNormalizedUbytePositions", TestGltfNormalizedUbytePositions},
    {"TestGltfPerspectiveCameraImport", TestGltfPerspectiveCameraImport},
    {"TestGltfMaterialFactorsImport", TestGltfMaterialFactorsImport},
    {"TestGltfSparsePositionImport", TestGltfSparsePositionImport},
    {"TestAddGltfModelNodeAppliesTransform", TestAddGltfModelNodeAppliesTransform},
    {"TestFbxImportScene", TestFbxImportScene},
    {"TestPsxWaterPackageNativeSceneProof", TestPsxWaterPackageNativeSceneProof},
    {"TestAddFbxModelNodeAppliesTransform", TestAddFbxModelNodeAppliesTransform},
    {"TestWavefrontObjMeshUsesLibraryImporter", TestWavefrontObjMeshUsesLibraryImporter},
    {"TestAddWavefrontObjNodePreservesCustomMaterialOptions", TestAddWavefrontObjNodePreservesCustomMaterialOptions},
    {"TestAddModelNodeDispatchesByExtension", TestAddModelNodeDispatchesByExtension},
    {"TestSceneKitMilestones", TestSceneKitMilestones},
    {"TestLocomotionTuning", TestLocomotionTuning},
    {"TestAdvancePlayerStamina", TestAdvancePlayerStamina},
    {"TestYawAngles", TestYawAngles},
#ifdef RAWIRON_WITH_DEV_INSPECTOR
    {"TestDevelopmentInspector", TestDevelopmentInspector},
#endif
    {"TestMovementControllerOptionalStamina", TestMovementControllerOptionalStamina},
    {"TestMovementControllerStep", TestMovementControllerStep},
    {"TestMovementControllerCoyoteBufferAndShortHop", TestMovementControllerCoyoteBufferAndShortHop},
    {"TestMovementControllerStanceAndSprint", TestMovementControllerStanceAndSprint},
    {"TestMovementControllerGroundProbeJump", TestMovementControllerGroundProbeJump},
    {"TestMovementControllerProtoMovementExtensions", TestMovementControllerProtoMovementExtensions},
    {"TestMovementControllerTraversalOptIn", TestMovementControllerTraversalOptIn},
    {"TestKinematicFallMultiplierAndTerminalVelocity", TestKinematicFallMultiplierAndTerminalVelocity},
    {"TestKinematicDurationSliceBudgetFlag", TestKinematicDurationSliceBudgetFlag},
    {"TestTraceSceneMetrics", TestTraceSceneMetrics},
    {"TestEntityPhysicsBatch", TestEntityPhysicsBatch},
    {"TestPickupHoldAndThrowObjectPhysics", TestPickupHoldAndThrowObjectPhysics},
    {"TestSceneEntityPhysicsApply", TestSceneEntityPhysicsApply},
};

} // namespace

int main(int argc, char** argv) {
    return ri::test::RunTestHarness(
        "RawIron.Core.Tests",
        std::span<const ri::test::TestCase>(kCoreTests),
        argc,
        argv);
}
