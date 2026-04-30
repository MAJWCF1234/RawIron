---
tags:
  - rawiron
  - engine
  - platform
  - linux
---

# Platform Support

## Official Platform Direction

RawIron is a desktop engine first.

The intended support order is:

1. Windows
2. Linux

Current non-targets:

- macOS
- iOS
- Android

That is not a moral judgment. It is a scope decision.

## What "Windows + Linux" Means

Supporting Linux naturally means the engine should avoid accidental Windows lock-in in its core architecture.

That means:

- platform code stays behind platform seams
- core runtime libraries stay portable by default
- build tooling should remain CMake and Ninja friendly
- renderer backends should not assume one OS owns the engine
- desktop platform work should aim for shared Windows/Linux solutions where practical

## Current Reality

RawIron is still being built primarily on Windows right now, but the architecture is being steered toward desktop portability.

Current direction:

- core engine systems are in native C++
- Vulkan is the first renderer backend
- Windows has the most verified runtime coverage today
- Linux is a planned desktop target, not an afterthought

## Current Native Seams

The important platform rule is:

- `RawIron.Core` should not become a Win32 dumping ground

Platform-sensitive behavior should live in places like:

- platform layer
- renderer backend integration
- executable host startup paths
- build and packaging scripts

## Vulkan And Linux

The Vulkan backend should remain desktop-portable.

Current state:

- Windows surface initialization is live
- Linux loader/surface compatibility is in active scope
- full Linux window/surface integration should land through the future shared desktop platform layer instead of one-off hacks

That keeps Linux support honest without pretending the engine is already fully shippable there.

## Build And Tooling Rule

RawIron should prefer tools and workflows that fit both Windows and Linux naturally:

- CMake
- Ninja
- Clang and GCC-friendly code
- Vulkan tooling
- minimal OS-specific assumptions in project layout

## Scope Guardrails

To keep the engine focused:

- do not let Apple/mobile concerns distort the first desktop architecture
- do not accept Win32-only shortcuts in core systems
- do not promise platform parity before the platform seams are real

## Practical Consequence

When adding a new subsystem, ask:

- does this belong in portable engine code
- does this belong behind a desktop platform seam
- or is this a Windows-only startup path that needs to stay contained

If we keep answering that honestly, Linux support stays natural instead of becoming a painful rescue effort.
