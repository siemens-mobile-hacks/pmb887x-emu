# What is this?
This is a hardware emulator for any boards with pmb8875/pmb8876 CPU, mostly legendary Siemens phones.
The current state is very early alpha with many bugs and most hardware unimplemented. :)

# Installation
- Windows (x86_64, ARM64): download from [Releases](https://github.com/siemens-mobile-hacks/pmb887x-emu/releases).
- Ubuntu 24.04 (amd64, arm64): download the `.deb` from [Releases](https://github.com/siemens-mobile-hacks/pmb887x-emu/releases) and run `sudo apt install ./pmb887x-emu_*.deb`.
- ArchLinux: `yay -S pmb887x-emu`
- macOS: `brew install siemens-mobile-hacks/tap/pmb887x-emu` or download from [Releases](https://github.com/siemens-mobile-hacks/pmb887x-emu/releases).
- Build from sources:
  ```bash
  # Ubuntu or Debian
  sudo apt-get install bison build-essential cmake flex gettext git libaio-dev libcacard-dev libcapstone-dev \
      libglib2.0-dev libgtk-3-dev libpixman-1-dev libpulse-dev libslirp-dev libudev-dev liburing-dev \
      libxkbcommon-dev ninja-build pkg-config python3-venv

  git clone --recurse-submodules --shallow-submodules --depth 1 https://github.com/siemens-mobile-hacks/pmb887x-emu
  cd pmb887x-emu

  cmake -B build
  cmake --build build -j$(nproc)
  sudo cmake --install build
  # or sudo cmake --install build --prefix /opt/pmb887x-emu
  ```

You can also use `./build/pmb887x-emu` instead of `pmb887x-emu` if you want to run it without installation.

Windows builds are cross-compiled in Docker: `tools/build_windows.sh x86_64` or `tools/build_windows.sh aarch64`, the result is in `build-windows-<arch>/dist`.

# Quick start

```
pmb887x-emu --fullflash EL71.bin
```

The device is detected from Siemens fullflash metadata.
LG phones need `--device` and an EFA image: see [LG fullflash](docs/usage.md#real-world-example).

# Documentation

- [Usage](docs/usage.md): command line, OTP and EFA, keyboard, startup scenarios, accessories, GSM network
- [SIM cards](docs/sim-card.md)
- [Debug output](docs/debug-output.md)
- [Fullflash recalculation](docs/recalc-siemens-fullflash.md)

# Supported hardware
**Siemens SG2 platform**
| Phone              | Emulator      |
|--------------------|---------------|
| BenQ-Siemens E71   | siemens-e71   |
| BenQ-Siemens EL71  | siemens-el71  |
| BenQ-Siemens C1F0  | siemens-el71  |
| BenQ-Siemens CL61  | siemens-cl61  |
| BenQ-Siemens CL61A | siemens-cl61a |
| BenQ-Siemens C81   | siemens-c81   |
| BenQ-Siemens M72   | siemens-m72   |
| BenQ-Siemens M81   | siemens-m81   |
| BenQ-Siemens S68   | siemens-s68   |
| Siemens S75        | siemens-s75   |
| Siemens SL75       | siemens-sl75  |

**Siemens SGL platform**
| Phone              | Emulator     |
|--------------------|--------------|
| Siemens C65        | siemens-c65  |
| Siemens CX65       | siemens-cx65 |
| Siemens M65        | siemens-m65  |
| Siemens S65        | siemens-s65  |
| Siemens SL65       | siemens-sl65 |
| Siemens SK65       | siemens-sk65 |
| Siemens CX70       | siemens-cx70 |
| Siemens C72        | siemens-c72  |
| Siemens C75        | siemens-c75  |
| Siemens CX75       | siemens-cx75 |
| Siemens M75        | siemens-m75  |
| Siemens ME75       | siemens-me75 |

**LG SG2 platform**
| Phone                         | Emulator     |
|-------------------------------|--------------|
| LG KE970 (Shine)              | lg-ke970     |
| LG KE800 (Chocolate Platinum) | lg-ke800     |


# AI-assisted contributions

We are not against AI. We are against vibe coding, AI slop, and attempts to offload engineering work to a model. This project prioritizes quality, not development speed or results at any cost.

1. **Do not use AI-generated text in human-to-human communication.**

   Write comments, discussions, PR descriptions, and responses to reviewers yourself.

2. **Do not let AI submit PRs or commits.**

   The author must always be a human who has personally reviewed the changes and takes responsibility for them.

4. **Do not submit code primarily designed or written by AI.**

   Architecture, algorithms, code organization, and the final implementation must be decided by a human. AI may only be used as an auxiliary tool.

6. **You must understand all the code you submit.**

   You must be able to explain every change, justify your decisions, and fix any problems yourself. If you do not understand the code, open a feature request instead of a PR.

8. **Code must be simple, clear, and tested.**

   Follow KISS, the project's coding style, and its existing architecture. Do not introduce unnecessary abstractions, dependencies, or untested changes.

AI slop PRs will be closed without review.
