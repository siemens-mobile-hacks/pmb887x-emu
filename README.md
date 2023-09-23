# Prebuilded releases
For Windows you can download in releases: https://github.com/Azq2/pmb887x-emu/releases

For MacOS/Linux you must build itself. Unix way :)

# Building
Linux
```bash
# Install dependencies (Ubuntu or Debian)
sudo apt-get install perl
sudo apt-get build-dep qemu

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build.sh
```

Windows (building on Ubuntu 23.04)
```bash
# Install dependencies
sudo apt-get -y install meson mingw-w64 mingw-w64-tools mingw-w64-i686-dev mingw-w64-x86-64-dev mingw-w64-common

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build_win.sh
./tools/make_dist_win.sh # optional, for .zip with release
```

MacOS
```bash
# Install dependencies
brew install llvm libffi gettext glib pkg-config pixman ninja meson coreutils perl

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build_osx.sh
./tools/make_dist_osx.sh # optional, for .tar.gz with release
```


# Supported hardware
| Phone                    | CPU     | LCD     | FLASH     |
|--------------------------|---------|---------|-----------|
| BenQ-Siemens EL71 / C1F0 | pmb8876 | jbt6k71 | 0020:8819 |
| BenQ-Siemens C81         | pmb8876 | ssd1286 | 0020:8819 |

# Running
1. Make sure to follow steps in [docs/recalc-siemens-fullflash.md](docs/recalc-siemens-fullflash.md) for your fullflash file.
If you would like to get past the "Insert your SIM card" screen, you will also currently need to apply a patch
like this one https://patches.kibab.com/patches/details.php5?id=7116 to your fullflash file. This can be done using V_Klay.
2. Fullflash file should be located in the same directory as `emu` binary with filename `ff.bin`.
3. Run `./emu` or `./emu --siemens-esn=12345678 --siemens-imei=490154203237518`.

Once the emulator is running, you should first see BENQ-Siemens boot screen and then something like this:
![A screenshot of a running emulator](docs/emu.png)

You can press keys on the phone keyboard using your computer keyboard.

* Soft keys: Left: `F1`, Right: `F2`. Send/Start Call: `F3`. End Call: `F4`.
* Navigation (joystick): `Arrow keys`. Press navigation key: `Enter`.
* Number keys and `*` are mapped to NUM-keys. `#` is mapped to Numpad `/`.

Full key mapping is defined in [board.c](https://github.com/Azq2/qemu-pmb887x/blob/7c83c045a11cd110d220ec39a6cad3dbafe86e6c/hw/arm/pmb887x/boards.c#L19-L67).


# TODO
- SGold2 boards:
  - [ ] BenQ-Siemens SL75
  - [ ] BenQ-Siemens S75
  - [ ] BenQ-Siemens M81
  - [ ] BenQ-Siemens S68
  - [ ] BenQ-Siemens E71 / M72
