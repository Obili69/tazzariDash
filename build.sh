#!/bin/bash
# build.sh - Build script for LVGL Dashboard with SimplifiedAudioManager

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[BUILD]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

# Default configuration
BUILD_TYPE="Debug"
DEPLOYMENT_MODE=false
SIMPLE_AUDIO_ENABLED=true

# Parse arguments
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
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --no-audio)
            SIMPLE_AUDIO_ENABLED=false
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build modes:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen)"
            echo "  --release           Force release build type"
            echo ""
            echo "Features:"
            echo "  --no-audio          Disable audio support"
            echo ""
            echo "Examples:"
            echo "  $0                  # Debug build with audio"
            echo "  $0 --deployment     # Production build"
            echo "  $0 --dev --no-audio # Debug without audio"
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
log_info "Audio: $([ "$SIMPLE_AUDIO_ENABLED" = true ] && echo "Simplified Audio Manager" || echo "Disabled")"

# Check prerequisites
log_info "Checking prerequisites..."

if [ ! -d "src" ] || [ ! -f "src/main.cpp" ]; then
    echo "Error: Source files not found!"
    echo "Expected directory structure:"
    echo "  src/main.cpp"
    echo "  src/SimplifiedAudioManager.cpp"
    echo "  src/SerialCommunication.cpp"
    echo "  include/[headers]"
    echo "  ui/[ui files]"
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

if [ "$SIMPLE_AUDIO_ENABLED" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DENABLE_SIMPLE_AUDIO=ON"
    
    # Check audio dependencies
    if ! which amixer >/dev/null 2>&1; then
        log_warning "ALSA tools missing - installing..."
        sudo apt install -y alsa-utils
    fi
    
    if ! which bluetoothctl >/dev/null 2>&1; then
        log_warning "Bluetooth tools missing - installing..."
        sudo apt install -y bluetooth bluez
    fi
else
    CMAKE_ARGS="$CMAKE_ARGS -DENABLE_SIMPLE_AUDIO=OFF"
fi

log_info "Running CMake: cmake $CMAKE_ARGS .."
cmake $CMAKE_ARGS ..

# Build
log_info "Compiling with $(nproc) cores..."
make -j$(nproc)

cd ..

# Determine executable name
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
# Generated run script
if [ -f "$EXECUTABLE" ]; then
    echo "Starting dashboard..."
    $EXECUTABLE
else
    echo "Executable not found. Run ./build.sh first."
fi
EOF
chmod +x "$RUN_SCRIPT"

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Quick run: ./$RUN_SCRIPT"
echo ""

if [ "$SIMPLE_AUDIO_ENABLED" = true ]; then
    echo "Simplified Audio Manager enabled:"
    echo "  • Volume control via BeoCreate DSP or ALSA"
    echo "  • Basic Bluetooth controls (play/pause/skip)"
    echo "  • Pi appears as 'TazzariAudio' on phones"
    echo "  • Much more reliable than complex setup"
    echo ""
    echo "Audio setup:"
    echo "  ./test_simplified_audio.sh  # Test audio systems"
    echo "  sudo systemctl start bluetooth  # Enable Bluetooth"
fi

echo ""
echo "Ready to run! Connect your ESP32 and start the dashboard."