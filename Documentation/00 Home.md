# RawIron

RawIron is a native C++20 game engine and game workspace built around a shared runtime core, Vulkan rendering, data-authored game projects, and in-repo shipping tools.

## Start Here

- [[01 Product/RawIron|What RawIron is]]
- [[02 Engine/00 Engine Home|Engine overview]]
- [[03 Projects/00 Projects Home|Game projects]]
- [[04 Pipeline/00 Pipeline Home|Build, test, and release pipeline]]

## Current Workspace Shape

- `Source/` contains the engine libraries.
- `Apps/` contains runtime hosts, editor-facing tools, and multiplayer utilities.
- `Games/` contains shippable game projects and game-specific runtime modules.
- `Assets/` contains the workspace asset corpus that ships with the workspace.
- `Installer/` contains the full-workspace installer that reassembles split release parts.
- `Scripts/` contains publishing, sync, and build helper scripts.

## Core Ideas

- Runtime lifecycle belongs to `RawIron.Runtime`.
- Games supply authored data and runtime modules, not private engine stacks.
- Config ownership is engine-enforced through shared contracts.
- Multiplayer is a first-class engine surface with dedicated, listen, hybrid, and client flows.
- Plugins, policies, manifests, and hook files are part of every game contract.
- GitHub releases are shipped as a split full-workspace archive plus installer.
