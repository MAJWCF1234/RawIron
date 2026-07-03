#include "RawIron/Editor/BundledGamePreviews.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"

#if defined(RAWIRON_EDITOR_BUNDLE_LIMINAL)
#include "RawIron/Games/LiminalHall/LiminalHallEditorPreview.h"
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_WILDERNESS)
#include "RawIron/Games/ForestRuins/ForestRuinsEditorPreview.h"
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_MULTIPLAYER_SANDBOX)
#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxEditorPreview.h"
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_CUBE_TEST)
#include "RawIron/Games/CubeTest/CubeTestEditorPreview.h"
#endif

#include <filesystem>

namespace ri::editor {

void RegisterBundledGameEditorPreviews() {
#if defined(RAWIRON_EDITOR_BUNDLE_LIMINAL)
    ri::games::liminal::RegisterLiminalHallEditorPreview();
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_WILDERNESS)
    ri::games::forestruins::RegisterForestRuinsEditorPreview();
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_MULTIPLAYER_SANDBOX)
    ri::games::multiplayersandbox::RegisterMultiplayerSandboxEditorPreview();
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_CUBE_TEST)
    ri::games::cubetest::RegisterCubeTestEditorPreview();
#endif
}

} // namespace ri::editor
