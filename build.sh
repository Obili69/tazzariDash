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
            echo "LVGL Dashboard Build Script with Multi-Audio Hardware Support"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Options:"
            echo "  --dev, --debug      Debug build (windowed, default)"
            echo "  --deployment, --prod Release build (fullscreen + fast autostart)"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack (PulseAudio + alsaeq)"
            echo "  --audio-dac         HiFiBerry DAC+ (ALSA hardware volume + alsaeq)"
            echo "  --audio-amp4        HiFiBerry AMP4 (ALSA hardware volume + alsaeq)"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (DSP REST API, default)"
            echo ""
            echo "Autostart Options:"
            echo "  --fast-autostart    Enable lightning-fast autostart (automatic for --deployment)"
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
log_info "Fast Autostart: $([ "$SETUP_FAST_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")"

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
export DISPLAY=:0
$EXECUTABLE
EOF
chmod +x "$RUN_SCRIPT"

log_success "Build complete!"
echo ""
echo "Executable: $EXECUTABLE"
echo "Run script: ./$RUN_SCRIPT"

# Setup lightning-fast autostart if requested
if [ "$SETUP_FAST_AUTOSTART" = true ]; then
    log_info "Setting up lightning-fast autostart..."
    
    # Create the fast autostart setup script
    cat > setup_fast_autostart.sh << 'EOF'
#!/bin/bash
# setup_fast_autostart.sh - Setup the NUTS FAST desktop autostart

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[FAST]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

echo "=== Setup LIGHTNING-FAST Desktop Autostart ==="
echo ""

# Detect dashboard executable
DASHBOARD_EXECUTABLE=""
AUDIO_HARDWARE=""

# Look for dashboard executables
for hw in AUX AMP4 BEOCREATE4 DAC; do
    for mode in deployment dev; do
        EXEC_PATH="build/LVGLDashboard_${hw}_${mode}"
        if [ -f "$EXEC_PATH" ]; then
            DASHBOARD_EXECUTABLE="$EXEC_PATH"
            AUDIO_HARDWARE="$hw"
            MODE="$mode"
            break 2
        fi
    done
done

if [ -z "$DASHBOARD_EXECUTABLE" ]; then
    echo "❌ No dashboard executable found!"
    echo "Expected files like: build/LVGLDashboard_AUX_deployment"
    echo "Build first with: ./build.sh --deployment --audio-[hardware]"
    exit 1
fi

DASHBOARD_PATH="$(pwd)"
EXECUTABLE_FULL_PATH="$DASHBOARD_PATH/$DASHBOARD_EXECUTABLE"

echo "🎯 Found: $DASHBOARD_EXECUTABLE"
echo "🎯 Hardware: $AUDIO_HARDWARE"
echo "🎯 Mode: $MODE"
echo ""

# Step 1: Nuclear cleanup of existing autostart methods
log_info "Step 1: Cleaning up existing autostart methods..."

# Kill any running dashboard
pkill -f "LVGLDashboard" 2>/dev/null || true
pkill -f "start_tazzari" 2>/dev/null || true
sleep 1

# Remove desktop autostart files
rm -f ~/.config/autostart/tazzari*.desktop 2>/dev/null || true

# Remove startup scripts
rm -f ~/start_tazzari*.sh 2>/dev/null || true
rm -f ~/start_dashboard*.sh 2>/dev/null || true

# Disable systemd services
systemctl --user disable tazzari-dashboard 2>/dev/null || true
systemctl --user stop tazzari-dashboard 2>/dev/null || true
sudo systemctl disable tazzari-dashboard* 2>/dev/null || true

# Clear crontab entries
(crontab -l 2>/dev/null | grep -v "tazzari\|TazzariAudio\|start_dashboard\|LVGLDashboard") | crontab - 2>/dev/null || true

# Disable rc.local
sudo tee /etc/rc.local > /dev/null << 'RCEOF'
#!/bin/bash
exit 0
RCEOF
sudo chmod +x /etc/rc.local

log_success "✅ Cleanup complete"

# Step 2: Test manual start first
log_info "Step 2: Testing manual start..."

export DISPLAY=:0
echo "Testing: $EXECUTABLE_FULL_PATH"

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

# Step 3: Create LIGHTNING-FAST desktop autostart
log_info "Step 3: Creating lightning-fast desktop autostart..."

mkdir -p ~/.config/autostart

cat > ~/.config/autostart/tazzari-dashboard-fast.desktop << EOF
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
EOF

log_success "⚡ Lightning-fast desktop autostart configured!"

# Step 4: Create control scripts
log_info "Step 4: Creating control scripts..."

# Status script
cat > dashboard_status.sh << 'EOFSTAT'
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
    echo "Autostart: ⚡ Lightning-fast desktop autostart enabled"
else
    echo "Autostart: ❌ Not configured"
fi
EOFSTAT
chmod +x dashboard_status.sh

# Stop script
cat > stop_dashboard.sh << 'EOFSTOP'
#!/bin/bash
echo "Stopping TazzariAudio Dashboard..."
pkill -f "LVGLDashboard" 2>/dev/null || true
sleep 1
if pgrep -f "LVGLDashboard" >/dev/null; then
    echo "⚠ Still running"
else
    echo "✅ Stopped"
fi
EOFSTOP
chmod +x stop_dashboard.sh

# Manual start script
cat > start_dashboard_manual.sh << EOF2
#!/bin/bash
echo "Starting TazzariAudio Dashboard manually..."
export DISPLAY=:0
cd "$DASHBOARD_PATH"
exec "$EXECUTABLE_FULL_PATH"
EOF2
chmod +x start_dashboard_manual.sh

# Disable autostart script
cat > disable_fast_autostart.sh << 'EOFDIS'
#!/bin/bash
echo "Disabling lightning-fast autostart..."
rm -f ~/.config/autostart/tazzari-dashboard-fast.desktop
echo "✅ Fast autostart disabled"
EOFDIS
chmod +x disable_fast_autostart.sh

# Show cursor script (for debugging)
cat > show_cursor.sh << 'EOFCUR'
#!/bin/bash
echo "Showing cursor for debugging..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr 2>/dev/null
    echo "✅ Cursor restored"
else
    echo "- Cursor backup not found"
fi
EOFCUR
chmod +x show_cursor.sh

# Step 5: Configure auto-login if needed
log_info "Step 5: Checking auto-login configuration..."

if who | grep -q "$(whoami)"; then
    log_success "✅ User $(whoami) is logged in"
else
    log_warning "⚠ User not logged in - desktop autostart needs user login"
fi

if [ -f "/etc/lightdm/lightdm.conf" ] && grep -q "autologin-user=$(whoami)" /etc/lightdm/lightdm.conf 2>/dev/null; then
    log_success "✅ Auto-login configured"
else
    echo ""
    echo "⚠ Auto-login not configured. Desktop autostart requires user login."
    echo ""
    read -p "Configure auto-login for $(whoami)? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if command -v lightdm >/dev/null; then
            sudo mkdir -p /etc/lightdm
            
            # Backup existing config
            if [ -f "/etc/lightdm/lightdm.conf" ]; then
                sudo cp /etc/lightdm/lightdm.conf /etc/lightdm/lightdm.conf.backup
            fi
            
            # Create auto-login config
            sudo tee /etc/lightdm/lightdm.conf > /dev/null << EOFLIGHT
[Seat:*]
autologin-user=$(whoami)
autologin-user-timeout=0
user-session=LXDE-pi
EOFLIGHT
            log_success "✅ Auto-login configured for $(whoami)"
        else
            log_warning "lightdm not found - configure manually"
        fi
    fi
fi

# Step 6: Hide cursor for deployment mode
if [[ "$DASHBOARD_EXECUTABLE" == *"deployment"* ]]; then
    log_info "Step 6: Hiding cursor for deployment mode..."
    
    if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
        sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak 2>/dev/null || true
        log_success "✅ Mouse cursor hidden"
    else
        echo "- Cursor already hidden or not found"
    fi
fi

log_success "=== LIGHTNING-FAST AUTOSTART SETUP COMPLETE! ==="
echo ""
echo "⚡ **NUTS FAST desktop autostart configured!**"
echo ""
echo "What was set up:"
echo "  ✅ Direct executable call (no scripts, no delays)"
echo "  ✅ Desktop autostart (starts when desktop loads)"
echo "  ✅ Background flashing fast startup"
echo "  ✅ Clean control scripts"
echo ""
echo "Control commands:"
echo "  ./dashboard_status.sh         # Check status"
echo "  ./stop_dashboard.sh           # Stop dashboard"
echo "  ./start_dashboard_manual.sh   # Manual start"
echo "  ./show_cursor.sh              # Debug mode"
echo "  ./disable_fast_autostart.sh   # Disable autostart"
echo ""
echo "🚀 **Test the lightning speed:**"
echo "   sudo reboot"
echo ""
echo "Expected: Background flashing → dashboard appears INSTANTLY! ⚡"
EOF
    chmod +x setup_fast_autostart.sh
    
    # Run the fast autostart setup
    if ./setup_fast_autostart.sh; then
        log_success "⚡ Lightning-fast autostart configured!"
    else
        log_warning "Fast autostart setup failed, but build completed successfully"
    fi
else
    echo "Fast Autostart: Disabled (manual start only)"
    echo ""
    echo "Run manually: ./$RUN_SCRIPT"
    echo "Or setup autostart later: ./setup_fast_autostart.sh"
fi

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
if [ "$SETUP_FAST_AUTOSTART" = true ]; then
    echo "🚗 TazzariAudio Dashboard ready for deployment!"
    echo "   ⚡ LIGHTNING-FAST autostart configured!"
    echo "   🧪 Test with: sudo reboot"
else
    echo "🚗 TazzariAudio Dashboard ready for development!"
    echo "   🎮 Run manually: ./$RUN_SCRIPT"
fi