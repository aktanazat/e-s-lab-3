# IR Remote Button Codes - TV Code 1003

## Protocol Details
- **Device Address (bits 0-31):** `0x40040100` (same for all buttons)
- **Command Code (bits 32-47):** varies by button
- **Total bits:** 48
- **Leader pulse:** ~3400us LOW, ~1600us HIGH
- **Bit encoding:** Short space (~400us) = 0, Long space (~1200us) = 1

## Button Code Mapping

| Button | Full Code (48 bits) | Command Code (bits 32-47) |
|--------|---------------------|---------------------------|
| 0 | `0x40040100 98990000` | `0x98990000` |
| 1 | `0x40040100 08090000` | `0x08090000` |
| 2 | `0x40040100 88890000` | `0x88890000` |
| 3 | `0x40040100 48490000` | `0x48490000` |
| 4 | `0x40040100 C8C90000` | `0xC8C90000` |
| 5 | `0x40040100 28290000` | `0x28290000` |
| 6 | `0x40040100 A8A90000` | `0xA8A90000` |
| 7 | `0x40040100 68690000` | `0x68690000` |
| 8 | `0x40040100 E8E90000` | `0xE8E90000` |
| 9 | `0x40040100 18190000` | `0x18190000` |
| MUTE (Send) | `0x40040100 4C4D0000` | `0x4C4D0000` |
| LAST (Delete) | `0x40040100 ECED0000` | `0xECED0000` |

## Multi-tap Character Mapping

| Button | Characters |
|--------|------------|
| 0 | (space) |
| 1 | (unused) |
| 2 | a, b, c |
| 3 | d, e, f |
| 4 | g, h, i |
| 5 | j, k, l |
| 6 | m, n, o |
| 7 | p, q, r, s |
| 8 | t, u, v |
| 9 | w, x, y, z |
| MUTE | Send message |
| LAST | Delete character |
