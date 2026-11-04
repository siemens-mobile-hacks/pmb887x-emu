# SIM cards

## Virtual SIM

The virtual SIM is enabled by default. Use `--sim none` for an empty slot.

For operator-locked firmware, set MCC+MNC (five or six digits):

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin --sim-operator 26201
```

The default IMSI starts with the operator code. An explicit IMSI must contain 15 digits and use the same prefix:

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin \
    --sim-operator 310260 --sim-imsi 310260123456789
```

### Authentication

The card answers RUN GSM ALGORITHM with the 3GPP TS 34.108 XOR test algorithm.

- `--sim-ki` sets the key: 32 hex digits.
- Default key: `000102030405060708090A0B0C0D0E0F`.

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin --sim-ki 00112233445566778899AABBCCDDEEFF
```

### PIN and PUK

| Option | Default | Notes |
|---|---|---|
| `--sim-pin1` | `0000` | Also enables PIN1. Without it PIN1 is disabled. |
| `--sim-pin2` | `0000` | |
| `--sim-puk1` | `12345678` | |
| `--sim-puk2` | `12345678` | |

PINs have 4 to 8 digits, PUKs have 8 digits.

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin --sim-pin1 1234
```

### Storage

`--sim-storage FILE` keeps the card between runs in a JSON file.

Stored:

- file contents;
- invalidated files;
- PIN and PUK values, attempt counters, PIN1 enabled state.

Not stored, always taken from the options:

- IMSI and operator;
- Ki.

Rules:

- A missing file is created from the current options.
- An existing file wins over the PIN and PUK options.

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin --sim-storage el71-sim.json
```

File format:

```json
{
  "chv1": { "enabled": false, "pin": "0000", "attempts": 3, "puk": "12345678", "unblock_attempts": 10 },
  "chv2": { "pin": "0000", "attempts": 3, "puk": "12345678", "unblock_attempts": 10 },
  "files": {
    "3F00/7F10/6F43": { "invalidated": false, "data": "FFFF" },
    "3F00/7F10/6F3A": { "invalidated": false, "records": ["FFFF...", "..."] }
  }
}
```

- Keys under `files` are file paths from the MF.
- Transparent files have `data`, record files have `records`.
- Values are hex strings of the full file or record size.

## Physical SIM

Physical SIM passthrough is available on Linux.

Install libcacard, PC/SC, and the reader driver. Linux package names:

- Arch Linux: `libcacard pcsclite ccid pcsc-tools`
- Debian/Ubuntu: `libcacard-dev pcscd libccid pcsc-tools`

Start PC/SC and list readers:

```bash
sudo systemctl enable --now pcscd.socket
pcsc_scan -r
```

Insert the SIM and use the first reader containing a card:

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin --sim reader
```

To select a reader, copy its exact name from `pcsc_scan`:

```bash
pmb887x-emu --device siemens-el71 --fullflash EL71.bin \
    --sim reader --sim-reader-name "Alcor Micro AU9540 00 00"
```

### Phone as a SIM reader

A phone running `bsp/apps/sim-reader` acts as a serial GemPC Twin reader for the PC/SC `ccid` driver.
Register it once in `/etc/reader.conf.d/phone-sim-reader`:

```
FRIENDLYNAME "Phone SIM reader"
DEVICENAME /dev/ttyUSB0
LIBPATH /usr/lib/pcsc/drivers/serial/libccidtwin.so
```

Boot the reader, restart PC/SC so that it opens the port, and use `--sim reader`:

```bash
bsp/apps/sim-reader/run.sh
sudo systemctl restart pcscd
pcsc_scan -r
pmb887x-emu --device lg-ke970 --fullflash KE970.bin --sim reader --sim-reader-name "Phone SIM reader 00 00"
```

The reader keeps the phone awake. Stop PC/SC and power the phone off before booting another program on it:

```bash
sudo systemctl stop pcscd
bsp/apps/sim-reader/sim-reader.py off
```

`sim-reader.py atr` and `sim-reader.py apdu A0A40000023F00` test the reader while PC/SC is stopped.

**APDUs are forwarded without write protection. Use a test SIM.**

Enable tracing with `--trace sim-card`.

- `has no available card`: insert the SIM before starting the emulator.
- Repeated `RST`/ATR without APDUs: the phone rejected the card.
