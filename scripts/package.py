"""Create the self-contained installer bundle used by the web configurator."""
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'downloads' / 'nex7-base-installer.zip'
FILES = [
    'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md',
    'install.sh', 'uninstall.sh', 'build.sh', 'test.sh',
    'docs/INSTALL.md', 'docs/RECIPES.md',
    'hook/recipe_hook.c', 'hook/menu_rename.c', 'hook/nexhook-recipe.so',
    'hook/nexhook-recipes.bin', 'hook/string_english.uxc',
    'hook/test_recipe_guard.c', 'hook/test_menu_rename.c', 'hook/test_wb_messages.c',
    'scripts/recipe_config.py', 'scripts/uxc_strings.py',
    'scripts/cam_service_shell.py', 'scripts/cam_readonly.py',
    'scripts/cam_recipe_diag.py', 'scripts/backup_dump.py',
    'scripts/service_cmds/recipe_stageA_test.txt',
    'scripts/service_cmds/recipe_stageB17_all12.txt',
    'scripts/service_cmds/hook_undo.txt',
]


def add(zf, rel):
    path = ROOT / rel
    if not path.is_file():
        raise FileNotFoundError(path)
    info = zipfile.ZipInfo(rel, date_time=(2026, 11, 11, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (0o755 if rel.endswith('.sh') else 0o644) << 16
    zf.writestr(info, path.read_bytes())


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUT, 'w') as zf:
        for rel in FILES:
            add(zf, rel)
        for path in sorted((ROOT / 'tools' / 'Sony-PMCA-RE').rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts and '.git' not in path.parts:
                add(zf, str(path.relative_to(ROOT)))
    print('wrote %s (%d bytes)' % (OUT, OUT.stat().st_size))


if __name__ == '__main__':
    main()
