# LVGL Dashboard Manual Installation Guide

This guide provides step-by-step manual installation instructions for all dependencies needed to build and run the LVGL automotive dashboard.

## Prerequisites

- Ubuntu 20.04+ or Raspberry Pi OS
- Internet connection
- User account with sudo privileges
- Basic terminal knowledge

## Step 1: Update System

```bash
# Update package lists
sudo apt update
sudo apt upgrade -y
```

## Step 2: Install Essential Build Tools

```bash
# Core development tools
sudo apt install -y build-essential
sudo apt install -y cmake
sudo apt install -y git
sudo apt install -y pkg-config
sudo apt install -y wget
sudo apt install -y curl
sudo apt install -y unzip

# Python development
sudo apt install -y python3
sudo apt install -y python3-pip
sudo apt install -y python3-dev
sudo apt install -y python3-venv
sudo apt install -y python3-setuptools
sudo apt install -y python3-wheel
```

## Step 3: Install SDL2 Libraries

```bash
# SDL2 core and extensions for LVGL
sudo apt install -y libsdl2-dev
sudo apt install -y libsdl2-image-dev
sudo apt install -y libsdl2-mixer-dev
sudo apt install -y libsdl2-ttf-dev
```

## Step 4: Install Audio System Dependencies

```bash
# ALSA (Advanced Linux Sound Architecture)
sudo apt install -y libasound2-dev
sudo apt install -y alsa-utils

# PulseAudio
sudo apt install -y libpulse-dev
sudo apt install -y pulseaudio
sudo apt install -y pulseaudio-utils

# Bluetooth audio support
sudo apt install -y libbluetooth-dev
sudo apt install -y bluez
sudo apt install -y bluez-tools
sudo apt install -y pulseaudio-module-bluetooth
```

## Step 5: Install Additional Development Libraries

```bash
# Security and communication libraries
sudo apt install -y libssl-dev
sudo apt install -y libffi-dev
sudo apt install -y libudev-dev
sudo apt install -y libusb-1.0-0-dev
```

## Step 6: Install Serial Communication Tools

```bash
# Terminal emulators for serial communication
sudo apt install -y screen
sudo apt install -y minicom
sudo apt install -y picocom
sudo apt install -y setserial
```

## Step 7: Configure User Permissions

```bash
# Add current user to dialout group for serial port access
sudo usermod -a -G dialout $USER

# Note: You must logout and login (or reboot) for this to take effect
echo "Remember to logout/login after installation for serial access!"
```

## Step 8: Create USB Serial Device Rules

```bash
# Create udev rules file for common USB serial adapters
sudo tee /etc/udev/rules.d/99-usb-serial.rules > /dev/null << 'EOF'
# ESP32 and Arduino USB serial devices
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="239a", MODE="0666", GROUP="dialout"
EOF

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Step 9: Set Up Project Directory

```bash
# Create main working directory
mkdir -p ~/lvgl-dashboard
cd ~/lvgl-dashboard
```

## Step 10: Install LVGL Library

```bash
# Clone LVGL with submodules
git clone --recurse-submodules https://github.com/lvgl/lvgl.git
cd lvgl
git checkout release/v9.0
cd ..
```

## Step 11: Install BeoCreate Tools

```bash
# Clone and install BeoCreate tools using official installer
git clone https://github.com/hifiberry/beocreate-tools
cd beocreate-tools
./install-all
cd ..
```

## Step 12: Create Project Configuration Files

### Create CMakeLists.txt
```bash
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
```

### Create LVGL Configuration Header
```bash
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
```

## Step 13: Create Helper Scripts

### Build Script
```bash
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
```

### Serial Port Testing Script
```bash
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
```

### BeoCreate Information Script
```bash
cat > beocreate_info.sh << 'EOF'
#!/bin/bash
echo "BeoCreate Tools Information:"
echo "Installation directory: $(pwd)/beocreate-tools"
echo ""
echo "Available tools:"
ls -la beocreate-tools/ | grep -E "\.(py|sh)$" || echo "Navigate to beocreate-tools/ directory to see available tools"
echo ""
echo "To use BeoCreate tools:"
echo "  cd beocreate-tools"
echo "  python3 <tool_name>.py"
echo ""
echo "SigmaTCP server (if available):"
echo "  sudo systemctl status sigmatcp"
EOF
chmod +x beocreate_info.sh
```

## Step 14: Raspberry Pi Specific Configuration (Optional)

Only run this section if you're installing on a Raspberry Pi:

```bash
# Check if running on Raspberry Pi
if grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    echo "Raspberry Pi detected - applying Pi-specific configuration"
    
    # Enable I2C for HiFiBerry
    if ! grep -q "i2c-dev" /etc/modules 2>/dev/null; then
        echo "i2c-dev" | sudo tee -a /etc/modules
    fi
    
    # Note about boot configuration
    echo "Consider adding HiFiBerry overlay to /boot/config.txt:"
    echo "  dtoverlay=hifiberry-dacplus"
    echo "  (Edit /boot/config.txt manually if using HiFiBerry hardware)"
else
    echo "Not running on Raspberry Pi - skipping Pi-specific configuration"
fi
```

## Step 15: Create Project Structure

```bash
# Create source directories
mkdir -p src
mkdir -p ui
mkdir -p build

# Create placeholder files to maintain directory structure
touch src/.gitkeep
touch ui/.gitkeep
```

## Step 16: Verify Installation

### Test SDL2 Installation
```bash
# Check if SDL2 is properly installed
pkg-config --modversion sdl2
echo "SDL2 version check complete"
```

### Test Serial Port Access
```bash
# Run the serial test script
./test_serial.sh
```

### Test Build System
```bash
# Note: This will fail until you add source files, but tests CMake configuration
mkdir -p build
cd build
cmake .. || echo "CMake configuration test complete (source files needed)"
cd ..
```

## Step 17: Final System Configuration

```bash
# Ensure audio system is running
sudo systemctl enable pulseaudio
sudo systemctl start pulseaudio

# For Bluetooth audio (optional)
sudo systemctl enable bluetooth
sudo systemctl start bluetooth
```

## Completion Checklist

After completing all steps, verify:

- [ ] All packages installed without errors
- [ ] User added to dialout group
- [ ] LVGL library cloned (v9.0 branch)
- [ ] BeoCreate tools installed
- [ ] CMakeLists.txt and lv_conf.h created
- [ ] Helper scripts created and executable
- [ ] Project directories created

## Next Steps

1. **Logout and login** to apply group permissions
2. Add your source files (main.cpp) to the `src/` directory
3. Add your UI files to the `ui/` directory
4. Run `./build.sh` to build the project
5. Connect your ESP32 and run `./test_serial.sh` to find the correct port
6. Run the dashboard with `./build/LVGLDashboard`

## Troubleshooting

### Build Issues
- Ensure all source files are in the correct directories
- Check that CMakeLists.txt paths match your project structure
- Verify SDL2 installation with `pkg-config --modversion sdl2`

### Serial Port Issues
- Run `groups` to verify dialout group membership
- Check available ports with `ls /dev/tty*`
- Test permissions with `ls -l /dev/ttyUSB0`

### Audio Issues
- Check PulseAudio status: `systemctl --user status pulseaudio`
- Test audio output: `speaker-test -c 2`
- For HiFiBerry: Verify device tree overlays in `/boot/config.txt`

### BeoCreate Issues
- Navigate to `beocreate-tools/` directory for available tools
- Check SigmaTCP service: `sudo systemctl status sigmatcp`
- Review BeoCreate documentation in the cloned repository

## Additional Resources

- LVGL Documentation: https://docs.lvgl.io/
- SDL2 Documentation: https://wiki.libsdl.org/
- BeoCreate Tools: https://github.com/hifiberry/beocreate-tools
- HiFiBerry Documentation: https://www.hifiberry.com/docs/
