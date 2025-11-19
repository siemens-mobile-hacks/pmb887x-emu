# What is this?
This is a hardware emulator for any boards with pmb8875/pmb8876 CPU, mostly legendary Siemens phones.
The current state is very early alpha with many bugs and most hardware unimplemented. :)

# Supported hardware
| Phone                    | CPU     | Emulator       |
|--------------------------|---------|----------------|
| BenQ-Siemens EL71        | pmb8876 | siemens-el71   |
| BenQ-Siemens CF130       | pmb8876 | siemens-el71   |
| BenQ-Siemens E71         | pmb8876 | siemens-e71    |
| BenQ-Siemens C81         | pmb8876 | siemens-c81    |
| BenQ-Siemens M81         | pmb8876 | siemens-m81    |
| Siemens S75              | pmb8876 | siemens-s75    |
| Siemens CX75             | pmb8875 | siemens-cx75   |

# Prebuilt releases
For Windows, you can download **pmb887x-emu-windows.zip** from releases: https://github.com/Azq2/pmb887x-emu/releases

For MacOS/Linux, you must build it yourself. The Unix way! :)

# Building
**Linux**
```bash
# Install dependencies (Ubuntu or Debian)
sudo apt-get build-dep qemu

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu --depth 1
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build.sh

# Install
cmake --install build
```

**Windows (Docker)**
```bash
# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu --depth 1
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build_win.sh
```

**MacOS**
```bash
# Install dependencies
brew install llvm libffi gettext glib pkg-config pixman ninja meson coreutils perl

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu --depth 1
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build_osx.sh

# Install
cmake --install build
```

# How to use
You can use a simple frontend called `pmb887x-emu`. It provides a simpler interface for QEMU.
Just run `pmb887x-emu --help` for all options. But not all options work yet! :) 

Some useful examples:

1. Running fullflash with default emulator OTP
```
pmb887x-emu --fullflash EL71.bin --device siemens-el71
```

2. Running fullflash with your own ESN and IMEI
```
pmb887x-emu --fullflash EL71.bin --device siemens-el71 --siemens-esn=12345678 --siemens-imei=490154203237518
```

P.S. You can also use `./build/emu` instead of `pmb887x-emu` if you want to run it without installation.

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
