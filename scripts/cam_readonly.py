"""Read-only NEX-7 session over PMCA-RE's updater shell. Writes nothing to the camera.

Boots the camera into firmware-updater mode with PMCA-RE's CXD4132 shell payload
(the main firmware is not flashed), then over USB:
  - dumps the whole settings store (Backup_read_data) to camera/backup-<ts>.bin
  - reads a few known setting IDs individually
  - dumps the boot loaders and boot ROM (for the bad-flash recovery question)
  - sends EXIT; the camera then reports the "update" as finished.

  sudo env DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib .venv/bin/python -W ignore \
      scripts/cam_readonly.py [LABEL]
"""
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'Sony-PMCA-RE'))

from pmca.commands.usb import importDriver, listDevices, updaterShellCommand  # noqa: E402
from pmca.usb.sony import SonyUpdaterDevice  # noqa: E402
from pmca.platform import PropertyInterface  # noqa: E402
from pmca.platform.backend.usb import UsbPlatformBackend  # noqa: E402

OUT = os.path.join(ROOT, 'camera')
IDS = {
    0x010700C1: 'ISO (current)',
    0x0107014C: 'ISO AUTO MAX',
    0x010706CB: '? (changed with ISO)',
    0x0107031C: 'PP_NO',
    0x01070248: 'PP1 COLORMODE',
    0x01070298: 'PP1 GAMMA',
    0x010701D0: 'PP1 BLACKGAMMA_LVL',
}


def save(name, data):
    path = os.path.join(OUT, name)
    with open(path, 'wb') as f:
        f.write(data)
    print('  saved %s (%d bytes)' % (path, len(data)))


LABEL = sys.argv[1] if len(sys.argv) > 1 else ''
ran = []


def wait_for_updater(seconds=180):
    """PMCA-RE gives up after 30 s; pressing OK on the camera often takes longer."""
    print('\nWaiting up to %d s for the camera to reconnect in updater mode (press OK on it)...' % seconds)
    with importDriver('libusb') as driver:
        for _ in range(seconds * 2):
            try:
                if any(isinstance(d, SonyUpdaterDevice) for d in listDevices(driver, True)):
                    return True
            except Exception:
                pass
            time.sleep(.5)
    return False


def session(device):
    ran.append(True)
    stamp = time.strftime('%Y%m%d-%H%M%S') + ('-' + LABEL if LABEL else '')
    backend = UsbPlatformBackend(device)
    backend.start()
    try:
        print('\n== Device properties')
        for _id, desc, value in PropertyInterface(backend).getProps():
            print('  %-20s%s' % (desc + ':', value))

        print('\n== Settings store')
        save('backup-%s.bin' % stamp, backend.getBackupData())

        print('\n== Individual IDs')
        for pid, desc in IDS.items():
            try:
                print('  0x%08x %-20s %s' % (pid, desc, backend.readBackup(pid).hex(' ')))
            except Exception as e:  # unknown ID on this model is fine
                print('  0x%08x %-20s error: %s' % (pid, desc, e))

        print('\n== Boot loaders / boot ROM')
        for i, data in enumerate(backend.readBootloader()):
            save('boot%d-%s.bin' % (i + 1, stamp), data)
        try:
            import io
            buf = io.BytesIO()
            backend.readMemory(0xFFFF0000, 0x6000, buf)
            save('bootrom-%s.bin' % stamp, buf.getvalue())
        except Exception as e:
            print('  bootrom read failed: %s' % e)
    finally:
        print('\n== Exiting updater shell')
        backend.stop()
    print('Done. The camera may show the update as complete; power-cycle it (battery out/in).')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    updaterShellCommand(model='NEX-7', driverName='libusb', complete=session)
    if not ran and wait_for_updater():
        updaterShellCommand(model='NEX-7', driverName='libusb', complete=session)
