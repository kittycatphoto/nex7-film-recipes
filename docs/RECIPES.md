# Recipe map

Standard remains neutral and preserves the user's WB settings. The other 12 entries use the following
base looks and live WB commands. PP hue is zero for every recipe; colour recipes keep PP WB shift at zero
so it is not applied twice.

| Menu entry | Recipe | Base style | PP saturation | WB mode | Temp (100K) | WB LB | WB CC |
|---|---|---|---:|---:|---:|---:|---:|
| Vivid | Ektachrome E100 | Vivid | 0 | 10 C.Temp | 58 | -2 | 0 |
| Neutral | Patina Pastel | Portrait | -2 | 0 Auto | 0 | +3 | 0 |
| Portrait | Portra 400 | Portrait | 0 | 10 C.Temp | 56 | +2 | +1 |
| Landscape | Improved Velvia | Landscape | +1 | 10 C.Temp | 53 | -1 | 0 |
| B/W | Acros + Yellow | B/W | 0 | 0 Auto | 0 | 0 | 0; PP LB +3 for filter emulation |
| Clear | Kodak Gold 200 | Standard | +2 | 10 C.Temp | 58 | +3 | +1 |
| Deep | NPL 160 Tungsten | Standard | -1 | 10 C.Temp | 40 | 0 | +1 |
| Light | Kanda Line | Standard | -2 | 10 C.Temp | 53 | -1 | 0 |
| Sunset | Golden Cinema | Standard | +1 | 10 C.Temp | 65 | +1 | 0 |
| Night | Cinestill 800T | Standard | +1 | 10 C.Temp | 34 | 0 | +1 |
| Autumn | Classic Neg / Superia | Standard | -1 | 10 C.Temp | 53 | -1 | -2 |
| Sepia | Tri-X Push | B/W | 0 | 0 Auto | 0 | 0 | 0 |

Temperature is stored as an unsigned Kelvin/100 byte: 53 means 5300K. LB is amber(+)/blue(−), CC is
magenta(+)/green(−). The PP filter fields use the same signs.

Per-colour HSL, grain, exposure compensation, and the detailed tone settings are not applied by the hook.
The base style provides the starting look, while overall saturation, PP values, and WB commands provide
the recipe-specific adjustments.
