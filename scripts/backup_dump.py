"""Read-only dumper for Sony Backup.bin (BK2/BK4) settings images.

Format follows ma1co/Sony-PMCA-RE pmca/backup/__init__.py. Never writes.

  python3 backup_dump.py FILE                 # list every property
  python3 backup_dump.py FILE --ids 0x003c0373 0x01070148
  python3 backup_dump.py --diff A B           # properties whose value differs
"""
import argparse
import struct

KNOWN = {
    0x00E70000: 'modelCode',
    0x003E0005: 'modelName',
    0x003C0373: 'recLimit[h]',
    0x003C0374: 'recLimit[m]',
    0x003C0375: 'recLimit[s]',
    0x003C04B6: 'recLimit4k',
    0x01070148: 'palNtscSelector',
    0x01640001: 'usbAppInstaller',
    0x01660024: 'androidPlatformVersion',
}
for i in range(35):
    KNOWN[0x010D008F + i] = 'language[%d]' % i


class Backup:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        d = self.data
        (self.magic, _cookie, _wc, rev, self.nSub, self.nProp,
         self.dataOff, self.dataSize, self.checksum) = struct.unpack_from('<III4sIIIII', d, 0)
        if rev[:2] != b'BK' or rev[3:] != b'\0':
            raise ValueError('not a backup file')
        self.rev = int(rev[2:3])
        self.size = self.dataOff + self.dataSize
        calc = sum(d[:0x20] + d[0x24:self.size]) & 0xFFFFFFFF
        self.checksumOk = calc == self.checksum
        self.region = d[0xE0:0x100].rstrip(b'\0').decode('latin1')
        self.version = d[0x40:0x60].rstrip(b'\0').decode('latin1')
        self.subOff = 0x100
        self.entSize = 6 if self.rev >= 4 else 5
        self.propOff = self.subOff + self.nSub * 6

    def _sub(self, i):
        return struct.unpack_from('<HI', self.data, self.subOff + i * 6)

    def _entry(self, idx):
        off = self.propOff + idx * self.entSize
        if self.rev >= 4:
            return struct.unpack_from('<HI', self.data, off)
        return struct.unpack_from('<BI', self.data, off)

    def get(self, pid):
        if (pid >> 16) >= self.nSub:
            raise KeyError(hex(pid))
        n, base = self._sub(pid >> 16)
        if (pid & 0xFFFF) >= n:
            raise KeyError(hex(pid))
        attr, ptr = self._entry(base + (pid & 0xFFFF))
        if ptr == 0xFFFFFFFF:
            raise KeyError(hex(pid))
        size, off = ptr >> 24, ptr & 0xFFFFFF
        maxSize = size
        if size == 0xFF:
            size = maxSize = struct.unpack_from('<H', self.data, off)[0]
            off += 2
        elif size == 0:
            size, maxSize = struct.unpack_from('<HH', self.data, off)
            off += 4
        value = self.data[off:off + size]
        reset = self.data[off + maxSize:off + maxSize + size] if attr & 0x74 else None
        return attr, value, reset

    def props(self):
        for s in range(self.nSub):
            n, _ = self._sub(s)
            for j in range(n):
                pid = s << 16 | j
                try:
                    yield pid, self.get(pid)
                except KeyError:
                    pass


def fmt(pid, attr, value, reset):
    v = value.hex() if len(value) <= 24 else value[:24].hex() + '...(%d B)' % len(value)
    r = '' if reset is None or reset == value else '  reset=' + reset[:24].hex()
    return '0x%08x attr=%02x %-22s %s%s' % (pid, attr, KNOWN.get(pid, ''), v, r)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--ids', nargs='*', type=lambda s: int(s, 0))
    ap.add_argument('--diff', action='store_true')
    a = ap.parse_args()
    bs = [Backup(p) for p in a.files]
    for p, b in zip(a.files, bs):
        print('# %s rev=BK%d region=%s version=%s subsystems=%d props=%d checksum=%s'
              % (p.split('/')[-1], b.rev, b.region, b.version, b.nSub, b.nProp,
                 'ok' if b.checksumOk else 'BAD'))
    if a.diff:
        A, B = dict(bs[0].props()), dict(bs[1].props())
        for pid in sorted(set(A) | set(B)):
            va = A.get(pid, (0, b'<missing>', None))[1]
            vb = B.get(pid, (0, b'<missing>', None))[1]
            if va != vb:
                print('0x%08x %-22s %s -> %s' % (pid, KNOWN.get(pid, ''), va[:24].hex(), vb[:24].hex()))
        return
    b = bs[0]
    if a.ids:
        for pid in a.ids:
            try:
                print(fmt(pid, *b.get(pid)))
            except KeyError:
                print('0x%08x %-22s <not present>' % (pid, KNOWN.get(pid, '')))
        return
    for pid, (attr, value, reset) in b.props():
        print(fmt(pid, attr, value, reset))


if __name__ == '__main__':
    main()
