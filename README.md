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
| Phone             | CPU     | LCD     | FLASH     |
|-------------------|---------|---------|-----------|
| BenQ-Siemens EL71 | pmb8876 | jbt6k71 | 0020:8819 |
