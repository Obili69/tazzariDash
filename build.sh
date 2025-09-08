#!/bin/bash
# build.sh - Build LVGL Dashboard with lightning-fast autostart

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
SETUP_FAST_AUTOSTART=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment|--prod)
            DEPLOYMENT_MODE=true
            BUILD_TYPE="Release"
            SETUP_FAST_AUTOSTART=true  # Auto-enable fast autostart for deployment builds
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
        --fast-autostart)
            SETUP_FAST_AUTOSTART=true
            shift
            ;;
        --no-autostart)
            SETUP_FAST_AUTOSTART=false
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Options:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen + fast autostart)"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack"
            echo "  --audio-dac         HiFiBerry DAC+"
            echo "  --audio-amp4        HiFiBerry AMP4"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (default)"
            echo ""
            echo "Autostart Options:"
            echo "  --fast-autostart    Enable lightning-fast autostart"
            echo "  --no-autostart      Disable autostart"
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
log_info "Fast Autostart: $([ "$SETUP_FAST_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")"

# Check prerequisites
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

# Check dependencies based on audio hardware
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

# Create run script with audio hardware check
cat > "$RUN_SCRIPT" << RUNEOF
#!/bin/bash
# Auto-generated run script for $AUDIO_HARDWARE hardware

echo "Starting LVGL Dashboard with $AUDIO_HARDWARE audio hardware..."

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found. Run ./build.sh first."
    exit 1
fi

# Audio hardware specific checks (NO TEST TONES)
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
export DISPLAY=:0
$EXECUTABLE
RUNEOF
chmod +x "$RUN_SCRIPT"

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Run script: ./$RUN_SCRIPT"

# Lightning-fast autostart setup
if [ "$SETUP_FAST_AUTOSTART" = true ]; then
    log_info "Setting up lightning-fast autostart..."
    
    # Get absolute paths
    DASHBOARD_PATH="$(pwd)"
    EXECUTABLE_FULL_PATH="$DASHBOARD_PATH/$EXECUTABLE"
    
    # Nuclear cleanup of existing autostart methods
    log_info "Cleaning up existing autostart methods..."
    pkill -f "LVGLDashboard" 2>/dev/null || true
    pkill -f "start_tazzari" 2>/dev/null || true
    sleep 1
    rm -f ~/.config/autostart/tazzari*.desktop 2>/dev/null || true
    rm -f ~/start_tazzari*.sh 2>/dev/null || true
    rm -f ~/start_dashboard*.sh 2>/dev/null || true
    systemctl --user disable tazzari-dashboard 2>/dev/null || true
    systemctl --user stop tazzari-dashboard 2>/dev/null || true
    sudo systemctl disable tazzari-dashboard* 2>/dev/null || true
    (crontab -l 2>/dev/null | grep -v "tazzari\|TazzariAudio\|start_dashboard\|LVGLDashboard") | crontab - 2>/dev/null || true
    log_success "✅ Cleanup complete"
    
    # Test manual start first (quick test)
    log_info "Testing manual start..."
    export DISPLAY=:0
    timeout 3s "$EXECUTABLE_FULL_PATH" && {
        log_success "✅ Manual start works!"
    } || {
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            log_success "✅ Manual start works (timeout reached)"
        else
            echo "❌ Manual start failed!"
            echo "Try: export DISPLAY=:0 && $EXECUTABLE_FULL_PATH"
            exit 1
        fi
    }
    
    # Create lightning-fast desktop autostart
    log_info "Creating lightning-fast desktop autostart..."
    mkdir -p ~/.config/autostart
    
    cat > ~/.config/autostart/tazzari-dashboard-fast.desktop << DESKTOPEOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard ($AUDIO_HARDWARE)
Comment=Lightning-fast automotive dashboard
Exec=$EXECUTABLE_FULL_PATH
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
StartupNotify=false
X-GNOME-Autostart-Delay=0
Path=$DASHBOARD_PATH
Categories=AudioVideo;Automotive;
Icon=applications-multimedia
DESKTOPEOF
    
    log_success "⚡ Lightning-fast desktop autostart configured!"
    
    # Create control scripts
    log_info "Creating control scripts..."
    
    # Status script
    cat > dashboard_status.sh << 'STATUSEOF'
#!/bin/bash
echo "=== TazzariAudio Dashboard Status ==="

if pgrep -f "LVGLDashboard" >/dev/null; then
    PID=$(pgrep -f "LVGLDashboard")
    UPTIME=$(ps -o etime= -p "$PID" 2>/dev/null | tr -d ' ')
    echo "Dashboard: ⚡ Running (PID: $PID, Uptime: $UPTIME)"
else
    echo "Dashboard: ❌ Not running"
fi

if [ -f ~/.config/autostart/tazzari-dashboard-fast.desktop ]; then
    echo "Autostart: ⚡ Lightning-fast enabled"
else
    echo "Autostart: ❌ Not configured"
fi
STATUSEOF
    chmod +x dashboard_status.sh
    
    # Stop script
    cat > stop_dashboard.sh << 'STOPEOF'
#!/bin/bash
echo "Stopping TazzariAudio Dashboard..."
pkill -f "LVGLDashboard" 2>/dev/null || true
sleep 1
if pgrep -f "LVGLDashboard" >/dev/null; then
    echo "⚠ Still running"
else
    echo "✅ Stopped"
fi
STOPEOF
    chmod +x stop_dashboard.sh
    
    # Manual start script
    cat > start_dashboard_manual.sh << MANUALEOF
#!/bin/bash
echo "Starting TazzariAudio Dashboard manually..."
export DISPLAY=:0
cd "$DASHBOARD_PATH"
exec "$EXECUTABLE_FULL_PATH"
MANUALEOF
    chmod +x start_dashboard_manual.sh
    
    # Disable autostart script
    cat > disable_fast_autostart.sh << 'DISABLEEOF'
#!/bin/bash
echo "Disabling lightning-fast autostart..."
rm -f ~/.config/autostart/tazzari-dashboard-fast.desktop
echo "✅ Fast autostart disabled"
DISABLEEOF
    chmod +x disable_fast_autostart.sh
    
    # Show cursor script
    cat > show_cursor.sh << 'CURSOREOF'
#!/bin/bash
echo "Showing cursor for debugging..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr 2>/dev/null
    echo "✅ Cursor restored"
else
    echo "- Cursor backup not found"
fi
CURSOREOF
    chmod +x show_cursor.sh
    
    # Hide cursor for deployment mode
    if [[ "$EXECUTABLE" == *"deployment"* ]]; then
        if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
            sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak 2>/dev/null || true
            log_success "✅ Mouse cursor hidden"
        fi
    fi
    
    log_success "⚡ Lightning-fast autostart setup complete!"
    echo ""
    echo "Control commands:"
    echo "  ./dashboard_status.sh         # Check status"
    echo "  ./stop_dashboard.sh           # Stop dashboard"
    echo "  ./start_dashboard_manual.sh   # Manual start"
    echo "  ./show_cursor.sh              # Debug mode"
    echo "  ./disable_fast_autostart.sh   # Disable autostart"
else
    echo "Fast Autostart: Disabled (manual start only)"
    echo ""
    echo "Run manually: ./$RUN_SCRIPT"
fi

echo ""
if [ "$SETUP_FAST_AUTOSTART" = true ]; then
    echo "🚗 TazzariAudio Dashboard ready for deployment!"
    echo "   ⚡ LIGHTNING-FAST autostart configured!"
    echo "   🧪 Test with: sudo reboot"
else
    echo "🚗 TazzariAudio Dashboard ready for development!"
    echo "   🎮 Run manually: ./$RUN_SCRIPT"
fi