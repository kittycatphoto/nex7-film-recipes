#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

CC="${CC:-clang}"
if [[ -n "${LLD:-}" ]]; then
    LLD_BIN="$LLD"
elif [[ -x /opt/homebrew/bin/ld.lld ]]; then
    LLD_BIN=/opt/homebrew/bin/ld.lld
else
    LLD_BIN="$(command -v ld.lld)"
fi
SONYDL=firmware/libsonydl.so
if [[ ! -f "$SONYDL" ]]; then
    echo "error: firmware libsonydl.so is missing at $SONYDL" >&2
    exit 1
fi

COMMON=(--target=armv6k-none-linux-gnueabi -marm -mfloat-abi=soft
        -fPIC -ffreestanding -fno-builtin -O2 -Wall -Wextra -Werror)
"$CC" "${COMMON[@]}" -c hook/recipe_hook.c -o hook/recipe_hook.o
"$CC" "${COMMON[@]}" -c hook/menu_rename.c -o hook/menu_rename.o
"$LLD_BIN" -shared --hash-style=sysv -init=nexhook_init --no-rosegment -z norelro \
    -soname nexhook.so -o hook/nexhook-recipe.so \
    hook/recipe_hook.o hook/menu_rename.o "$SONYDL"

python3 -I scripts/recipe_config.py
python3 -I scripts/uxc_strings.py roundtrip hook/string_english.uxc
