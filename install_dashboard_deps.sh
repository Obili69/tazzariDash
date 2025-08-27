#!/bin/bash

# LVGL Dashboard Complete Dependencies Installation Script
# Compatible with Ubuntu 20.04+ and Raspberry Pi OS

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   log_error "This script should not be run as root. Run as regular user with sudo access."
   exit 1
fi

# Detect OS
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    OS=$ID
    VERSION=$VERSION_ID
    log_info "Detected OS: $OS $VERSION"
else
    log_error "Cannot detect OS version"
    exit 1
fi

# Check for supported OS
case $OS in
    "ubuntu"|"debian"|"raspbian")
        log_info "Supported OS detected"
        ;;
    *)
        log_warning "OS not specifically tested, proceeding anyway..."
        ;;
esac

log_info "Starting LVGL Dashboard Dependencies Installation..."
log_info "This will install:"
log_info "  • Build tools (GCC, CMake, Git)"
log_info "  • SDL2 development libraries"
log_info "  • Audio system dependencies (ALSA, PulseAudio, Bluetooth)"
log_info "  • Python and BeoCreate tools"
log_info "  • Serial communication tools"
log_info "  • LVGL library"

read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Installation cancelled"
    exit 0
fi

# Update package lists
log_info "Updating package lists..."
sudo apt update

# Essential build tools
log_info "Installing essential build tools..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    python3-dev \
    python3-venv

# SDL2 for LVGL development window
log_info "Installing SDL2 development libraries..."
sudo apt install -y \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libsdl2-ttf-dev

# Audio system dependencies
log_info "Installing audio system dependencies..."
sudo apt install -y \
    libasound2-dev \
    libpulse-dev \
    pulseaudio \
    pulseaudio-utils \
    alsa-utils \
    libbluetooth-dev \
    bluez \
    bluez-tools \
    pulseaudio-module-bluetooth

# Additional development libraries
log_info "Installing additional development libraries..."
sudo apt install -y \
    libssl-dev \
    libffi-dev \
    libudev-dev \
    libusb-1.0-0-dev

# Serial communication tools
log_info "Installing serial communication tools..."
sudo apt install -y \
    screen \
    minicom \
    picocom \
    setserial

# Add user to dialout group for serial access
log_info "Adding user to dialout group for serial port access..."
sudo usermod -a -G dialout $USER

# Python development tools and BeoCreate dependencies
log_info "Installing Python development tools..."
sudo apt install -y \
    python3-setuptools \
    python3-wheel

# Create Python virtual environment for BeoCreate tools
log_info "Setting up Python virtual environment for BeoCreate..."
python3 -m venv ~/beocreate-env
source ~/beocreate-env/bin/activate

# Install BeoCreate tools
log_info "Installing BeoCreate Python tools..."
pip3 install --upgrade pip
pip3 install \
    beocreate \
    numpy \
    scipy \
    matplotlib

deactivate

# Create working directory
WORK_DIR="$HOME/lvgl-dashboard"
log_info "Creating working directory: $WORK_DIR"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Clone LVGL library
log_info "Cloning LVGL library..."
if [ ! -d "lvgl" ]; then
    git clone --recurse-submodules https://github.com/lvgl/lvgl.git
    cd lvgl
    git checkout release/v9.0
    cd ..
    log_success "LVGL library cloned"
else
    log_info "LVGL already exists, skipping clone"
fi

# Clone BeoCreate tools
log_info "Cloning BeoCreate tools..."
if [ ! -d "beocreate-tools" ]; then
    git clone https://github.com/hifiberry/beocreate-tools.git
    log_success "BeoCreate tools cloned"
else
    log_info "BeoCreate tools already exist, skipping clone"
fi

# Create basic CMakeLists.txt template
log_info "Creating CMakeLists.txt template..."
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)
project(LVGLDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# LVGL configuration
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h CACHE STRING "Path to lv_conf.h")
add_subdirectory(lvgl)

# Include directories
include_directories(include)
include_directories(lvgl)
include_directories(ui)

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp" 
    "src/*.c"
    "ui/*.c"
)

# Create executable
add_executable(LVGLDashboard ${SOURCES})

# Link libraries
target_link_libraries(LVGLDashboard 
    lvgl
    ${SDL2_LIBRARIES}
    pthread
)

# Compiler flags
target_compile_options(LVGLDashboard PRIVATE ${SDL2_CFLAGS_OTHER})
EOF

# Create basic lv_conf.h template
log_info "Creating lv_conf.h template..."
mkdir -p include
cat > include/lv_conf.h << 'EOF'
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_DEV_VERSION      1
#define LV_COLOR_DEPTH          32
#define LV_MEM_SIZE             (128U * 1024U)

/* SDL drivers */
#define LV_USE_SDL              1

/* Enable fonts */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_48   1

/* Charts */
#define LV_USE_CHART            1

/* Enable all widgets */
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_LABEL            1
#define LV_USE_IMG              1
#define LV_USE_SLIDER           1

#endif /*LV_CONF_H*/
EOF

# Create udev rules for common USB serial devices
log_info "Creating udev rules for USB serial devices..."
sudo tee /etc/udev/rules.d/99-usb-serial.rules > /dev/null << 'EOF'
# ESP32 and Arduino USB serial devices
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="239a", MODE="0666", GROUP="dialout"
EOF

# Reload udev rules
log_info "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

# Create build script
log_info "Creating build script..."
cat > build.sh << 'EOF'
#!/bin/bash
set -e

echo "Building LVGL Dashboard..."

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make -j$(nproc)

echo "Build complete! Run with: ./build/LVGLDashboard"
EOF
chmod +x build.sh

# Create BeoCreate activation script
log_info "Creating BeoCreate environment activation script..."
cat > activate_beocreate.sh << 'EOF'
#!/bin/bash
echo "Activating BeoCreate Python environment..."
source ~/beocreate-env/bin/activate
echo "BeoCreate environment activated. Use 'deactivate' to exit."
EOF
chmod +x activate_beocreate.sh

# Create test serial script
log_info "Creating serial port test script..."
cat > test_serial.sh << 'EOF'
#!/bin/bash
echo "Available serial ports:"
ls /dev/tty{USB,ACM}* 2>/dev/null || echo "No USB serial ports found"

echo ""
echo "USB devices:"
lsusb | grep -E "(Arduino|ESP32|CH340|CP210|FTDI)"

echo ""
echo "To test a port, run:"
echo "  screen /dev/ttyUSB0 115200"
echo "  (Ctrl+A then K then Y to exit)"
EOF
chmod +x test_serial.sh

# System configuration
log_info "Configuring system settings..."

# Enable I2C for HiFiBerry (if on Raspberry Pi)
if [[ $OS == "raspbian" ]] || grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    log_info "Raspberry Pi detected - configuring for HiFiBerry..."
    
    # Add I2C and SPI modules
    if ! grep -q "i2c-dev" /etc/modules; then
        echo "i2c-dev" | sudo tee -a /etc/modules
    fi
    
    # Configure boot config for HiFiBerry
    if ! grep -q "dtoverlay=hifiberry" /boot/config.txt 2>/dev/null; then
        log_info "Consider adding HiFiBerry overlay to /boot/config.txt:"
        log_info "  dtoverlay=hifiberry-dacplus"
    fi
fi

# Create summary file
log_info "Creating installation summary..."
cat > INSTALLATION_SUMMARY.md << EOF
# LVGL Dashboard Installation Summary

## Installation completed: $(date)

### Installed Dependencies:
- ✅ Build tools (GCC, CMake, Git)
- ✅ SDL2 development libraries  
- ✅ Audio system (ALSA, PulseAudio, Bluetooth)
- ✅ Python and BeoCreate tools
- ✅ Serial communication tools
- ✅ LVGL library (v9.0)

### Directory Structure:
- \`lvgl/\` - LVGL library
- \`beocreate-tools/\` - HiFiBerry BeoCreate tools
- \`include/lv_conf.h\` - LVGL configuration
- \`CMakeLists.txt\` - Build configuration

### Scripts Created:
- \`build.sh\` - Build the dashboard
- \`activate_beocreate.sh\` - Activate BeoCreate Python environment
- \`test_serial.sh\` - Test serial ports

### Next Steps:
1. **Logout and login** to apply group changes
2. Add your source files to \`src/\` directory
3. Add your UI files to \`ui/\` directory  
4. Run \`./build.sh\` to build
5. Test serial connection with \`./test_serial.sh\`

### BeoCreate Tools:
- Python environment: \`~/beocreate-env\`
- Activate with: \`source activate_beocreate.sh\`

### Troubleshooting:
- Serial permissions: Check \`groups\` command includes 'dialout'
- USB ports: Run \`./test_serial.sh\` to find available ports
- Build issues: Check CMakeLists.txt paths match your project structure
EOF

log_success "Installation completed successfully!"
log_info "Working directory: $WORK_DIR"
log_info "Next steps:"
log_info "  1. Logout and login to apply group changes"
log_info "  2. Copy your source files (main.cpp, ui files) to this directory"
log_info "  3. Run ./build.sh to build the project"
log_info "  4. Run ./test_serial.sh to find your ESP32 port"

log_warning "IMPORTANT: You must logout and login (or reboot) for serial port access to work!"

echo
log_info "Installation summary saved to: $WORK_DIR/INSTALLATION_SUMMARY.md"
