# What is this?
This is a hardware emulator for any boards with pmb8875/pmb8876 CPU, mostly legendary Siemens phones.
The current state is very early alpha with many bugs and most hardware unimplemented. :)

# Installation
- Windows: download from [Releases](https://github.com/Azq2/pmb887x-emu/releases).
- ArchLinux: `yay -S pmb887x-emu`
- OSX: `brew install siemens-mobile-hacks/tap/pmb887x-emu` or download from [Releases](https://github.com/Azq2/pmb887x-emu/releases).
- Build from sources:
  ```bash
  sudo apt-get build-dep qemu # Ubuntu or Debian

  git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/siemens-mobile-hacks/pmb887x-emu
  cd pmb887x-emu

  cmake -B build
  cmake --build build -j$(nproc)
  sudo cmake --install build
  # or sudo cmake --install build --prefix /opt/pmb887x-emu
  ```

# Usage
```
Usage: pmb887x-emu [--help] [--version] --device VAR --fullflash VAR [--rw] [--flash-otp0 VAR] [--flash-otp1 VAR] [--siemens-esn VAR] [--siemens-imei VAR] [--serial VAR] [--usartd] [--gdb] [--trace VAR] [--trace-io VAR] [--trace-log VAR] [--qemu-monitor VAR] [--qemu-run-with-gdb] [--qemu-stop-on-exception] [--qemu-debug VAR]

Generic emulator for PMB887X-based mobile phones.

Optional arguments:
  -h, --help                    shows help message and exits 
  -v, --version                 prints version information and exits 

Main options (detailed usage):
  -d, --device                  Device name or path to custom device.cfg file [required]
  -f, --fullflash               Path to the fullflash.bin file [required]
  --rw                          Allow writing to fullflash.bin (dangerous!) 

OTP options (detailed usage):
  --flash-otp0                  Raw NOR flash otp0 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --flash-otp1                  Raw NOR flash otp1 value in HEX (with lock bits) [nargs=0..1] [default: ""]
  --siemens-esn                 Siemens flash ESN (HEX) [nargs=0..1] [default: ""]
  --siemens-imei                Siemens flash IMEI (number) [nargs=0..1] [default: ""]

Serial options (detailed usage):
  --serial                      Connect host serial port to QEMU 
  --usartd                      Connect to usartd.pl in QEMU 

Trace options (detailed usage):
  --gdb                         Run firmware with GDB 
  -D, --trace                   CPU IO + CPU emulation log 
  --trace-io                    CPU IO tracing only 
  --trace-log                   CPU emulation logs only 

QEMU options (detailed usage):
  --qemu-monitor                QEMU monitor 
  --qemu-run-with-gdb           Run emulator using GDB (debug) 
  -E, --qemu-stop-on-exception  Stop QEMU on ARM exception 
  --qemu-debug                  QEMU debug options 
```

**Some useful examples:**

1. Running fullflash with default emulator OTP
```
pmb887x-emu --fullflash EL71.bin --device siemens-el71
```

2. Running fullflash with your own ESN and IMEI
```
pmb887x-emu --fullflash EL71.bin --device siemens-el71 --siemens-esn=12345678 --siemens-imei=490154203237518
```

P.S. You can also use `./build/pmb887x-emu` instead of `pmb887x-emu` if you want to run it without installation.

# Real world example
Let's assume you have a fullflash. Of course, simply running commands from the examples won't work. :)

That's because Siemens mobile devices are paranoid and the firmware has hardware binding.

You have two options:

1. Recalculate keys in the firmware using the following steps: [docs/recalc-siemens-fullflash.md](docs/recalc-siemens-fullflash.md)
   
   Then run the emulator like this:
   ```
   pmb887x-emu --fullflash EL71.bin --device siemens-el71
   ```

2. Find the original ESN and IMEI from your phone and run the emulator like this:
   ```
   pmb887x-emu --fullflash EL71.bin --device siemens-el71 --siemens-esn=12345678 --siemens-imei=490154203237518
   ```

Once the emulator is running, you should first see the BENQ-Siemens boot screen and then something like this:

![A screenshot of a running emulator](docs/emu.png)

Don't worry, that's okay. :)

Currently, the emulator does not support SIM card emulation.

If you would like to get past the "Insert your SIM card" screen, you will also need to apply a patch like this one: https://patches.kibab.com/patches/details.php5?id=7116 to your fullflash file. This can be done using V_Klay.

# Keyboard
You can press keys on the phone keyboard using your computer keyboard.

* Soft keys: Left: `F1`, Right: `F2`. Send/Start Call: `F3`. End Call: `F4`.
* Navigation (joystick): `Arrow keys`. Press navigation key: `Enter`.
* Number keys and `*` are mapped to NUM-keys. `#` is mapped to Numpad `/`.

The full key mapping is defined in [board.c](https://github.com/Azq2/qemu-pmb887x/blob/7c83c045a11cd110d220ec39a6cad3dbafe86e6c/hw/arm/pmb887x/boards.c#L19-L67).
rovements throughout

# Supported hardware
**Siemens SG2 platform**
| Phone              | Emulator     |
|--------------------|--------------|
| BenQ-Siemens E71   | siemens-e71  |
| BenQ-Siemens EL71  | siemens-el71 |
| BenQ-Siemens CF130 | siemens-el71 |
| BenQ-Siemens M72   | siemens-m72  |
| BenQ-Siemens CL61  | siemens-cl61 |
| BenQ-Siemens C81   | siemens-c81  |
| BenQ-Siemens M81   | siemens-m81  |
| BenQ-Siemens S68   | siemens-s68  |
| Siemens S75        | siemens-s75  |
| Siemens SL75       | siemens-sl75 |

**Siemens SGL platform**
| Phone              | Emulator     |
|--------------------|--------------|
| Siemens CX75       | siemens-cx75 |
