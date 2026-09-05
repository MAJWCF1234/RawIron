# Bug review and validation — 2026-09-05

Reviewed the pending engine, shared desktop/VR interaction, procedural geometry,
rendering, dependency relocation, and Cube Test changes in the working tree.

## Fixes from this review

- Reject ray and projectile direction vectors whose squared length overflows.
  Previously a finite but extreme vector normalized to zero: a ray could select
  the object containing its origin, and emission could consume a pool slot with
  zero velocity. A standalone reproduction returned index 0 for both before the
  fix and rejection (-1) for both afterward.
- Reject non-finite physics settings before computing substep counts or updating
  props. This prevents an unsafe float-to-integer conversion for a NaN step size.
- Center oversized props along axes where they cannot fit, clearing velocity on
  those axes rather than supplying reversed bounds to `std::clamp`. Skip movement
  for props with non-finite extents.
- Added regression coverage to `InteractivePropFieldSmoke.cpp`.

## Validation

Fresh Windows MSVC x64 RelWithDebInfo build, with tests, tools, and real ENet enabled:

```powershell
cmake --preset dev-msvc -B build/validation-msvc -DRAWIRON_BUILD_TESTS=ON -DRAWIRON_BUILD_TOOLS=ON
cmake --build build/validation-msvc --config RelWithDebInfo --parallel 8
ctest --test-dir build/validation-msvc -C RelWithDebInfo --output-on-failure --timeout 180
```

- Complete workspace build passed.
- All 159 registered CTests passed (66.35 seconds).
- UiMenu `--workspace=E:/RawIron --headless` passed.
- `Scripts/Test-NormalMapping.ps1 -BuildDirectory build/validation-msvc`: all 16
  GPU comparisons passed, each with mean absolute pixel error 0.
- `Scripts/Test-MaterialCalibration.ps1 -BuildDirectory build/validation-msvc`:
  all six GPU checks passed, including static capture repeatability and shadows.
- Focused MSVC AddressSanitizer compilation and execution of the ray/emission
  overflow reproduction passed without a sanitizer report.
- All 13 project reference assets matched their recorded sizes and SHA-256 hashes.
- All four new PowerShell scripts parsed successfully; `git diff --check` passed.

The pre-existing `build/dev-msvc` cache referenced the former O: source location;
the fresh validation directory avoids using stale binaries. Optional loose engine
textures were absent, as documented by the cooked-package workflow. Physical VR
headset interaction, EOS, and the complete sanitizer CI matrix were not exercised
in this review. These checks do not establish that the entire engine is bug-free.
