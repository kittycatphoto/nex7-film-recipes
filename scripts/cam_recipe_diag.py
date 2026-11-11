"""Read-only recipe-hook diagnostics over service mode (no settings writes).

  sudo env DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib .venv/bin/python -W ignore \
      scripts/cam_recipe_diag.py
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'Sony-PMCA-RE'))
from pmca.commands.usb import senserShellCommand  # noqa: E402

OUT = os.path.join(ROOT, 'camera', 'service', 'recipe_generation.txt')


def main():
    def go(cam):
        lines = []
        value = cam.readBackup(0x00940004)
        lines.append('WBI count: %s (%d)' % (value.hex(), int.from_bytes(value, 'little')))
        for label, pid in (
            ('WB mode', 0x01070019), ('WB adjust LB', 0x01070017),
            ('WB adjust CC', 0x01070016), ('WB color temp', 0x01070018),
            ('WB daylight level', 0x0107035d)):
            value = cam.readBackup(pid)
            lines.append('%s: %s (%d)' % (label, value.hex(), int.from_bytes(value, 'little', signed=True)))
        for path in ('/tmp/nexhook.guardgen', '/tmp/nexhook-style.state'):
            try:
                data = cam.readFile(path)
                lines.append('%s: %s' % (path, data.hex()))
            except Exception as e:
                lines.append('%s: %s' % (path, type(e).__name__))
        try:
            lines.append('recipe log:\n' + cam.readFile('/setting/nexhook-recipe.log').decode('latin1'))
        except Exception as e:
            lines.append('recipe log: %s' % type(e).__name__)
        text = '\n'.join(lines) + '\n'
        print(text, end='')
        with open(OUT, 'w') as f:
            f.write(text)

    senserShellCommand(driverName='libusb', complete=go)


if __name__ == '__main__':
    main()
