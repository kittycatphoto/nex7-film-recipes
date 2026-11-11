"""Run a fixed list of shell commands on the NEX-7's live system over service mode.

Uses the root BusyBox console that service ("senser") mode exposes over USB. Commands come
from a text file, one per line, and output is saved to camera/service/<name>.txt.
Only put read-only commands in the file; this script doesn't filter them.

  sudo env DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib .venv/bin/python -W ignore \
      scripts/cam_service_shell.py scripts/service_cmds/explore1.txt
"""
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'Sony-PMCA-RE'))

from pmca.commands.usb import senserShellCommand  # noqa: E402

OUT = os.path.join(ROOT, 'camera', 'service')
MARK = '__CMD_DONE__'


TMP_OUT = '/tmp/_svc.out'


def run(cam, cmd, timeout=30):
    """The USB console lags and mixes output between sessions, so each command writes its
    output to a small file on the camera, which the service-mode file read fetches reliably."""
    # split the marker with "" so nothing but the final echo can produce it
    line = '( %s ) > %s 2>&1; echo "rc=$?" >> %s; echo %s""%s >> %s\n' % (
        cmd, TMP_OUT, TMP_OUT, MARK[:5], MARK[5:], TMP_OUT)
    cam.dev.writeTerminal(line.encode('latin1'))
    end = time.time() + timeout
    while time.time() < end:
        cam.dev.readTerminal()
        time.sleep(0.5)
        try:
            out = cam.readFile(TMP_OUT).decode('latin1')
        except Exception:
            continue
        if MARK in out:
            return out.replace(MARK + '\n', '')
    return '(timed out)\n'


def session(cmds, name):
    def go(cam):
        cam.setTerminalEnable(True)
        # drain whatever an earlier session left queued on the console
        end = time.time() + 3
        while time.time() < end:
            cam.dev.readTerminal()
        out = []
        try:
            for cmd in cmds:
                try:
                    cam.deleteFile(TMP_OUT)  # so a stale result can't be read back
                except Exception:
                    pass
                text = run(cam, cmd)
                out.append('### %s\n%s\n' % (cmd, text))
                print('### %s\n%s' % (cmd, text))
            if HOLD:
                # leaving service mode makes the app re-send every setting, undoing injections,
                # so let the user shoot while we're still in service mode
                input('\n>>> Service mode is held open. Shoot now, then press Enter here to exit... ')
        finally:
            try:
                cam.deleteFile(TMP_OUT)
            except Exception:
                pass
            cam.setTerminalEnable(False)
            path = os.path.join(OUT, name + '.txt')
            with open(path, 'w') as f:
                f.write(''.join(out))
            print('saved', path)
    return go


HOLD = '--hold' in sys.argv
# --put LOCAL:REMOTE uploads a file over the service-mode file write before the commands run
PUTS = [a.split('=', 1)[1].split(':', 1) for a in sys.argv[1:] if a.startswith('--put=')]


def with_uploads(go):
    def run_all(cam):
        for local, remote in PUTS:
            with open(local, 'rb') as f:
                data = f.read()
            cam.writeFile(remote, data)
            print('uploaded %s -> %s (%d bytes)' % (local, remote, len(data)))
        go(cam)
    return run_all


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    src = [a for a in sys.argv[1:] if not a.startswith('--')][0]
    cmds = [l.strip() for l in open(src) if l.strip() and not l.startswith('#')]
    name = os.path.splitext(os.path.basename(src))[0]
    senserShellCommand(driverName='libusb', complete=with_uploads(session(cmds, name)))
