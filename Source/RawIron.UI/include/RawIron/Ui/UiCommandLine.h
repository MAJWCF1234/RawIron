#pragma once

namespace ri::core {
class CommandLine;
}

namespace ri::ui {

/// When `--validate-ui-manifest` is present, loads `Assets/UI/default_menu.ui.json` under `--workspace`
/// (or current directory) unless `--ui-manifest` overrides. Logs OK / failure; no-op otherwise.
void MaybeValidateUiManifestFromCommandLine(const ri::core::CommandLine& commandLine);

} // namespace ri::ui
