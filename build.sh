#!/bin/bash
# build.sh - Build LVGL Dashboard with SimplifiedAudioManager

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
            echo "LVGL Dashboard Build Script"
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

log_info "Building LVGL Dashboard..."
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

# Check audio system (from setup.sh)
if [ -f "/usr/local/bin/beocreate-client" ]; then
    log_success "BeoCreate DSP tools detected"
elif which amixer >/dev/null 2>&1; then
    log_warning "ALSA found but no BeoCreate tools (run ./setup.sh for DSP control)"
else
    log_warning "No audio control detected (run ./setup.sh first)"
fi

# Check Bluetooth
if systemctl is-active a2dp-sink >/dev/null 2>&1; then
    log_success "A2DP sink service active"
elif which bluetoothctl >/dev/null 2>&1; then
    log_warning "Bluetooth available but A2DP sink not running (run ./setup.sh)"
else
    log_warning "No Bluetooth detected (run ./setup.sh first)"
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
    echo "Starting LVGL Dashboard..."
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

# Show audio status
echo "Audio system status:"
if [ -f "/usr/local/bin/beocreate-client" ]; then
    echo "  ✓ BeoCreate DSP control available"
    echo "  ✓ Hardware volume control via DSP"
    if systemctl is-active sigmatcp >/dev/null 2>&1; then
        echo "  ✓ SigmaTCP server running"
    else
        echo "  - SigmaTCP server not running (will start on next boot)"
    fi
else
    echo "  - No BeoCreate DSP control (software volume only)"
fi

if systemctl is-active a2dp-sink >/dev/null 2>&1; then
    echo "  ✓ A2DP sink active (Pi appears as 'TazzariAudio')"
else
    echo "  - A2DP sink not active"
fi

echo ""
echo "Ready to run! Connect your ESP32 and pair your phone to 'TazzariAudio'"