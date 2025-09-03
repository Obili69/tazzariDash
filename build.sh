#!/bin/bash
# build.sh - Build LVGL Dashboard with HiFiBerry BeoCreate 4 integration

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[BUILD]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

# Parse arguments
BUILD_TYPE="Debug"
DEPLOYMENT_MODE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment|--prod)
            DEPLOYMENT_MODE=true
            BUILD_TYPE="Release"
            shift
            ;;
        --dev|--debug)
            DEPLOYMENT_MODE=false
            BUILD_TYPE="Debug"
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script with HiFiBerry BeoCreate 4"
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen)"
            echo "  --help, -h          Show this help"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

log_info "Building LVGL Dashboard with HiFiBerry BeoCreate 4..."
log_info "Mode: $([ "$DEPLOYMENT_MODE" = true ] && echo "Deployment (fullscreen)" || echo "Development (windowed)")"
log_info "Type: $BUILD_TYPE"

# Check prerequisites
log_info "Checking prerequisites..."

# Check directory structure
if [ ! -d "src" ] || [ ! -f "src/main.cpp" ]; then
    echo "Error: Source files not found!"
    echo "Expected: src/main.cpp"
    exit 1
fi

if [ ! -d "lvgl" ]; then
    echo "Error: LVGL library not found!"
    echo "Run: git clone --recurse-submodules https://github.com/lvgl/lvgl.git"
    echo "Then: cd lvgl && git checkout release/v9.0"
    exit 1
fi

# Check for required headers
REQUIRED_HEADERS=("include/lv_conf.h" "include/SimplifiedAudioManager.h" "include/SerialCommunication.h")
for header in "${REQUIRED_HEADERS[@]}"; do
    if [ ! -f "$header" ]; then
        echo "Error: Required header $header not found"
        exit 1
    fi
done

# Check for required source files
REQUIRED_SOURCES=("src/SimplifiedAudioManager.cpp" "src/SerialCommunication.cpp")
for source in "${REQUIRED_SOURCES[@]}"; do
    if [ ! -f "$source" ]; then
        echo "Error: Required source file $source not found"
        exit 1
    fi
done

# Check dependencies
log_info "Checking dependencies..."

# Check for libcurl (required for REST API)
if ! pkg-config --exists libcurl; then
    echo "Error: libcurl not found!"
    echo "Install with: sudo apt install libcurl4-openssl-dev"
    exit 1
else
    log_success "libcurl found"
fi

# Check SDL2
if ! pkg-config --exists sdl2; then
    echo "Error: SDL2 not found!"
    echo "Install with: sudo apt install libsdl2-dev"
    exit 1
else
    log_success "SDL2 found"
fi

# Check ALSA
if ! pkg-config --exists alsa; then
    echo "Error: ALSA not found!"
    echo "Install with: sudo apt install libasound2-dev"
    exit 1
else
    log_success "ALSA found"
fi

# Check HiFiBerry DSP system
if systemctl is-active sigmatcpserver >/dev/null 2>&1; then
    log_success "SigmaTCP server running"
elif [ -f "/usr/bin/sigmatcpserver" ]; then
    log_warning "SigmaTCP server available but not running"
else
    log_warning "HiFiBerry DSP tools not found (install with setup.sh)"
fi

# Check if REST API is responding
if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
    log_success "BeoCreate 4 DSP REST API responding"
else
    log_warning "BeoCreate 4 DSP REST API not responding (check sigmatcpserver)"
fi

# Check Bluetooth
if systemctl is-active a2dp-agent >/dev/null 2>&1; then
    log_success "A2DP agent service active"
elif which bluetoothctl >/dev/null 2>&1; then
    log_warning "Bluetooth available but A2DP agent not running"
else
    log_warning "No Bluetooth detected"
fi

# Create build directory
mkdir -p build
cd build

# Configure CMake
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$DEPLOYMENT_MODE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=ON"
else
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=OFF"
fi

log_info "Running CMake..."
cmake $CMAKE_ARGS ..

# Build
log_info "Compiling with $(nproc) cores..."
make -j$(nproc)

cd ..

# Determine executable name and create run script
if [ "$DEPLOYMENT_MODE" = true ]; then
    EXECUTABLE="./build/LVGLDashboard_deployment"
    RUN_SCRIPT="run_deployment.sh"
else
    EXECUTABLE="./build/LVGLDashboard_dev"
    RUN_SCRIPT="run_dev.sh"
fi

# Create run script
cat > "$RUN_SCRIPT" << EOF
#!/bin/bash
if [ -f "$EXECUTABLE" ]; then
    echo "Starting LVGL Dashboard with HiFiBerry BeoCreate 4..."
    $EXECUTABLE
else
    echo "Executable not found. Run ./build.sh first."
    exit 1
fi
EOF
chmod +x "$RUN_SCRIPT"

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Run with: ./$RUN_SCRIPT"
echo ""

# Show system status
echo "System status:"

# HiFiBerry DSP status
if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
    echo "  ✓ BeoCreate 4 DSP REST API active"
    echo "  ✓ Hardware volume control via DSP"
    echo "  ✓ 3-band EQ control via DSP"
    
    # Check if profile is loaded
    DSP_RESPONSE=$(curl -s http://localhost:13141/metadata 2>/dev/null)
    if echo "$DSP_RESPONSE" | grep -q "profileName"; then
        PROFILE_NAME=$(echo "$DSP_RESPONSE" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
        echo "  ✓ DSP Profile: $PROFILE_NAME"
    else
        echo "  - No DSP profile loaded (run ./setup_dsp.sh)"
    fi
else
    echo "  - BeoCreate 4 DSP REST API not responding"
    echo "    Run: sudo systemctl start sigmatcpserver"
    echo "    Then: ./setup_dsp.sh"
fi

# Bluetooth status
if systemctl is-active a2dp-agent >/dev/null 2>&1; then
    echo "  ✓ A2DP agent active (Pi appears as 'TazzariAudio')"
    
    # Check if any devices are connected
    CONNECTED=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
    if [ "$CONNECTED" -gt 0 ]; then
        DEVICE=$(bluetoothctl devices Connected 2>/dev/null | head -1 | cut -d' ' -f3-)
        echo "  ✓ Connected: $DEVICE"
    else
        echo "  - No Bluetooth devices connected"
    fi
else
    echo "  - A2DP agent not active"
    echo "    Run: sudo systemctl start a2dp-agent"
fi

echo ""
echo "Ready to run! Connect your phone to 'TazzariAudio' for audio streaming."