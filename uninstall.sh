#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
if [[ "${EUID}" -ne 0 ]]; then
    exec sudo -E "$0" "$@"
fi
cd "$ROOT"
export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-/opt/homebrew/lib:/usr/local/lib}"

cat <<'EOF'
This removes the preload environment and returns the next boot to stock.
Connect the camera in Mass Storage mode and eject its card volume in Finder.
When "OK to reset" appears later, press OK on the camera.
EOF
read -r -p "Continue? [y/N] " answer
if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
    echo "Cancelled."
    exit 1
fi

if [[ ! -x .venv/bin/python ]]; then
    echo "error: .venv is missing; run install.sh once first" >&2
    exit 1
fi
.venv/bin/python -W ignore scripts/cam_service_shell.py scripts/service_cmds/hook_undo.txt
.venv/bin/python -W ignore scripts/cam_readonly.py coldboot-stock
printf 'Stock boot restored. Add-on files left on /setting are inert.\n'
