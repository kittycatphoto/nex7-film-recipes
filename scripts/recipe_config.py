"""Build/check the NEX-7 recipe hook's fixed-size style mapping config.

The add-on reads /setting/nexhook-recipes.bin only in im.elf. A missing or invalid
config makes it pass all camera messages through unchanged. Changing this file does
not require rebuilding the add-on, but does require a cold boot to re-read it.

Format (164 bytes):
  +0  b'NXR3'
  +4  16 records, one for each possible core Creative Style byte
Each 10-byte record:
  style, base_style, saturation, hue, PP_LB, PP_CC, WB_mode, WB_temp, WB_LB, WB_CC
Signed fields use two's complement. Unmapped styles use their own base look, neutral
PP values, and WB mode 0xff (leave all WB fields unchanged). WB_LB/WB_CC are the
normal WB adjustment fields in the 0x400c command; PP_LB/PP_CC are separate Picture
Profile shifts. Colour recipes keep PP shifts at zero to avoid double adjustment.

  python3 -I scripts/recipe_config.py [OUTPUT]   # default hook/nexhook-recipes.bin
  python3 -I scripts/recipe_config.py --check [INPUT]
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGIC = b'NXR3'
RECORD_SIZE = 10
STYLE_COUNT = 16
SIZE = len(MAGIC) + RECORD_SIZE * STYLE_COUNT
DEFAULT_OUTPUT = os.path.join(ROOT, 'hook', 'nexhook-recipes.bin')

STYLE_NAMES = {
    0: 'Standard', 1: 'Vivid', 2: 'Neutral', 3: 'Portrait',
    4: 'Landscape', 5: 'B/W', 6: 'Clear', 7: 'Deep', 8: 'Light',
    9: 'Sunset', 10: 'Night', 11: 'Autumn', 12: 'Unused 12',
    13: 'Sepia', 14: 'Unused 14', 15: 'Unused 15',
}

# name, base Creative Style, PP (sat, hue, LB, CC), WB (mode, temp, adjust LB, adjust CC)
RECIPES = {
    'C1': ('Golden Cinema', 0, (1, 0, 0, 0), (10, 65, 1, 0)),
    'C2': ('Kanda Line', 0, (-2, 0, 0, 0), (10, 53, -1, 0)),
    'C3': ('Kodak Gold 200', 0, (2, 0, 0, 0), (10, 58, 3, 1)),
    'C4': ('Patina Pastel', 3, (-2, 0, 0, 0), (0, 0, 3, 0)),
    'C5': ('NPL 160 Tungsten', 0, (-1, 0, 0, 0), (10, 40, 0, 1)),
    'C6': ('Improved Velvia', 4, (1, 0, 0, 0), (10, 53, -1, 0)),
    'C7': ('Portra 400', 3, (0, 0, 0, 0), (10, 56, 2, 1)),
    'C8': ('Cinestill 800T', 0, (1, 0, 0, 0), (10, 34, 0, 1)),
    'C9': ('Ektachrome E100', 1, (0, 0, 0, 0), (10, 58, -2, 0)),
    'C10': ('Classic Neg / Superia', 0, (-1, 0, 0, 0), (10, 53, -1, -2)),
    'C11': ('Acros + Yellow', 5, (0, 0, 3, 0), (0, 0, 0, 0)),
    'C12': ('Tri-X Push', 5, (0, 0, 0, 0), (0, 0, 0, 0)),
}
STYLE_RECIPES = {
    1: 'C9',   # Vivid     -> Ektachrome E100
    2: 'C4',   # Neutral   -> Patina Pastel
    3: 'C7',   # Portrait  -> Portra 400
    4: 'C6',   # Landscape -> Improved Velvia
    5: 'C11',  # B/W       -> Acros + Yellow
    6: 'C3',   # Clear     -> Kodak Gold 200
    7: 'C5',   # Deep      -> NPL 160 Tungsten
    8: 'C2',   # Light     -> Kanda Line
    9: 'C1',   # Sunset    -> Golden Cinema
    10: 'C8',  # Night     -> Cinestill 800T
    11: 'C10', # Autumn    -> Classic Neg / Superia
    13: 'C12', # Sepia     -> Tri-X Push
}


def make_config():
    data = bytearray(MAGIC)
    for style in range(STYLE_COUNT):
        key = STYLE_RECIPES.get(style)
        if key:
            _, base, (sat, hue, pp_lb, pp_cc), (wb_mode, wb_temp, wb_lb, wb_cc) = RECIPES[key]
        else:
            base = style
            sat = hue = pp_lb = pp_cc = 0
            wb_mode, wb_temp, wb_lb, wb_cc = 0xff, 0, 0, 0
        data.extend((style, base, sat & 0xff, hue & 0xff, pp_lb & 0xff, pp_cc & 0xff,
                     wb_mode, wb_temp, wb_lb & 0xff, wb_cc & 0xff))
    result = bytes(data)
    assert len(result) == SIZE
    decode(result)
    return result


def signed(v):
    return v - 256 if v >= 128 else v


def decode(data):
    """Validate and return (style, base, sat, hue, pp_lb, pp_cc, mode, temp, lb, cc)."""
    if len(data) != SIZE:
        raise ValueError('config is %d bytes; expected exactly %d' % (len(data), SIZE))
    if data[:4] != MAGIC:
        raise ValueError('bad config magic/version')
    records = []
    for style in range(STYLE_COUNT):
        rec = data[4 + style * RECORD_SIZE:4 + (style + 1) * RECORD_SIZE]
        values = (rec[0], rec[1], *(signed(v) for v in rec[2:6]), *rec[6:8],
                  signed(rec[8]), signed(rec[9]))
        got_style, base, sat, hue, pp_lb, pp_cc, wb_mode, wb_temp, wb_lb, wb_cc = values
        if got_style != style or base >= STYLE_COUNT:
            raise ValueError('bad style/base in record %d' % style)
        if not -8 <= sat <= 7:
            raise ValueError('saturation out of range in record %d' % style)
        if not -7 <= hue <= 7:
            raise ValueError('hue out of range in record %d' % style)
        if not -9 <= pp_lb <= 9 or not -9 <= pp_cc <= 9:
            raise ValueError('PP WB shift out of range in record %d' % style)
        if wb_mode >= STYLE_COUNT and wb_mode != 0xff:
            raise ValueError('WB mode out of range in record %d' % style)
        if wb_temp > 100:
            raise ValueError('WB colour temperature out of range in record %d' % style)
        if not -9 <= wb_lb <= 9 or not -9 <= wb_cc <= 9:
            raise ValueError('WB adjustment out of range in record %d' % style)
        records.append(values)
    return records


def main():
    check = len(sys.argv) > 1 and sys.argv[1] == '--check'
    path = sys.argv[2] if check and len(sys.argv) > 2 else None
    if not check and len(sys.argv) > 1:
        path = sys.argv[1]
    path = path or DEFAULT_OUTPUT
    if check:
        with open(path, 'rb') as f:
            records = decode(f.read())
    else:
        data = make_config()
        with open(path, 'wb') as f:
            f.write(data)
        records = decode(data)
        print('wrote %s (%d bytes)' % (path, len(data)))
    print('%-4s %-21s %-11s %4s %4s %4s %4s %4s %4s %4s %4s' %
          ('idx', 'recipe', 'base style', 'sat', 'hue', 'PPLB', 'PPCC', 'WB', 'temp', 'WBLB', 'WBCC'))
    for style, base, sat, hue, pp_lb, pp_cc, wb_mode, wb_temp, wb_lb, wb_cc in records:
        key = STYLE_RECIPES.get(style)
        name = RECIPES[key][0] if key else '(neutral / user WB)'
        wb_text = 'keep' if wb_mode == 0xff else str(wb_mode)
        print('%-4d %-21s %-11s %+4d %+4d %+4d %+4d %4s %4d %+4d %+4d' %
              (style, name, STYLE_NAMES[base], sat, hue, pp_lb, pp_cc,
               wb_text, wb_temp, wb_lb, wb_cc))


if __name__ == '__main__':
    main()
