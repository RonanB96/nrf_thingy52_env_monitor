# Development Environment Setup Guide

This guide provides step-by-step instructions for setting up the development environment for the Nordic Thingy:52 Environmental Monitor project.

## Prerequisites

- Linux Ubuntu 22.04+ (recommended) or macOS
- Git
- Python 3.10+ (minimum version as per [Zephyr requirements](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies))
- VS Code (optional but recommended)

## 1. Clone Repository

```bash
git clone https://github.com/RonanB96/nrf_thingy52_env_monitor.git
cd nrf_thingy52_env_monitor
```

## 2. Install nRF Connect SDK Dependencies

### Option A: Using nRF Connect SDK Toolchain Manager (Recommended)

1. Download and install [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)
2. Install "Toolchain Manager" app
3. Install nRF Connect SDK v3.0.2 with full toolchain

### Option B: Manual Installation

**Reference**: Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) for complete dependency installation.

```bash
# Install dependencies (Ubuntu 22.04+)
sudo apt update
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1

# Install west
pip3 install --user west

# Set up toolchain path (adjust path to your installation)
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=/home/$USER/ncs/toolchains/c5be9c56c7/opt/zephyr-sdk
```

## 3. Set Up Python Virtual Environment

**Reference**: Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#get-zephyr-and-install-python-dependencies) for Python dependency installation.

```bash
# Create virtual environment
python3 -m venv .venv

# Activate virtual environment
source .venv/bin/activate

# Install west (Zephyr's meta-tool)
pip install west

# Install Python dependencies using west (recommended method)
# Note: Since this project uses a self-contained workspace, we install manually
pip install intelhex          # Required for hex file generation
pip install pyelftools        # ELF file processing
pip install pykwalify         # YAML validation
pip install ruamel.yaml       # YAML processing
pip install python-dateutil   # Date utilities
```

## 4. Configure West Workspace

The project uses a self-contained West workspace. Configure it:

```bash
# Activate virtual environment (if not already active)
source .venv/bin/activate

# Configure West with automatic toolchain and board settings
west config build.board thingy52/nrf52832
west config build.cmake-args -- "-DZEPHYR_TOOLCHAIN_VARIANT=zephyr -DZEPHYR_SDK_INSTALL_DIR=/home/$USER/ncs/toolchains/c5be9c56c7/opt/zephyr-sdk"

# Verify configuration
west config -l
```

**Expected output:**

```text
manifest.path=app
manifest.file=west.yml
zephyr.base=modules/zephyr
build.board=thingy52/nrf52832
build.cmake-args=-DZEPHYR_TOOLCHAIN_VARIANT=zephyr -DZEPHYR_SDK_INSTALL_DIR=/home/USER/ncs/toolchains/c5be9c56c7/opt/zephyr-sdk
```

## 5. Verify Build Environment

```bash
# Test build configuration (CMake only, fast)
west build app --cmake-only

# Full pristine build test
west build -p always app
```

**Expected success indicators:**

- No CMake errors
- Toolchain detection: `Found toolchain: zephyr 0.17.0`
- Board detection: `Board: thingy52, qualifiers: nrf52832`
- Final output: Memory usage report and `merged.hex` generation

## 6. Hardware Setup (Optional)

For flashing and debugging:

```bash
# Install nRF tools
# Download nRF Command Line Tools from Nordic website
# Or use package manager:
sudo apt install nrf-command-line-tools  # If available

# Test device connection
nrfutil device list
```

## 7. VS Code Setup (Recommended)

Install recommended extensions:

```bash
# Install VS Code extensions
code --install-extension ms-vscode.cpptools
code --install-extension nordic-semiconductor.nrf-connect
code --install-extension ms-python.python
```

Configure VS Code workspace:

- Open project folder in VS Code
- nRF Connect extension should detect the project automatically
- Use "nRF Connect: Generate config" for build tasks

## Troubleshooting

### Common Issues

**Build fails with "No such file or directory" for linker scripts:**

- Ensure toolchain path is correct in West config
- Try pristine build: `west build -p always app`

**"sx1509b@3e init failed: -5" errors:**

- Hardware-specific issue - check device tree overlay
- Sensor initialization order problem

**Python package missing errors:**

- Activate virtual environment: `source .venv/bin/activate`
- Install missing package: `pip install <package-name>`
- For complete dependency reinstall: Use `pip install west` followed by the packages listed in Section 3

**Note**: The project requires specific Python packages for Nordic NCS compatibility. If using a standard Zephyr workspace, you can use `west packages pip --install` as described in the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#get-zephyr-and-install-python-dependencies).

### Alternative: Standard Zephyr Dependencies

For reference, a standard Zephyr project would use:

```bash
# Standard Zephyr workspace dependency installation
west packages pip --install
```

However, this Nordic Thingy:52 project uses a self-contained workspace and requires the manual package installation shown above.

### Verification Commands

```bash
# Check West configuration
west config -l

# Verify toolchain
west build app --cmake-only | grep "Found toolchain"

# Check Zephyr version compatibility
cat modules/zephyr/SDK_VERSION  # Should match installed SDK

# Test Python environment
python --version  # Should be 3.10+
pip list | grep west  # Should show west package
```

## Daily Development Workflow

```bash
# 1. Activate environment
source .venv/bin/activate

# 2. Build (incremental)
west build app

# 3. Flash to device
west flash

# 4. Monitor output
west attach
# Or use: picocom -b 115200 /dev/ttyUSB0
```

## Environment Variables Reference

The following are automatically configured via West:

```bash
# Automatically set by West configuration:
ZEPHYR_TOOLCHAIN_VARIANT=zephyr
ZEPHYR_SDK_INSTALL_DIR=/path/to/ncs/toolchains/c5be9c56c7/opt/zephyr-sdk

# Board automatically detected:
BOARD=thingy52/nrf52832
```

## Official Documentation References

This setup guide supplements the official documentation:

- **[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)**: Official Zephyr installation and setup
- **[Zephyr Dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies)**: Complete list of system dependencies
- **[Zephyr Python Dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#get-zephyr-and-install-python-dependencies)**: Python virtual environment setup
- **[Nordic nRF Connect SDK](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)**: Nordic-specific documentation and tools
- **[West Tool Documentation](https://docs.zephyrproject.org/latest/develop/west/index.html)**: Comprehensive west meta-tool reference

**Key Differences from Standard Zephyr**:

- This project uses a self-contained West workspace with Nordic NCS v3.0.2
- Manual Python dependency installation instead of `west packages pip --install`
- Pre-configured board and toolchain settings via West configuration

## Next Steps

Once environment is set up:

1. Read [copilot-instructions.md](.github/copilot-instructions.md) for project-specific patterns
2. See issue templates in `.github/ISSUE_TEMPLATE/` for development workflows
3. Review `README.md` for project architecture and BLE service details
