"""Dump / rewrite a NEX-7 menu string bank (share/app/string_<lang>.uxc).

Format (the 'flags 8' layout used by English and the other Latin/CJK banks):
  0x00  'uxc' 07, u32 0, u32 8 (flags; 0x0f = Arabic/Greek/Russian/Thai/Persian layout, not handled)
  0x0c  u32 packed: bits 0-11 = string count N, bits 12-31 = table length in u16 words (N + pad)
  0x10  u16 offset[N]: record i is at base + 4*offset[i], base = 0x10 + 2*table_words
  base  records, 4-byte aligned: u16 id (= index), 00, 1e, UTF-8 text, NUL, zero pad to 4
UI string ID 0x0200xxxx = record xxxx. No checksum or file size inside; strings may grow
(offsets are rebuilt) as long as every record starts within base + 4*0xffff.

  python3 -I scripts/uxc_strings.py dump BANK [ID ...]
  python3 -I scripts/uxc_strings.py roundtrip BANK
  python3 -I scripts/uxc_strings.py set BANK OUT ID=TEXT [ID=TEXT ...]    (ID in hex, e.g. 146=Velvia)
"""
import struct
import sys

MAGIC = b'uxc\x07'


def parse(data):
    if data[:4] != MAGIC or struct.unpack_from('<I', data, 8)[0] != 8:
        raise ValueError('not a flags-8 uxc string bank')
    packed = struct.unpack_from('<I', data, 12)[0]
    count, words = packed & 0xFFF, packed >> 12
    offsets = struct.unpack_from('<%dH' % count, data, 0x10)
    base = 0x10 + 2 * words
    strings = []
    for i, off in enumerate(offsets):
        p = base + 4 * off
        rid, tag = struct.unpack_from('<H2s', data, p)
        if rid != i or tag != b'\x00\x1e':
            raise ValueError('record %d at 0x%x: id %d tag %s' % (i, p, rid, tag.hex()))
        end = data.index(b'\0', p + 4)
        strings.append(data[p + 4:end].decode('utf-8'))
    return data[:0x10 + 2 * words], strings


def build(header, strings):
    base = len(header)
    out = bytearray(header)
    offsets = []
    for i, s in enumerate(strings):
        rel = len(out) - base
        if rel % 4 or rel // 4 > 0xFFFF:
            raise ValueError('record %d offset 0x%x out of range' % (i, rel))
        offsets.append(rel // 4)
        rec = struct.pack('<H2s', i, b'\x00\x1e') + s.encode('utf-8') + b'\0'
        out += rec + b'\0' * (-len(rec) % 4)
    struct.pack_into('<%dH' % len(offsets), out, 0x10, *offsets)
    return bytes(out)


def main():
    cmd, bank = sys.argv[1], sys.argv[2]
    data = open(bank, 'rb').read()
    header, strings = parse(data)
    if cmd == 'dump':
        ids = [int(x, 16) for x in sys.argv[3:]] or range(len(strings))
        for i in ids:
            print('%04x\t%s' % (i, strings[i].replace('\n', '\\n')))
    elif cmd == 'roundtrip':
        same = build(header, strings) == data
        print('%s: %d strings, rebuild %s' % (bank, len(strings), 'byte-identical' if same else 'DIFFERS'))
        sys.exit(0 if same else 1)
    elif cmd == 'set':
        out = sys.argv[3]
        for arg in sys.argv[4:]:
            k, v = arg.split('=', 1)
            i = int(k, 16)
            print('%04x  %r -> %r' % (i, strings[i], v))
            strings[i] = v
        new = build(header, strings)
        if parse(new)[1] != strings:
            sys.exit('rebuilt bank does not parse back')
        open(out, 'wb').write(new)
        print('wrote %s (%d -> %d bytes)' % (out, len(data), len(new)))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
