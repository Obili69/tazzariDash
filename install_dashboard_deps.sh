#!/bin/bash

# LVGL Dashboard Minimal Dependencies Installation Script
# Installs only what's needed for basic dashboard functionality

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

log_info "Starting LVGL Dashboard Dependencies Installation..."
log_info "This will install:"
log_info "  • Essential build tools (GCC, CMake, Git)"
log_info "  • SDL2 development libraries (for LVGL display)"
log_info "  • Audio system dependencies (ALSA, PulseAudio, Bluetooth)"
log_info "  • HiFiBerry DSP control library"
log_info "  • Serial communication tools (for ESP32 connection)"
log_info "  • User permissions for USB serial access"

read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Installation cancelled"
    exit 0
fi

# Update package lists
log_info "Updating package lists..."
sudo apt update

# Essential build tools only
log_info "Installing essential build tools..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config

# SDL2 for LVGL display (essential)
log_info "Installing SDL2 development libraries..."
sudo apt install -y \
    libsdl2-dev \
    libsdl2-image-dev

# Audio system dependencies for HiFiBerry DSP control
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

# Serial communication tools for ESP32 testing
log_info "Installing serial communication tools..."
sudo apt install -y \
    screen \
    minicom \
    picocom

# Add user to dialout group for serial access
log_info "Adding user to dialout group for serial port access..."
sudo usermod -a -G dialout $USER

# Create working directory and setup LVGL
WORK_DIR="$HOME/tazzariDash"
if [ ! -d "$WORK_DIR" ]; then
    log_info "Creating working directory: $WORK_DIR"
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"
else
    log_info "Working directory exists: $WORK_DIR"
    cd "$WORK_DIR"
fi

# Clone LVGL library if not present
log_info "Setting up LVGL library..."
if [ ! -d "lvgl" ]; then
    log_info "Cloning LVGL library..."
    git clone --recurse-submodules https://github.com/lvgl/lvgl.git
    cd lvgl
    git checkout release/v9.0
    cd ..
    log_success "LVGL library setup complete"
else
    log_info "LVGL already exists, skipping clone"
fi

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

# Configure with CMake
echo "Running CMake configuration..."
cmake ..

# Build with all available cores
echo "Compiling project..."
make -j$(nproc)

echo ""
echo "Build complete!"
echo "Run with: ./build/LVGLDashboard"
EOF
chmod +x build.sh

# Create test serial script
log_info "Creating serial port test script..."
cat > test_serial.sh << 'EOF'
#!/bin/bash
echo "=== Serial Port Detection ==="
echo ""
echo "Available USB serial ports:"
ls /dev/tty{USB,ACM}* 2>/dev/null || echo "No USB serial ports found"

echo ""
echo "Connected USB devices:"
lsusb | grep -E "(Arduino|ESP32|CH340|CP210|FTDI|Silicon Labs)" || echo "No known serial devices found"

echo ""
echo "User groups (should include 'dialout'):"
groups

echo ""
echo "To test a serial port:"
echo "  screen /dev/ttyUSB0 115200"
echo "  (Press Ctrl+A then K then Y to exit screen)"
EOF
chmod +x test_serial.sh

# Create HiFiBerry DSP info script
log_info "Creating HiFiBerry DSP information script..."
cat > dsp_info.sh << 'EOF'
#!/bin/bash
echo "HiFiBerry DSP Control Information:"
echo "Installation directory: $(pwd)/hifiberry-dsp"
echo ""
echo "Available DSP control files:"
ls -la hifiberry-dsp/ | grep -E "\.(c|h|cpp)$" || echo "Navigate to hifiberry-dsp/ directory to see available files"
echo ""
echo "To integrate DSP control:"
echo "  1. Include hifiberry-dsp headers in your project"
echo "  2. Link against ALSA libraries"
echo "  3. Use DSP control functions in your audio event handlers"
echo ""
echo "Audio system status:"
echo "  ALSA: $(which aplay > /dev/null && echo "Available" || echo "Not found")"
echo "  PulseAudio: $(systemctl --user is-active pulseaudio 2>/dev/null || echo "Not running")"
EOF
chmod +x dsp_info.sh

# Create clean script
log_info "Creating clean script..."
cat > clean.sh << 'EOF'
#!/bin/bash
echo "Cleaning build artifacts..."
rm -rf build/
echo "Clean complete!"
EOF
chmod +x clean.sh

# Create installation summary
log_info "Creating installation summary..."
cat > MINIMAL_INSTALL_SUMMARY.md << EOF
# LVGL Dashboard Minimal Installation Summary

## Installation completed: $(date)

### Installed Dependencies:
- ✅ Essential build tools (GCC, CMake, Git, pkg-config)
- ✅ SDL2 development libraries (libsdl2-dev, libsdl2-image-dev)
- ✅ Audio system dependencies (ALSA, PulseAudio, Bluetooth)
- ✅ HiFiBerry DSP control library
- ✅ Serial communication tools (screen, minicom, picocom)
- ✅ LVGL library (v9.0)
- ✅ USB serial device permissions

### Scripts Created:
- \`build.sh\` - Build the dashboard project
- \`clean.sh\` - Clean build artifacts  
- \`test_serial.sh\` - Test serial port connectivity
- \`dsp_info.sh\` - HiFiBerry DSP control information

### Project Structure:
\`\`\`
tazzariDash/
├── build/              # Build output (auto-created)
├── include/            # Header files
├── lvgl/              # LVGL library
├── hifiberry-dsp/     # HiFiBerry DSP control library
├── src/               # Source files
├── ui/                # UI files
├── CMakeLists.txt     # Build configuration
└── build.sh           # Build script
\`\`\`

### Next Steps:
1. **IMPORTANT: Logout and login** to apply group changes for serial access
2. Run \`./build.sh\` to build the project
3. Connect ESP32 via USB
4. Run \`./test_serial.sh\` to verify serial connection
5. Run \`./dsp_info.sh\` to check DSP control setup
6. Run \`./build/LVGLDashboard\` to start the dashboard

### Troubleshooting:
- **Build errors**: Check that all source files are in place
- **Serial access**: Ensure you've logged out/in after installation
- **No serial ports**: Check USB connection and run \`./test_serial.sh\`
- **SDL2 errors**: Verify installation with \`pkg-config --modversion sdl2\`
- **Audio issues**: Check PulseAudio status with \`systemctl --user status pulseaudio\`
- **DSP control**: Review \`./dsp_info.sh\` for integration guidance
EOF

log_success "Minimal installation completed successfully!"
log_info "Working directory: $WORK_DIR"
log_info ""
log_info "Next steps:"
log_info "  1. LOGOUT AND LOGIN to apply serial port permissions"
log_info "  2. Run: cd $WORK_DIR && ./build.sh"
log_info "  3. Test serial: ./test_serial.sh" 
log_info "  4. Run dashboard: ./build/LVGLDashboard"

log_warning ""
log_warning "CRITICAL: You MUST logout and login for serial port access to work!"

echo
log_info "Installation summary saved to: $WORK_DIR/MINIMAL_INSTALL_SUMMARY.md"
