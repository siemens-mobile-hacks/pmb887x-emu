# Usage

## Command line
```
Usage: pmb887x-emu [--help] [--version] [--device VAR] --fullflash VAR [--rw] [--flash-0-otp0 VAR] [--flash-0-otp1 VAR] [--flash-0-otp0-file VAR] [--flash-0-otp1-file VAR] [--flash-0-efa-file VAR] [--flash-1-otp0 VAR] [--flash-1-otp1 VAR] [--flash-1-otp0-file VAR] [--flash-1-otp1-file VAR] [--flash-1-efa-file VAR] [--flash-2-otp0 VAR] [--flash-2-otp1 VAR] [--flash-2-otp0-file VAR] [--flash-2-otp1-file VAR] [--flash-2-efa-file VAR] [--flash-3-otp0 VAR] [--flash-3-otp1 VAR] [--flash-3-otp0-file VAR] [--flash-3-otp1-file VAR] [--flash-3-efa-file VAR] [--siemens-esn VAR] [--siemens-imei VAR] [--siemens-recalc] [--sim VAR] [--sim-reader-name VAR] [--sim-imsi VAR] [--sim-operator VAR] [--startup VAR] [--serial VAR] [--usartd] [--wait-for-serial] [--gdb] [--trace VAR] [--trace-io VAR] [--trace-log VAR] [--trace-time] [--qemu-monitor VAR] [--qemu-accel VAR] [--qemu-icount VAR] [--qemu-run-with-gdb] [--qemu-stop-on-exception] [--qemu-debug VAR]

Generic emulator for PMB887X-based mobile phones.

Optional arguments:
  -h, --help                              shows help message and exits
  -v, --version                           prints version information and exits

Main options (detailed usage):
  -d, --device                            Device name or path to a custom board TOML file (detected for Siemens fullflashes by default) [nargs=0..1] [default: ""]
  -f, --fullflash                         Path to the fullflash.bin file [required]
  --rw                                    Allow writing to fullflash.bin (dangerous!)

OTP options (detailed usage):
  --flash-otp0, --flash-0-otp0            Raw NOR flash otp0 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-otp1, --flash-0-otp1            Raw NOR flash otp1 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-otp0-file, --flash-0-otp0-file  Raw NOR flash OTP0 file [nargs=0..1] [default: ""]
  --flash-otp1-file, --flash-0-otp1-file  Raw NOR flash OTP1 file [nargs=0..1] [default: ""]
  --flash-efa-file, --flash-0-efa-file    Raw NOR flash EFA file [nargs=0..1] [default: ""]
  --flash-1-otp0                          Raw NOR flash otp0 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-1-otp1                          Raw NOR flash otp1 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-1-otp0-file                     Raw NOR flash OTP0 file [nargs=0..1] [default: ""]
  --flash-1-otp1-file                     Raw NOR flash OTP1 file [nargs=0..1] [default: ""]
  --flash-1-efa-file                      Raw NOR flash EFA file [nargs=0..1] [default: ""]
  --flash-2-otp0                          Raw NOR flash otp0 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-2-otp1                          Raw NOR flash otp1 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-2-otp0-file                     Raw NOR flash OTP0 file [nargs=0..1] [default: ""]
  --flash-2-otp1-file                     Raw NOR flash OTP1 file [nargs=0..1] [default: ""]
  --flash-2-efa-file                      Raw NOR flash EFA file [nargs=0..1] [default: ""]
  --flash-3-otp0                          Raw NOR flash otp0 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-3-otp1                          Raw NOR flash otp1 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-3-otp0-file                     Raw NOR flash OTP0 file [nargs=0..1] [default: ""]
  --flash-3-otp1-file                     Raw NOR flash OTP1 file [nargs=0..1] [default: ""]
  --flash-3-efa-file                      Raw NOR flash EFA file [nargs=0..1] [default: ""]
  --siemens-esn                           Siemens flash ESN (HEX) [nargs=0..1] [default: "12345678"]
  --siemens-imei                          Siemens flash IMEI (number) [nargs=0..1] [default: "490154203237518"]
  --siemens-recalc                        Recalculate the fullflash security keys instead of recovering its original OTP

SIM options (detailed usage):
  --sim                                   SIM source: virtual, none, or reader [nargs=0..1] [default: "virtual"]
  --sim-reader-name                       Exact PC/SC reader name for --sim reader (uses the first reader with a card by default) [nargs=0..1] [default: ""]
  --sim-imsi                              Virtual SIM IMSI (15 decimal digits; derived from --sim-operator by default) [nargs=0..1] [default: ""]
  --sim-operator                          Virtual SIM operator code as MCC+MNC (5 or 6 decimal digits) [nargs=0..1] [default: "00101"]

Startup options (detailed usage):
  --startup                               Startup scenario from the board config [nargs=0..1] [default: "ONLINE"]

Accessory options (detailed usage):
  --accessory                             Accessory from the board config plugged into the system connector

GSM options (detailed usage):
  --gsm-osmotrx                           Act as the transceiver of osmo-bts-trx: HOST:PORT for the CLOCK, TRXC, and TRXD ports PORT..PORT+2
  --gsm-trxmux                            Join the cell of a TRX multiplexer shared by several emulators: HOST:PORT
  --gsm-off                               No GSM network: the phone receives no signal
  --gsm-arfcn                             Channel of the built-in cell: a GSM850, GSM900, or DCS1800 ARFCN, or PCS1900 with --gsm-pcs1900 (default 512)
  --gsm-pcs1900                           --gsm-arfcn is a PCS1900 channel
  --gsm-gsmtap                            Send the built-in cell's air traffic as GSMTAP to HOST, UDP port 4729

Serial options (detailed usage):
  --serial                                Connect host serial port to QEMU
  --usartd                                Connect to usartd.pl in QEMU
  -W, --wait-for-serial                   Wait for first byte on serial port

Trace options (detailed usage):
  --gdb                                   Run firmware with GDB
  -D, --trace                             Debug channels for logs and register accesses, for example '*,-DSP.*' or 'USART0,LCD0'
  --trace-io                              Debug channels for register accesses only
  --trace-log                             Debug channels for logs only
  --trace-time                            Prefix debug lines with the emulated time

QEMU options (detailed usage):
  --qemu-monitor                          QEMU monitor
  --qemu-accel                            QEMU -accel options (default: tcg,vcpu-timers=on,cross-page-links=on, or tcg with --qemu-icount)
  --qemu-icount                           QEMU -icount options, for example shift=3,sleep=off
  --qemu-run-with-gdb                     Run emulator using GDB (debug)
  -E, --qemu-stop-on-exception            Stop QEMU on ARM exception
  --qemu-debug                            QEMU -d log items added to cpu_reset,cpu_exit,guest_errors,unimp, for example 'int'
```

**Some useful examples:**

1. Running a Siemens fullflash with automatic OTP recovery
```
pmb887x-emu --fullflash EL71.bin
```

The device is detected from Siemens fullflash metadata. Use `--device` explicitly for other vendors, custom board files,
or to override the detected device.

2. Running fullflash with your own ESN and IMEI
```
pmb887x-emu --fullflash EL71.bin --siemens-esn=12345678 --siemens-imei=490154203237518
```

## OTP and EFA

OTP0, OTP1 and EFA are separate NOR flash regions outside the main array stored in the fullflash image.

With `--rw`, FLASH0 stores changes in raw sidecars next to the fullflash:

- `EL71.bin.cfi-otp0`
- `EL71.bin.cfi-otp1`
- `EL71.bin.cfi-efa`

Files are created on the first successful data change. A missing or empty file uses the initial value from `--flash-otp0`, `--flash-otp1`, `--siemens-esn` or `--siemens-imei`. A non-empty file overrides the initial value and must match the flash geometry. Without `--rw`, files are loaded but not changed.

Use `--flash-N-otp0-file`, `--flash-N-otp1-file` and `--flash-N-efa-file` to override paths for banks 0-3. FLASH0 also accepts the names without `-0`. Passing a file for an unsupported region is an error.

## Real world example
Let's assume you have a fullflash. Siemens mobile devices are paranoid and the firmware has hardware binding:
the keys stored in the fullflash must match the ESN and IMEI of the phone.

The emulator handles this for you: by default it recovers the original OTP from the fullflash and caches the ESN,
so this simply works:
```
pmb887x-emu --fullflash EL71.bin
```

The result and available identity data are saved as TOML next to the fullflash in a `.esn` file.
The cached ESN is reused while its `HASH` matches the fullflash.

Use `--siemens-recalc` to recalculate the keys for the emulator identity instead.
Details: [docs/recalc-siemens-fullflash.md](recalc-siemens-fullflash.md).
The algorithm itself is described in [docs/recalc-algorithm.md](recalc-algorithm.md).

If you know the original ESN and IMEI of your phone, you can run the emulator with them and nothing needs to be recalculated:
```
pmb887x-emu --fullflash EL71.bin --siemens-esn=12345678 --siemens-imei=490154203237518
```

LG phones do not need ESN/IMEI, but they need a provisioned EFA region to work correctly (though not strictly required to boot) - commonly seen as separate 32K `.eep` image (unencrypted, a valid one should begin with `FF` bytes and contain model/version info around 0x72A0) or appended as 32K tail at the end of a `.bin` dump.

If you have a `.bin` flash you need to split it into two files, eg. for KE970:

```sh
dd if="KE970P40-07-V10j-XXX-XX JUL 15 2008.bin" of=KE970.bin          bs=1M count=128
dd if="KE970P40-07-V10j-XXX-XX JUL 15 2008.bin" of=KE970.bin.cfi-efa  bs=1M skip=128

stat -c '%n %s' KE970.bin KE970.bin.cfi-efa
# KE970.bin 134217728
# KE970.bin.cfi-efa 32768
```

Then run the emulator like this:
```
pmb887x-emu --fullflash KE970.bin --device lg-ke970
```

The virtual SIM is enabled by default. Use `--sim none` to disable it. Operator-locked firmware may require matching `--sim-operator` or `--sim-imsi`.

See [SIM card setup](sim-card.md).

## Keyboard
You can press phone keys using the following computer keyboard shortcuts. Only keys available on the selected board are
active.

| Phone key | Computer key |
|-----------|--------------|
| Navigation | Arrow keys |
| Navigation center | `Enter` or Numpad `Enter` |
| Left soft key | `F1` |
| Right soft key | `F2` |
| Send | `F3` |
| End call | `F4` |
| Music | `F5` |
| Play/pause | `F6` |
| Push-to-talk | `F7` |
| Camera | `F8` |
| Browser | `F9` |
| Volume up | Numpad `+` |
| Volume down | Numpad `-` |
| `C` | Backspace |
| `0` | `0` or Numpad `0` |
| `1` / `2` / `3` | `1` / `2` / `3` or Numpad `7` / `8` / `9` |
| `4` / `5` / `6` | `4` / `5` / `6` or Numpad `4` / `5` / `6` |
| `7` / `8` / `9` | `7` / `8` / `9` or Numpad `1` / `2` / `3` |
| `*` | Numpad `*` |
| `#` | Numpad `/` |

## Startup scenarios

Use `--startup NAME` to select a startup scenario. `ONLINE` is used by default. Supported boards may also provide
`PTEST` for the service-mode key combination and `OFFLINE` for starting without pressed keys.

Startup keys are pressed when firmware execution begins. The release countdown starts when the keypad becomes ready.

## Accessories

Use `--accessory NAME` to select what is plugged into the system connector at boot.
The default is `data-cable` on Siemens boards and `none` on LG boards; `none` leaves the connector empty.

| Siemens boards | Accessories |
|----------------|-------------|
| E71, EL71, M72, S68, CL61 | `none`, `data-cable`, `usb-cable`, `usb-cable-nc`, `boot-box`, `tty`, `music-cable`, `headset-mono`, `headset-stereo`, `atc-device` |
| C81, M81, S75, SL75 | `none`, `data-cable`, `usb-cable`, `boot-box`, `desktop-charger`, `tty`, `tty-x55`, `music-set`, `headset-mono`, `headset-stereo`, `atc-device`, `a2i-device` |
| C65, C72, C75, CX65, CX70, M65, ME75, S65, SL65, CX75, M75 | `none`, `data-cable`, `usb-cable`, `boot-box`, `desktop-charger`, `tty`, `tty-x55`, `music-set`, `headset-mono`, `headset-stereo`, `atc-device`, `a2i-device` |

| LG boards | Accessories |
|-----------|-------------|
| KE800 | `none`, `data-cable`, `headset-stereo` |
| KE970 | `none`, `headset-stereo`, `data-cable` |

Other boards have no accessories yet.

## GSM network

### Built-in cell

Runs by default: network 001-01, GPRS through the host.
No calls, SMS or MMS.

Internet profile on the phone:

| Setting | Value |
| --- | --- |
| APN | any |
| User, password | any |
| Proxy | off |

Modem: `AT+CGDCONT=1,"IP","internet"`.

Cell options:

- `--gsm-arfcn N`: channel, default 512 (DCS1800).
  GSM850 is 128–251, GSM900 is 0–124 and 975–1023, DCS1800 is 512–885.
- `--gsm-pcs1900`: `--gsm-arfcn` 512–810 is a PCS1900 channel.
- `--gsm-gsmtap HOST`: copy of the air traffic as GSMTAP to UDP port 4729, for example for Wireshark.

```sh
build/pmb887x-emu -f CX75.bin --gsm-arfcn 600 --gsm-gsmtap 127.0.0.1
```

### Osmocom network

Calls, SMS, MMS and internet; several emulators in one cell.

```sh
(cd bts && docker compose up -d)
build/pmb887x-emu -f EL71.bin --gsm-trxmux 127.0.0.1:5750
build/pmb887x-emu -f EL71.bin --gsm-trxmux 127.0.0.1:5750 --sim-imsi 001010000001001
```

- Each emulator needs its own `--sim-imsi`.
- Operator profiles work as is.
- A phone without settings gets them by SMS.

More in [bts/README.md](../bts/README.md).

### Other options

- `--gsm-osmotrx HOST:PORT`: TRX for an external `osmo-bts-trx`, one phone only.
- `--gsm-off`: no network.
