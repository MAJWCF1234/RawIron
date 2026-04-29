#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [[ -x "build/dev-linux-clang/Apps/RawIron.VisualShell/RawIron.VisualShell" ]]; then
  exec "build/dev-linux-clang/Apps/RawIron.VisualShell/RawIron.VisualShell" "$@"
fi

if [[ -x "build/dev-clang/Apps/RawIron.VisualShell/RawIron.VisualShell" ]]; then
  exec "build/dev-clang/Apps/RawIron.VisualShell/RawIron.VisualShell" "$@"
fi

echo "RawIron.VisualShell was not found."
echo "Build the project first with CMake, then run this launcher again."
exit 1
