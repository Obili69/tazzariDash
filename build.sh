#!/bin/bash
# build.sh - Enhanced build script with Bluetooth audio support

set -e

# Default build mode
BUILD_MODE="prod"
BUILD_TYPE="Debug"
ENABLE_BLUETOOTH="ON"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment|--prod)
            BUILD_MODE="deployment"
            BUILD_TYPE="Release"
            shift
            ;;
        --dev|--development)
            BUILD_MODE="dev"
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-bluetooth)
            ENABLE_BLUETOOTH="OFF"
            shift
            ;;
        --bluetooth)
            ENABLE_BLUETOOTH="ON"
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script with Bluetooth Audio"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Modes:"
            echo "  --dev, --development    Development build (windowed, default)"
            echo "  --deployment, --prod    Deployment build (fullscreen)"
            echo ""
            echo "Build Types:"
            echo "  --debug                 Debug build (default for dev)"
            echo "  --release               Release build (default for deployment)"
            echo ""
            echo "Features:"
            echo "  --bluetooth             Enable Bluetooth audio (default)"
            echo "  --no-bluetooth          Disable Bluetooth audio"
            echo ""
            echo "Examples:"
            echo "  $0                      Development build with Bluetooth"
            echo "  $0 --deployment         Deployment build with Bluetooth"
            echo "  $0 --dev --no-bluetooth Development build without Bluetooth"
            exit 0
            ;;
        *)
            echo "Unknown option $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Set deployment build type to Release by default
if [[ "$BUILD_MODE" == "deployment" && "$BUILD_TYPE" == "Debug" ]]; then
    BUILD_TYPE="Release"
fi

echo "Building LVGL Dashboard with Bluetooth Audio..."
echo "Build Mode: $BUILD_MODE"
echo "Build Type: $BUILD_TYPE"
echo "Bluetooth: $ENABLE_BLUETOOTH"
echo ""

# Check if source files exist
if [ ! -f "src/main.cpp" ]; then
    echo "Error: src/main.cpp not found"
    echo "Please ensure source files are in the correct location:"
    echo "  src/main.cpp"
    echo "  src/BluetoothAudioManager.cpp"
    echo "  src/SerialCommunication.cpp"
    exit 1
fi

# Check for required header files
REQUIRED_HEADERS=(
    "include/lv_conf.h"
    "include/BluetoothAudioManager.h"
    "include/SerialCommunication.h"
)

for header in "${REQUIRED_HEADERS[@]}"; do
    if [ ! -f "$header" ]; then
        echo "Warning: $header not found - copying from current directory if available"
        
        # Try to find the header in current directory
        header_name=$(basename "$header")
        if [ -f "$header_name" ]; then
            mkdir -p "$(dirname "$header")"
            cp "$header_name" "$header"
            echo "Copied $header_name to $header"
        else
            echo "Error: Required header $header not found"
            exit 1
        fi
    fi
done

# Check if LVGL exists
if [ ! -d "lvgl" ]; then
    echo "Error: LVGL library not found"
    echo "Run ./install_dashboard_deps.sh first to install LVGL"
    exit 1
fi

# Check Bluetooth dependencies
if [[ "$ENABLE_BLUETOOTH" == "ON" ]]; then
    echo "Checking Bluetooth dependencies..."
    
    # Check if BlueZ is available
    if ! command -v bluetoothctl &> /dev/null; then
        echo "Warning: bluetoothctl not found - run ./setup_bluetooth_audio.sh"
    fi
    
    # Check if PulseAudio is available
    if ! command -v pulseaudio &> /dev/null; then
        echo "Warning: PulseAudio not found - run ./setup_bluetooth_audio.sh"
    fi
    
    # Check D-Bus development packages
    if ! pkg-config --exists dbus-1; then
        echo "Warning: D-Bus development libraries not found"
        echo "Installing D-Bus development packages..."
        sudo apt install -y libdbus-1-dev
    fi
fi

# Create build directory
mkdir -p build
cd build

# Configure CMake based on build mode and features
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_BLUETOOTH=$ENABLE_BLUETOOTH"

if [[ "$BUILD_MODE" == "deployment" ]]; then
    echo "Configuring for DEPLOYMENT (fullscreen)..."
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=ON"
else
    echo "Configuring for DEVELOPMENT (windowed)..."
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=OFF"
fi

echo "Running CMake with: $CMAKE_ARGS"
cmake $CMAKE_ARGS ..

echo ""
echo "Compiling project..."
make -j$(nproc)

echo ""
log_success "Build complete!"

# Show appropriate executable and instructions
if [[ "$BUILD_MODE" == "deployment" ]]; then
    EXECUTABLE="./build/LVGLDashboard_deployment"
    echo "Deployment executable: $EXECUTABLE"
    echo ""
    echo "To run in fullscreen:"
    echo "  $EXECUTABLE"
else
    EXECUTABLE="./build/LVGLDashboard_dev"
    echo "Development executable: $EXECUTABLE"
    echo ""
    echo "To run in development mode:"
    echo "  $EXECUTABLE"
fi

echo ""
echo "Build configuration summary:"
echo "  Mode: $BUILD_MODE"
echo "  Type: $BUILD_TYPE"
echo "  Display: $([ "$BUILD_MODE" == "deployment" ] && echo "Fullscreen" || echo "Windowed (1024x600)")"
echo "  Bluetooth: $ENABLE_BLUETOOTH"

if [[ "$ENABLE_BLUETOOTH" == "ON" ]]; then
    echo ""
    echo "Bluetooth Audio Features:"
    echo "  • Volume control via arc widget"
    echo "  • Play/pause/skip via buttons"
    echo "  • Auto-connects to paired devices"
    echo "  • Routes audio to 3.5mm jack"
    echo ""
    echo "To pair a device:"
    echo "  ./pair_bluetooth_device.sh"
    echo ""
    echo "To test audio routing:"
    echo "  ./route_audio_to_jack.sh"
fi

# Go back to project root
cd ..

echo ""
echo "Ready to run! Connect your ESP32 and start the dashboard."