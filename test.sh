#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p .build/tests

CFLAGS=(-O2 -Wall -Wextra -Werror)
for name in recipe_guard menu_rename wb_messages; do
    clang "${CFLAGS[@]}" "hook/test_${name}.c" -o ".build/tests/${name}"
    ".build/tests/${name}"
done

if [[ "${SANITIZE:-1}" == 1 ]]; then
    SAN=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror)
    for name in recipe_guard menu_rename wb_messages; do
        clang "${SAN[@]}" "hook/test_${name}.c" -o ".build/tests/${name}_san"
        ".build/tests/${name}_san"
    done
fi

python3 -I scripts/recipe_config.py --check hook/nexhook-recipes.bin
python3 -I scripts/uxc_strings.py roundtrip hook/string_english.uxc
printf 'All tests passed.\n'
