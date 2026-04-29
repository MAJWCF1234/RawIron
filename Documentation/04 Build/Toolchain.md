---
tags:
  - rawiron
  - build
  - toolchain
---

# Toolchain and Build Environment

## Build Space

- root: repository checkout root

## Installed / Verified

- Microsoft Visual C++ toolchain via Visual Studio 2022
- CMake
- Ninja
- LLVM / Clang
- Windows SDK

## Desktop Target Direction

RawIron is being shaped for:

- Windows
- Linux

Not current targets:

- macOS
- iOS
- Android

## Current Working Reality

The machine is already capable of native engine development.

Verified capabilities:

- C compiler works
- C++ compilation works
- CMake project generation works
- Ninja build works
- Clang++ build works

Current preset rule:

- CMake presets now pin both `C` and `C++` compilers so mixed-language dependencies like `cgltf` regenerate cleanly under both MSVC and Clang

## Helper Command

A convenience launcher exists at:

- `C:\Users\majwc\AppData\Local\Tools\bin\cpp-dev.cmd`

Purpose:

- initializes the MSVC build environment
- adds LLVM and CMake to PATH
- opens a ready-to-use PowerShell session for C/C++ work

## Direction

RawIron should use a native build and tool flow from the beginning.

No Electron dependency should remain in the core development path.
The build shape should stay friendly to both Windows and Linux instead of depending on one workstation forever.
