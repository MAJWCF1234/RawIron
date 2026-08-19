# Architecture boundaries

RawIron protects its stable foundation at CMake configure time. The policy is deliberately
narrow: it freezes the dependency direction of low-level modules while allowing higher-level
World, Content, rendering, editor, and game composition to continue evolving.

The protected dependency edges are:

- `RawIron.Core` and `RawIron.Audio`: no RawIron engine-library dependencies
- `RawIron.Runtime`, `RawIron.Logic`, `RawIron.Spatial`, `RawIron.Structural`, and `RawIron.UI`: Core only
- `RawIron.Events`: Runtime only
- `RawIron.Trace`: Core, Runtime, and Spatial
- `RawIron.Validation`: Events and Structural

Core source may include only Core-owned `Core`, `Math`, `Render`, and `Scene` headers.
Runtime source may additionally include Runtime headers. A forbidden direct link or upward
include stops configuration with the file or target that violated the boundary.

This is an architectural firewall, not a declaration that higher layers are finished.
Composition modules are intentionally excluded until their APIs stabilize. Promote a new edge
into the protected policy only when its ownership and lifecycle are clear; do not weaken a rule
to make a convenient dependency compile.

`RawIron.Architecture.FoundationPolicy` exercises the policy and scans the live source tree.
The same checks also run during every normal CMake configuration, so violations fail before a
long build begins.
