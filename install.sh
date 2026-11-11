#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
if [[ "${EUID}" -ne 0 ]]; then
    exec sudo -E "$0" "$@"
fi
cd "$ROOT"

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    echo "error: Python 3 is required (https://www.python.org/downloads/macos/)" >&2
    exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
    echo "error: Homebrew is required for libusb (https://brew.sh)" >&2
    exit 1
fi
if [[ ! -e /opt/homebrew/lib/libusb-1.0.dylib && ! -e /usr/local/lib/libusb-1.0.dylib ]]; then
    echo "Installing libusb with Homebrew..."
    brew install libusb
fi
if [[ ! -x .venv/bin/python ]]; then
    "$PYTHON" -m venv .venv
fi
.venv/bin/python -m pip install --disable-pip-version-check -q -r tools/Sony-PMCA-RE/requirements.txt

export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-/opt/homebrew/lib:/usr/local/lib}"

cat <<'EOF'
Camera setup:
  1. Set USB Connection to Mass Storage.
  2. Connect the camera to this Mac.
  3. Eject the SD-card volume in Finder, but leave USB connected.

No firmware is flashed. Stage A runs first; installation happens only after it passes.
EOF
read -r -p "Continue? [y/N] " answer
if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
    echo "Cancelled."
    exit 1
fi

.venv/bin/python -W ignore scripts/cam_service_shell.py \
    --put=hook/nexhook-recipe.so:/tmp/nexhook-recipe.so \
    scripts/service_cmds/recipe_stageA_test.txt
if ! grep -q 'init inactive' camera/service/recipe_stageA_test.txt; then
    echo "error: Stage A did not produce the expected inactive-process record" >&2
    exit 1
fi

.venv/bin/python -W ignore scripts/cam_service_shell.py \
    --put=hook/nexhook-recipe.so:/setting/nexhook-recipe-v17.so \
    --put=hook/nexhook-recipes.bin:/setting/nexhook-recipes.bin \
    --put=hook/string_english.uxc:/setting/string_english.uxc \
    scripts/service_cmds/recipe_stageB17_all12.txt

cat <<'EOF'
The camera will now enter read-only updater mode to force a cold boot.
When it displays "OK to reset", press OK on the camera.
EOF
.venv/bin/python -W ignore scripts/cam_readonly.py coldboot-recipe-v17-all12

cat <<'EOF'
Installed. Open Creative Style and choose a recipe.
Standard remains neutral. The shooting-screen icon is unchanged.
To return to stock, run ./uninstall.sh on this Mac.
EOF
