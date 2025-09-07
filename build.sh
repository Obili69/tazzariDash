#!/bin/bash
# build.sh - Build LVGL Dashboard with selectable audio hardware and auto-start
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
SETUP_AUTOSTART=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment|--prod)
            DEPLOYMENT_MODE=true
            BUILD_TYPE="Release"
            SETUP_AUTOSTART=true  # Auto-enable autostart for deployment builds
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
        --autostart)
            SETUP_AUTOSTART=true
            shift
            ;;
        --no-autostart)
            SETUP_AUTOSTART=false
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script with Multi-Audio Hardware Support"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Options:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen + autostart)"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack (PulseAudio + alsaeq)"
            echo "  --audio-dac         HiFiBerry DAC+ (ALSA hardware volume + alsaeq)"
            echo "  --audio-amp4        HiFiBerry AMP4 (ALSA hardware volume + alsaeq)"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (DSP REST API, default)"
            echo ""
            echo "Autostart Options:"
            echo "  --autostart         Enable autostart (automatic for --deployment)"
            echo "  --no-autostart      Disable autostart"
            echo ""
            echo "Examples:"
            echo "  $0 --dev --audio-amp4           # Development build with AMP4"
            echo "  $0 --deployment --audio-dac     # Production build with DAC+"
            echo "  $0 --deployment --audio-beocreate4 --no-autostart  # Production without autostart"
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
log_info "Autostart: $([ "$SETUP_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")"

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

# Check prerequisites (existing code remains the same)
log_info "Checking prerequisites..."

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

# Check dependencies based on audio hardware (existing code)
log_info "Checking audio dependencies for $AUDIO_HARDWARE..."

case $AUDIO_HARDWARE in
    "AUX")
        if ! pkg-config --exists libpulse; then
            echo "Error: PulseAudio development files not found!"
            echo "Install with: sudo apt install libpulse-dev libasound2-plugin-equal"
            exit 1
        fi
        log_success "PulseAudio development files found"
        ;;
        
    "DAC"|"AMP4")
        if ! pkg-config --exists alsa; then
            echo "Error: ALSA development files not found!"
            echo "Install with: sudo apt install libasound2-dev libasound2-plugin-equal"
            exit 1
        fi
        log_success "ALSA development files found"
        ;;
        
    "BEOCREATE4")
        if ! pkg-config --exists libcurl; then
            echo "Error: libcurl not found!"
            echo "Install with: sudo apt install libcurl4-openssl-dev"
            exit 1
        fi
        log_success "libcurl found"
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

# Create run script with audio hardware check (existing code)
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
        if ! pulseaudio --check 2>/dev/null; then
            echo "Starting PulseAudio..."
            pulseaudio --start
        fi
        ;;
    "DAC"|"AMP4")
        echo "Using HiFiBerry $AUDIO_HARDWARE..."
        if ! aplay -l | grep -q hifiberry 2>/dev/null; then
            echo "Warning: HiFiBerry device not detected in ALSA"
            echo "Check boot configuration: dtoverlay=hifiberry-dacplus-std"
        fi
        ;;
    "BEOCREATE4")
        echo "Using HiFiBerry BeoCreate 4..."
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

# Setup autostart if requested
if [ "$SETUP_AUTOSTART" = true ]; then
    log_info "Setting up autostart for deployment..."
    
    # Create autostart directory
    mkdir -p ~/.config/autostart
    
    # Get absolute paths
    DASHBOARD_PATH="$(pwd)"
    EXECUTABLE_FULL_PATH="$DASHBOARD_PATH/$EXECUTABLE"
    
    # Remove any existing autostart configurations to avoid conflicts
    log_info "Removing any existing autostart configurations..."
    rm -f ~/.config/autostart/tazzari-dashboard-*.desktop 2>/dev/null || true
    rm -f ~/start_tazzari_dashboard_*.sh 2>/dev/null || true
    
    # Create startup script
    log_info "Creating startup script for $AUDIO_HARDWARE..."
    cat > ~/start_tazzari_dashboard_${AUDIO_HARDWARE,,}.sh << EOF
#!/bin/bash
# TazzariAudio Dashboard auto-start for $AUDIO_HARDWARE

echo "=== TazzariAudio Dashboard Auto-Start ==="
echo "Audio Hardware: $AUDIO_HARDWARE"
echo "Build: Deployment Mode"

# Wait for desktop environment
sleep 15

cd "$DASHBOARD_PATH"

# Hardware-specific startup checks
case "$AUDIO_HARDWARE" in
    "AUX")
        echo "Checking PulseAudio..."
        if ! pulseaudio --check 2>/dev/null; then
            echo "Starting PulseAudio..."
            pulseaudio --start
            sleep 3
        fi
        ;;
    "DAC"|"AMP4")
        echo "Checking HiFiBerry $AUDIO_HARDWARE..."
        for i in {1..10}; do
            if aplay -l | grep -q hifiberry 2>/dev/null; then
                echo "✓ HiFiBerry device detected"
                break
            fi
            sleep 2
        done
        ;;
    "BEOCREATE4")
        echo "Waiting for BeoCreate 4 DSP..."
        for i in {1..20}; do
            if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
                echo "✓ DSP REST API ready"
                break
            fi
            sleep 3
        done
        ;;
esac

# Start dashboard with restart capability
echo "Starting TazzariAudio Dashboard..."
while true; do
    echo "\$(date): Starting dashboard..."
    $EXECUTABLE_FULL_PATH
    echo "\$(date): Dashboard exited, restarting in 3 seconds..."
    sleep 3
done
EOF
    chmod +x ~/start_tazzari_dashboard_${AUDIO_HARDWARE,,}.sh
    
    # Create desktop autostart entry
    cat > ~/.config/autostart/tazzari-dashboard-${AUDIO_HARDWARE,,}.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard ($AUDIO_HARDWARE)
Exec=/home/$(whoami)/start_tazzari_dashboard_${AUDIO_HARDWARE,,}.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Comment=TazzariAudio Dashboard with $AUDIO_HARDWARE audio hardware
EOF
    
    # Hide cursor for deployment builds
    if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
        log_info "Hiding mouse cursor for deployment..."
        sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak 2>/dev/null || true
    fi
    
    # Create control scripts
    cat > stop_dashboard_${AUDIO_HARDWARE,,}.sh << 'EOF'
#!/bin/bash
echo "Stopping TazzariAudio Dashboard..."
pkill -f "LVGLDashboard_.*_deployment"
pkill -f "start_tazzari_dashboard"
echo "Dashboard stopped"
EOF
    chmod +x stop_dashboard_${AUDIO_HARDWARE,,}.sh
    
    cat > show_cursor.sh << 'EOF'
#!/bin/bash
echo "Restoring mouse cursor..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr 2>/dev/null
    echo "✓ Cursor restored (restart desktop to see effect)"
else
    echo "- Cursor backup not found"
fi
EOF
    chmod +x show_cursor.sh
    
    cat > disable_autostart_${AUDIO_HARDWARE,,}.sh << EOF
#!/bin/bash
echo "Disabling TazzariAudio Dashboard autostart..."
rm -f ~/.config/autostart/tazzari-dashboard-${AUDIO_HARDWARE,,}.desktop
./stop_dashboard_${AUDIO_HARDWARE,,}.sh
echo "✓ Autostart disabled"
EOF
    chmod +x disable_autostart_${AUDIO_HARDWARE,,}.sh
    
    log_success "Autostart configured!"
    log_info ""
    log_info "Autostart Control Scripts:"
    log_info "  ./stop_dashboard_${AUDIO_HARDWARE,,}.sh      # Stop dashboard"
    log_info "  ./show_cursor.sh                   # Show cursor for debugging"
    log_info "  ./disable_autostart_${AUDIO_HARDWARE,,}.sh   # Disable autostart"
    log_info ""
fi

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Run script: ./$RUN_SCRIPT"

if [ "$SETUP_AUTOSTART" = true ]; then
    echo "Autostart: ✓ Enabled (starts on boot)"
    echo ""
    echo "Test autostart: sudo reboot"
    echo "Or run manually: ./$RUN_SCRIPT"
else
    echo "Autostart: Disabled (manual start only)"
    echo ""
    echo "Run manually: ./$RUN_SCRIPT"
fi

echo ""

# Show system status for selected audio hardware (existing code)
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
            
            DSP_RESPONSE=$(curl -s http://localhost:13141/metadata 2>/dev/null)
            if echo "$DSP_RESPONSE" | grep -q "profileName"; then
                PROFILE_NAME=$(echo "$DSP_RESPONSE" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
                echo "  ✓ DSP Profile: $PROFILE_NAME"
            else
                echo "  - No DSP profile loaded (run ./setup_dsp_beocreate4.sh)"
            fi
        else
            echo "  - DSP REST API not responding"
            echo "    Run: sudo systemctl start sigmatcpserver"
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
if [ "$SETUP_AUTOSTART" = true ]; then
    echo "🚗 TazzariAudio Dashboard ready for deployment!"
    echo "   Reboot to test autostart: sudo reboot"
else
    echo "🚗 TazzariAudio Dashboard ready for development!"
    echo "   Run manually: ./$RUN_SCRIPT"
fi