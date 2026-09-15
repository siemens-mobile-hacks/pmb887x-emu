# Siemens fullflash key recalculation algorithm

This document describes what "Recalc Fullflash" in **x65PapuaUtils v1.1.1c** (by a1ex) actually
does to a fullflash. The algorithm was re-implemented in `src/siemens/recalc.cpp`. The emulator
applies it when `--siemens-recalc` is requested (see
[recalc-siemens-fullflash.md](recalc-siemens-fullflash.md)).

The implementation was verified against fullflashes recalculated by the original tool:
for the same IMEI / ESN / SKEY the output is byte-identical (S75 x75, EL71 x85).

All multi-byte integers below are little-endian.

## 1. Inputs

| Input | Meaning | Emulator default |
|-------|---------|------------------|
| IMEI  | 15 decimal digits | `490154203237518` |
| ESN   | 32-bit NOR flash serial number, given as 8 hex digits | `12345678` |
| SKEY  | "service key", up to 8 **decimal** digits, used as a 32-bit integer | `12345678` (fixed) |
| Master codes | six 32-bit integers, the phone master codes | `12345678` each (PapuaUtils default) |
| VerDown | one byte stored in EEPROM block 52, the "VerDown" setting of PapuaUtils | `1` |

Note that ESN is parsed as **hex** (`$12345678`) while SKEY is parsed as **decimal**
(`12345678` = `0x00BC614E`).

## 2. Key derivation: BKEY and HASH

```
block[0..3]  = ESN  (u32 LE)
block[4..7]  = SKEY (u32 LE)
for i in 0..7:
    block[8+i] = block[i] ^ block[i+3]      # computed in place, so bytes 13..15 use the new bytes 8..10
BKEY = MD5(block)          # 16 bytes
HASH = MD5(BKEY)           # 16 bytes
```

PapuaUtils does this with a single hand-padded MD5 transform, which is equivalent to a normal
MD5 of the 16-byte message.

Example: ESN `12345678`, SKEY `12345678` gives
BKEY `2062670E09537A7FE079462A0EC98168`, HASH `54F80AC12ACD94B2F5CFFB9BF7E4D493`.

## 3. Fullflash layout detection

The bootcore must contain magic `43 4A 4B 54` at `0x3C` and a supported interface header:

| | x65 / x75 (SGold) | x85 (SGold2: EL71, E71, M81, S68, C81, ...) |
|---|---|---|
| Detection | `00 02 4C 53` or `02 02 4C 53` at `0x200` | `00 03 4C 53` at `0x1200` |
| Erase block size | 0x20000 | 0x40000 |
| Block signature offset | 0 | 0x3FFE0 (end of the block) |
| EEPROM header stride | 0x10 | 0x20 |

The tool walks the file in erase blocks and classifies each block by the 4 bytes at the
signature offset:

| Signature | Block type |
|-----------|------------|
| `E5 1F F0 E5` (ARM `ldr pc`) or `FF FF FF FF` in block 0 | BCORE (bootcore) |
| `EELI` (`EELITE`) | EEPROM LITE |
| `EEFU` (`EEFULL`) | EEPROM FULL (always two consecutive blocks) |
| `FFS\0`, `FFS_0/1/2`, `FFS_B/C` | file system (not touched) |
| `BB BB 00 00` | EXIT (not touched) |

Typical layout: BCORE at 0, EELITE at 0x40000, EEFULL in the last blocks of the flash.

## 4. Bootcore patch

Two values are replaced in block 0:

| Layout | HASH (16 bytes) | IMEI (15 ASCII digits) |
|--------|-----------------|------------------------|
| x85 | 0x3E400 | 0x3E410 |
| x65/x75, interface version 2.2 | 0x23C | 0x660 |
| x65/x75, interface version 2.0 | 0x238 | 0x65C |

If all 16 bytes at the HASH location are `FF`, the bootcore is "cleared" and recalculation
is impossible.

## 5. EEPROM structure

Every EEPROM partition has an entry table growing **downwards** from its header. Entries are
16 bytes on x65/x75 and 32 bytes on x85. The first entry is at 0x1FFF0 on x65/x75 and 0x3FFC0
on x85. A free entry ends the x65/x75 table; x85 scanning continues because inline data and
unused physical entries can occur between valid entries.

```
struct header {
    u32 flags;       // 0xFFFFFFC0 = valid entry, 0xFFFFFF00 = obsolete
    u32 id;          // x85 EELITE uses u16; IDs must be below 5000
    u32 size;        // x85 EELITE uses u16
    u32 data_offset; // offset of the data inside the erase block
};
```

Where the data lives:

* **x65/x75**: always at `data_offset`.
* **x85**: small entries (FULL: size <= 0x200, LITE: size <= 0x10) are stored *inline* right
  below the header: starting at `hdr - 0x10 - size - (size & ~0xF)`, in 16-byte pieces that
  skip every second 16-byte slot (bytes whose block offset has bit 4 set are not data).
  Larger entries use `data_offset`. Every 0x20-byte header slot is examined.

Stored size vs. payload:

* EEFULL entries carry one metadata byte before the payload. Extended entries also carry five
  trailing metadata bytes; all metadata is excluded from the exposed payload.
* On x65/x75, EELITE entries carry one extra byte **after** the payload.
* On x85, EELITE entries have no extra byte.

If a matched entry has an unexpected size it is reported and left alone.

## 6. Entries that get replaced

`IMEI8` below is the IMEI packed into 8 bytes: `(d0 << 4) | 0x0A`, then
`(d2 << 4) | d1`, `(d4 << 4) | d3`, ... `(d12 << 4) | d11`, `d13` (digit 14 is unused).

| Entry | Size | Content | Mandatory |
|-------|------|---------|-----------|
| LITE 76 | 10 | IMEI block, variant 0 (see 7.1) | yes |
| FULL 8 (5008) | 0xE0 | encrypted record (see 7.3) | yes |
| FULL 9 (5009) | 10 | IMEI block, variant 1 (see 7.1) | yes |
| FULL 77 (5077) | 0xE8 | encrypted record (see 7.3) | yes |
| FULL 121 (5121) | 0x38 | encrypted SKEY marker + 6 encrypted master codes (see 7.4) | yes |
| FULL 122 (5122) | 6 | `SKEY (u32)`, `58 00` | yes |
| FULL 123 (5123) | 0xC or 0x20 | `3F FF FF FF` + first 8 bytes of block 121 (12-byte variant, x75/x85); `07 1F FF FF` + the same followed by zeros (0x20-byte x65 variant) | yes |
| LITE 52 | 0x122 | `BKEY (16)`, `VerDown (1)`, then a constant 145-byte tail starting `FF 01 00 00 01 03 00 01 00 01 00 54 00 55 00 80 00 D9 D9 F3 ...` and `FF` padding (see `BLOCK52_TAIL` in the source) | yes |
| FULL 468 (5468) | 0x31 | `BKEY (16)`, `58`, `MD5(ESN u32)` (16), `MD5(IMEI ASCII)[0..8]`, `MD5(previous 0x29 bytes)[0..8]` | no (x85 only) |
| LITE 320 | 0x10 | `BKEY` | no (x85 only) |

PapuaUtils refuses the result if any mandatory entry was not found. Entries whose current
content already equals the generated one are reported as "does not require replacement".

## 7. Building blocks

### 7.1 IMEI block (LITE 76 / FULL 9)

```
buf[0..6] = 14 IMEI digits as BCD, low nibble = even digit, high nibble = odd digit
buf[7]    = luhn_check_digit(digits 0..13) << 4      # recomputed, digit 14 of the IMEI is ignored
xe = buf[0] ^ buf[2] ^ buf[4] ^ buf[6]
xo = buf[1] ^ buf[3] ^ buf[5] ^ buf[7]
buf[8] = xe   (variant 0)   |   xo   (variant 1)
buf[9] = xe ^ xo ^ 0xFF
rounds: variant 0 uses round keys K[1], K[3], K[5], K[7]; variant 1 uses K[0], K[2], K[4], K[6]
        K = E3 B7 5C 13 B0 D2 C4 19
each round:
    mask = 0
    for k in 0..4:
        old    = buf[k]
        s      = (SBOX_HI[old >> 4] << 4) + SBOX_LO[old & 0xF]
        buf[k] = s ^ K[r] ^ buf[k+5] ^ mask
        buf[k+5] = old
        mask ^= 0xFF
SBOX_HI = 03 0F 01 0D 05 0B 0D 09 0D 07 06 00 0E 06 0B 08
SBOX_LO = 0A 0E 01 05 03 06 02 0F 0B 0A 03 05 06 05 04 02
```

(The tool contains three S-box pairs selected by a "phone type" argument; the fullflash
recalculation always uses type 5, which is the pair above.)

### 7.2 Stream cipher

All encrypted entries use the same cipher, keyed by a 64-byte key block:

```
K  = MD4(key64)                                   # 16 bytes (MD4 with normal padding)
S  = RC4 key schedule with K as a 16-byte key      # standard KSA
i = j = prev = 0
for each byte b:
    i    = (i + 1) & 0xFF
    prev = (prev + S[i]) & 0xFF
    j    = (j + prev) & 0xFF
    t    = (S[j] + prev) & 0xFF
    S[i] = S[j]                                    # not a swap!
    S[j] = prev
    encrypt: prev = S[t] ^ b ; output prev
    decrypt: prev = b        ; output S[t] ^ prev
```

Three key blocks are embedded in the tool (see `KEY_EEP`, `KEY_MK1`, `KEY_MK2` in the
source); some words are overwritten per phone:

* `KEY_EEP` (blocks 8 and 77): `[0x30] = ESN`, `[0x38..0x40] = IMEI8`.
* `KEY_MK1(code)` (block 121): `[0] = code ^ 0x7F0BF23B`, `[4] = ESN`, `[0x38..0x40] = IMEI8`.
* `KEY_MK2(code)` (block 121): `[0] = (IMEI8[0] << 8) + 8 + (IMEI8[1] << 16) + (IMEI8[2] << 24)`,
  `[4..8] = IMEI8[3..7]`, `[0x38] = code ^ 0x7F0BF23B`, `[0x3C] = ESN`.

### 7.3 Blocks 8 and 77

Block 8 (0xE0 bytes):

```
u32 @0x00 = 0x56605650   u32 @0x04 = 0          u32 @0x08 = 0x300      u32 @0x0C = 0x670000
u32 @0x10 = 0            u32 @0x14 = 0xFFFFFFFF u32 @0x18 = 0xFFFFFFFF u32 @0x1C = 0x6462FF00
u32 @0x20 = 0x56805670   u32 @0x24 = 0          bytes 0x28..0xE0 = FF  u32 @0xD8 = 0x50  u32 @0xDC = 0
@0x1E = sum8(bytes 0x08..0x1E), @0x1F = xor8(bytes 0x08..0x1E)
@0xD8 = sum8(bytes 0x28..0xD8), @0xD9 = xor8(bytes 0x28..0xD8)
u32 @0x00 ^= 0xBA1FE5D7, u32 @0x04 ^= 0xD95D2DFD, u32 @0x20 ^= 0xBA1FE5D7, u32 @0x24 ^= 0xD95D2DFD
encrypt(bytes 0x00..0x20, KEY_EEP); encrypt(bytes 0x20..0xE0, KEY_EEP)     # two separate streams
```

Block 77 (0xE8 bytes):

```
u32 @0x00 = 0x56A05690, u32 @0x04 = 0, bytes 0x08..0xE8 = FF, byte @0x0B = 0
u32 @0xE0 = 0x063DFF29, u32 @0xE4 = 0xF3800B06
@0xE0 = sum8(bytes 0x08..0xE0), @0xE1 = xor8(bytes 0x08..0xE0)     # overwrites their first two bytes
u32 @0x00 ^= 0xBA1FE5D7, u32 @0x04 ^= 0xD95D2DFD
encrypt(all 0xE8 bytes, KEY_EEP)
```

### 7.4 Block 121 (SKEY and master codes)

```
out[0..8]  = encrypt({0x77C5742D, 0xF49A4ADA}, KEY_MK1(SKEY))
for k in 0..5:
    v = {k ^ 0x77C5742D, 0xF49A4ADA}
    v = encrypt(v, KEY_MK2(master[k]))
    v = encrypt(v, KEY_MK1(master[k]))
    out[8 + 8k .. 16 + 8k] = v
```

## 8. What the emulator does with it

With `--siemens-recalc`, `pmb887x-emu` runs the algorithm with the defaults listed above.
If anything changes, QEMU receives an in-memory copy (`memfd` on Linux, a temporary file elsewhere),
so the file on disk is not modified. With `--rw` the recalculated data replaces the file itself.
Recalculation and recovery are skipped when the BCORE HASH already equals the default recalculated
HASH `54F80AC12ACD94B2F5CFFB9BF7E4D493`.

The same relations run backwards: a fullflash stores the IMEI, the SKEY and BKEY, so the only
unknown in section 2 is the ESN. By default the emulator brute forces those 32 bits against the stored
BKEY (or against the bootcore HASH when BKEY is blank) and runs the fullflash unmodified with
the ESN it finds.
