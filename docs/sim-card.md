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

Only PC/SC readers are supported.

**APDUs are forwarded without write protection. Use a test SIM.**

Enable tracing with `--trace sim-card`.

- `has no available card`: insert the SIM before starting the emulator.
- Repeated `RST`/ATR without APDUs: the phone rejected the card.
