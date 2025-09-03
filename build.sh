#!/bin/bash
# build.sh - Build LVGL Dashboard with selectable audio hardware

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[BUILD]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

# Default values
BUILD_TYPE="Debug"
DEPLOYMENT_MODE=false
AUDIO_HARDWARE="BEOCREATE4"

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
        --audio-aux)
            AUDIO_HARDWARE="AUX"
            shift
            ;;
        --audio-dac)
            AUDIO_HARDWARE="DAC"
            shift
            ;;
        --audio-amp4)
            AUDIO_HARDWARE="AMP4"
            shift
            ;;
        --audio-beocreate4)
            AUDIO_HARDWARE="BEOCREATE4"
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script with Multi-Audio Hardware Support"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Options:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen)"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack (PulseAudio + alsaeq)"
            echo "  --audio-dac         HiFiBerry DAC+ (ALSA hardware volume + alsaeq)"
            echo "  --audio-amp4        HiFiBerry AMP4 (ALSA hardware volume + alsaeq)"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (DSP REST API, default)"
            echo ""
            echo "Examples:"
            echo "  $0 --dev --audio-amp4           # Development build with AMP4"
            echo "  $0 --deployment --audio-dac     # Production build with DAC+"
            echo "  $0 --audio-aux                  # Development with built-in audio"
            echo ""
            echo "Hardware Requirements:"
            echo "  AUX:        No additional hardware (built-in 3.5mm)"
            echo "  DAC:        HiFiBerry DAC+ + external amplifier"
            echo "  AMP4:       HiFiBerry AMP4 (built-in amplifier)"
            echo "  BEOCREATE4: HiFiBerry BeoCreate 4 DSP (4-channel amp)"
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
log_info "Audio Hardware: $AUDIO_HARDWARE"

# Display audio hardware info
case $AUDIO_HARDWARE in
    "AUX")
        log_info "  → Built-in 3.5mm jack with PulseAudio"
        log_info "  → Software volume + EQ (alsaeq)"
        log_info "  → Boot config: dtparam=audio=on"
        ;;
    "DAC")
        log_info "  → HiFiBerry DAC+ (requires external amplifier)"
        log_info "  → Hardware volume via ALSA + software EQ"
        log_info "  → Boot config: dtoverlay=hifiberry-dacplus-std"
        ;;
    "AMP4")
        log_info "  → HiFiBerry AMP4 (built-in amplifier)"
        log_info "  → Hardware volume via ALSA + software EQ"
        log_info "  → Boot config: dtoverlay=hifiberry-dacplus-std"
        ;;
    "BEOCREATE4")
        log_info "  → HiFiBerry BeoCreate 4 DSP (4-channel amplifier)"
        log_info "  → Hardware volume + EQ via REST API"
        log_info "  → Boot config: dtoverlay=hifiberry-dac + DSP tools"
        ;;
esac

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
REQUIRED_HEADERS=("include/lv_conf.h" "include/MultiAudioManager.h" "include/SerialCommunication.h")
for header in "${REQUIRED_HEADERS[@]}"; do
    if [ ! -f "$header" ]; then
        echo "Error: Required header $header not found"
        exit 1
    fi
done

# Check for required source files
REQUIRED_SOURCES=("src/MultiAudioManager.cpp" "src/SerialCommunication.cpp")
for source in "${REQUIRED_SOURCES[@]}"; do
    if [ ! -f "$source" ]; then
        echo "Error: Required source file $source not found"
        exit 1
    fi
done

# Check dependencies based on audio hardware
log_info "Checking audio dependencies for $AUDIO_HARDWARE..."

case $AUDIO_HARDWARE in
    "AUX")
        # Check PulseAudio
        if ! pkg-config --exists libpulse; then
            echo "Error: PulseAudio development files not found!"
            echo "Install with: sudo apt install libpulse-dev libasound2-plugin-equal"
            exit 1
        fi
        log_success "PulseAudio development files found"
        
        # Check if alsaeq is available
        if ! dpkg -l | grep -q libasound2-plugin-equal; then
            log_warning "alsaeq plugin not installed (will install during runtime)"
        else
            log_success "alsaeq plugin available"
        fi
        ;;
        
    "DAC"|"AMP4")
        # Check ALSA
        if ! pkg-config --exists alsa; then
            echo "Error: ALSA development files not found!"
            echo "Install with: sudo apt install libasound2-dev libasound2-plugin-equal"
            exit 1
        fi
        log_success "ALSA development files found"
        
        # Check if alsaeq is available
        if ! dpkg -l | grep -q libasound2-plugin-equal; then
            log_warning "alsaeq plugin not installed (will install during runtime)"
        else
            log_success "alsaeq plugin available"
        fi
        ;;
        
    "BEOCREATE4")
        # Check libcurl
        if ! pkg-config --exists libcurl; then
            echo "Error: libcurl not found!"
            echo "Install with: sudo apt install libcurl4-openssl-dev"
            exit 1
        fi
        log_success "libcurl found"
        
        # Check SigmaTCP server
        if systemctl is-active sigmatcpserver >/dev/null 2>&1; then
            log_success "SigmaTCP server running"
        elif [ -f "/usr/bin/sigmatcpserver" ]; then
            log_warning "SigmaTCP server available but not running"
        else
            log_warning "HiFiBerry DSP tools not found (install with setup.sh)"
        fi
        ;;
esac

# Check SDL2 (common)
if ! pkg-config --exists sdl2; then
    echo "Error: SDL2 not found!"
    echo "Install with: sudo apt install libsdl2-dev"
    exit 1
else
    log_success "SDL2 found"
fi

# Create build directory
mkdir -p build
cd build

# Configure CMake
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE -DAUDIO_HARDWARE=$AUDIO_HARDWARE"

if [ "$DEPLOYMENT_MODE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=ON"
else
    CMAKE_ARGS="$CMAKE_ARGS -DDEPLOYMENT_BUILD=OFF"
fi

log_info "Running CMake with: $CMAKE_ARGS"
cmake $CMAKE_ARGS ..

# Build
log_info "Compiling with $(nproc) cores..."
make -j$(nproc)

cd ..

# Determine executable name and create run script
if [ "$DEPLOYMENT_MODE" = true ]; then
    EXECUTABLE="./build/LVGLDashboard_${AUDIO_HARDWARE}_deployment"
    RUN_SCRIPT="run_${AUDIO_HARDWARE,,}_deployment.sh"
else
    EXECUTABLE="./build/LVGLDashboard_${AUDIO_HARDWARE}_dev"
    RUN_SCRIPT="run_${AUDIO_HARDWARE,,}_dev.sh"
fi

# Create run script with audio hardware check
cat > "$RUN_SCRIPT" << EOF
#!/bin/bash
# Auto-generated run script for $AUDIO_HARDWARE hardware

echo "Starting LVGL Dashboard with $AUDIO_HARDWARE audio hardware..."

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found. Run ./build.sh first."
    exit 1
fi

# Audio hardware specific checks
case "$AUDIO_HARDWARE" in
    "AUX")
        echo "Using built-in 3.5mm jack..."
        # Check if PulseAudio is running
        if ! pulseaudio --check 2>/dev/null; then
            echo "Starting PulseAudio..."
            pulseaudio --start
        fi
        ;;
    "DAC"|"AMP4")
        echo "Using HiFiBerry $AUDIO_HARDWARE..."
        # Check ALSA configuration
        if ! aplay -l | grep -q hifiberry 2>/dev/null; then
            echo "Warning: HiFiBerry device not detected in ALSA"
            echo "Check boot configuration: dtoverlay=hifiberry-dacplus-std"
        fi
        ;;
    "BEOCREATE4")
        echo "Using HiFiBerry BeoCreate 4..."
        # Check DSP REST API
        if ! curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
            echo "Warning: BeoCreate 4 DSP REST API not responding"
            echo "Check: sudo systemctl status sigmatcpserver"
        fi
        ;;
esac

echo ""
$EXECUTABLE
EOF
chmod +x "$RUN_SCRIPT"

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Run script: ./$RUN_SCRIPT"
echo ""

# Show system status for selected audio hardware
case $AUDIO_HARDWARE in
    "AUX")
        echo "Built-in Audio Status:"
        if pulseaudio --check 2>/dev/null; then
            echo "  ✓ PulseAudio running"
        else
            echo "  - PulseAudio not running (will start automatically)"
        fi
        ;;
        
    "DAC"|"AMP4")
        echo "HiFiBerry $AUDIO_HARDWARE Status:"
        if aplay -l 2>/dev/null | grep -q hifiberry; then
            echo "  ✓ HiFiBerry device detected in ALSA"
            DEVICE_NAME=$(aplay -l | grep hifiberry | head -1 | cut -d':' -f2 | cut -d',' -f1)
            echo "  ✓ Device:$DEVICE_NAME"
        else
            echo "  - HiFiBerry device not detected"
            echo "    Add to /boot/firmware/config.txt:"
            echo "    dtparam=audio=off"
            echo "    dtoverlay=hifiberry-dacplus-std"
        fi
        ;;
        
    "BEOCREATE4")
        echo "HiFiBerry BeoCreate 4 Status:"
        if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
            echo "  ✓ DSP REST API active"
            echo "  ✓ Hardware volume control available"
            echo "  ✓ Hardware EQ available"
            
            # Check if profile is loaded
            DSP_RESPONSE=$(curl -s http://localhost:13141/metadata 2>/dev/null)
            if echo "$DSP_RESPONSE" | grep -q "profileName"; then
                PROFILE_NAME=$(echo "$DSP_RESPONSE" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
                echo "  ✓ DSP Profile: $PROFILE_NAME"
            else
                echo "  - No DSP profile loaded (run ./setup_dsp.sh)"
            fi
        else
            echo "  - DSP REST API not responding"
            echo "    Run: sudo systemctl start sigmatcpserver"
            echo "    Then: ./setup_dsp.sh"
        fi
        ;;
esac

# Bluetooth status (common)
if systemctl is-active a2dp-agent >/dev/null 2>&1; then
    echo "  ✓ A2DP agent active (Pi appears as 'TazzariAudio')"
    
    CONNECTED=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
    if [ "$CONNECTED" -gt 0 ]; then
        DEVICE=$(bluetoothctl devices Connected 2>/dev/null | head -1 | cut -d' ' -f3-)
        echo "  ✓ Connected: $DEVICE"
    else
        echo "  - No Bluetooth devices connected"
    fi
else
    echo "  - A2DP agent not active"
fi

echo ""
echo "Ready to run! Use ./$RUN_SCRIPT"