# Why is this needed?
Siemens Mobile is paranoid and all fullflashes have hardware binding to the NOR flash serial number.
The keys in the fullflash must match the ESN and IMEI of the (emulated) phone before it can boot.

# Automatic OTP recovery
By default the emulator extracts the IMEI and security records, recovers the original ESN, and
presents the matching OTP to the firmware. The fullflash remains unchanged.

```
pmb887x-emu --device siemens-s75 --fullflash S75.bin
```

The recovered ESN is cached in `S75.bin.esn`, so the 32-bit search only runs once while the
fullflash identity remains unchanged. The file is TOML and stores `ESN`, `IMEI`, `HASH`, `BKEY`,
and `SKEY`; the cached ESN is used only when `HASH` matches the current fullflash.

Related options:

- `--siemens-recalc` recalculates the keys for IMEI `490154203237518`, ESN `12345678`, and SKEY
  `12345678` instead of recovering the original OTP.
- Supplying `--siemens-imei`, `--siemens-esn`, or raw OTP data skips both operations. The supplied
  identity is assumed to match the fullflash.
- Recovery and recalculation are also skipped when the BCORE HASH is
  `54F80AC12ACD94B2F5CFFB9BF7E4D493`, produced by the default ESN and SKEY.
- ESN recovery is skipped when the BCORE HASH is erased.
- With `--siemens-recalc --rw`, the recalculated data replaces the fullflash file. Without `--rw`,
  QEMU receives an in-memory recalculated copy.

# Running a fullflash without touching it
A fullflash read from a real phone is already consistent with the ESN of that phone. Everything the
key check needs is stored in it except the ESN itself: the IMEI and the HASH in the bootcore, the SKEY
in EEPROM block 122 and BKEY in EEPROM block 52.

So instead of rewriting the keys for the emulator ESN, the emulator can search for the ESN they were
built from and give that to the firmware:

```
pmb887x-emu --device siemens-s75 --fullflash S75.bin
```

This sweeps all 2^32 ESNs on every core (up to 30s on Ryzen 9 5950X) and then runs the fullflash with its
original IMEI and ESN, without changing a single byte. Delete `S75.bin.esn` to search again.

It requires at least one intact security record and any companion data used by that record. Recovery can
use BKEY or the bootcore HASH with SKEY, EEPROM block 5468, or the encrypted EEPROM records that are tied
to the IMEI. Recalculation remains available when none of those combinations is usable.

# Running with original ESN and IMEI
If you know your original IMEI and ESN, you can run the emulator without key recalculation.

Example:
```
pmb887x-emu --fullflash S75.bin --siemens-esn=12345678 --siemens-imei=490154203237518
```

Supplying these values skips automatic recalculation and ESN recovery.

# Manual recalculation with PapuaUtils
This is equivalent to `--siemens-recalc`; use it when you need a recalculated file for other tools.

1. Download [x65PapuaUtils](https://siepatch.dev/docs/soft/sgold/service/x65-papua-utils) (works fine in Wine)

2. Go to the "Codes" tab and enter these values:
    - **IMEI:** 490154203237518
    - **ESN:** 12345678
    - **SKEY:** 12345678

    ![recalc-flash-0.png](recalc-flash-0.png)

3. Press the button "Calc HASH + BootKEY from ESN and SKEY".

4. Go to the "Convert" tab and press the button "Recalc Fullflash". In the opened window, select your input fullflash and output file.
    ![recalc-flash-1.png](recalc-flash-1.png)

5. Done! Now you can use the newly saved fullflash in the emulator without recalculation or ESN bruteforce.
