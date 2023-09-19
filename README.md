# Building

At the moment, building and running is only supported on Linux or WSL2 (not tested).

```bash
# Install dependencies
sudo apt-get build-dep qemu

# Clone from GIT
git clone https://github.com/Azq2/pmb887x-emu
cd pmb887x-emu
git submodule update --init

# Configure and build
./tools/build.sh
```

# Supported hardware
| Phone                    | CPU     | LCD     | FLASH     |
|--------------------------|---------|---------|-----------|
| BenQ-Siemens EL71 / C1F0 | pmb8876 | jbt6k71 | 0020:8819 |

# TODO
- SGold2 boards:
  - [ ] BenQ-Siemens SL75
  - [ ] BenQ-Siemens S75
  - [ ] BenQ-Siemens C81
  - [ ] BenQ-Siemens M81
  - [ ] BenQ-Siemens S68
  - [ ] BenQ-Siemens E71 / M72
