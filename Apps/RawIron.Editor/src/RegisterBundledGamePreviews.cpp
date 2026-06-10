#include "RawIron/Editor/BundledGamePreviews.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"

#if defined(RAWIRON_EDITOR_BUNDLE_LIMINAL)
#include "RawIron/Games/LiminalHall/LiminalHallEditorPreview.h"
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_WILDERNESS)
#include "RawIron/Games/ForestRuins/ForestRuinsEditorPreview.h"
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
}

} // namespace ri::editor
