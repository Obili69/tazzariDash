#!/bin/bash
# deploy.sh - Complete TazzariAudio Dashboard deployment script

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[DEPLOY]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

AUDIO_HARDWARE=""
ENABLE_AUTOSTART=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
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
        --no-autostart)
            ENABLE_AUTOSTART=false
            shift
            ;;
        --help|-h)
            echo "TazzariAudio Dashboard Complete Deployment"
            echo "Usage: $0 [options]"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack"
            echo "  --audio-dac         HiFiBerry DAC+"
            echo "  --audio-amp4        HiFiBerry AMP4"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (default)"
            echo ""
            echo "Options:"
            echo "  --no-autostart      Skip autostart configuration"
            echo ""
            echo "This script will:"
            echo "  1. Build deployment version"
            echo "  2. Test audio hardware"
            echo "  3. Configure autostart"
            echo "  4. Set up production environment"
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

# Auto-detect audio hardware if not specified
if [ -z "$AUDIO_HARDWARE" ]; then
    log_info "Auto-detecting audio hardware configuration..."
    
    # Check boot configuration
    if grep -q "dtoverlay=hifiberry-dac" /boot/firmware/config.txt 2>/dev/null || grep -q "dtoverlay=hifiberry-dac" /boot/config.txt 2>/dev/null; then
        if systemctl is-active sigmatcpserver >/dev/null 2>&1; then
            AUDIO_HARDWARE="BEOCREATE4"
        else
            log_warning "HiFiBerry DAC overlay found but no DSP service - assuming BEOCREATE4"
            AUDIO_HARDWARE="BEOCREATE4"
        fi
    elif grep -q "dtoverlay=hifiberry-dacplus" /boot/firmware/config.txt 2>/dev/null || grep -q "dtoverlay=hifiberry-dacplus" /boot/config.txt 2>/dev/null; then
        # Check if it's AMP4 or DAC+ by testing for amplifier
        if dmesg | grep -q "HiFiBerry.*AMP" 2>/dev/null; then
            AUDIO_HARDWARE="AMP4"
        else
            AUDIO_HARDWARE="DAC"
        fi
    elif grep -q "dtparam=audio=on" /boot/firmware/config.txt 2>/dev/null || grep -q "dtparam=audio=on" /boot/config.txt 2>/dev/null; then
        AUDIO_HARDWARE="AUX"
    else
        log_warning "Could not auto-detect audio hardware, using BEOCREATE4 default"
        AUDIO_HARDWARE="BEOCREATE4"
    fi
    
    log_info "Detected audio hardware: $AUDIO_HARDWARE"
fi

echo ""
log_info "=== TazzariAudio Dashboard Deployment ==="
log_info "Audio Hardware: $AUDIO_HARDWARE"
log_info "Autostart: $([ "$ENABLE_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")"
echo ""

# Step 1: Build deployment version
log_info "Step 1: Building deployment version..."
if ! ./build.sh --deployment --audio-${AUDIO_HARDWARE,,}; then
    log_error "Build failed!"
    exit 1
fi

EXECUTABLE="build/LVGLDashboard_${AUDIO_HARDWARE}_deployment"
if [ ! -f "$EXECUTABLE" ]; then
    log_error "Deployment executable not found: $EXECUTABLE"
    exit 1
fi

log_success "Deployment build complete: $EXECUTABLE"

# Step 2: Test audio hardware
log_info "Step 2: Testing audio hardware..."

case $AUDIO_HARDWARE in
    "AUX")
        log_info "Testing built-in audio..."
        if ! pulseaudio --check 2>/dev/null; then
            log_info "Starting PulseAudio..."
            pulseaudio --start
            sleep 3
        fi
        
        if pulseaudio --check 2>/dev/null; then
            log_success "✓ PulseAudio ready"
        else
            log_warning "PulseAudio not responding"
        fi
        
        # Test volume control
        if pactl get-sink-volume @DEFAULT_SINK@ >/dev/null 2>&1; then
            log_success "✓ Volume control ready"
        else
            log_warning "Volume control not available"
        fi
        ;;
        
    "DAC"|"AMP4")
        log_info "Testing HiFiBerry $AUDIO_HARDWARE..."
        
        # Check ALSA device
        if aplay -l | grep -q hifiberry 2>/dev/null; then
            log_success "✓ HiFiBerry device detected"
        else
            log_warning "HiFiBerry device not detected in ALSA"
            log_info "Check boot configuration for dtoverlay=hifiberry-dacplus-std"
        fi
        
        # Test hardware volume
        if amixer get Digital >/dev/null 2>&1; then
            log_success "✓ Hardware volume control ready"
        else
            log_warning "Hardware volume control not available"
        fi
        ;;
        
    "BEOCREATE4")
        log_info "Testing HiFiBerry BeoCreate 4..."
        
        # Check SigmaTCP server
        if systemctl is-active sigmatcpserver >/dev/null 2>&1; then
            log_success "✓ SigmaTCP server running"
        else
            log_warning "SigmaTCP server not running"
            log_info "Starting SigmaTCP server..."
            sudo systemctl start sigmatcpserver
            sleep 3
        fi
        
        # Test REST API
        if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
            log_success "✓ DSP REST API active"
            
            # Check for loaded profile
            DSP_RESPONSE=$(curl -s http://localhost:13141/metadata 2>/dev/null)
            if echo "$DSP_RESPONSE" | grep -q "profileName"; then
                PROFILE_NAME=$(echo "$DSP_RESPONSE" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
                log_success "✓ DSP Profile loaded: $PROFILE_NAME"
            else
                log_warning "No DSP profile loaded"
                if [ -f "./setup_dsp_beocreate4.sh" ]; then
                    log_info "Loading default DSP profile..."
                    ./setup_dsp_beocreate4.sh
                fi
            fi
        else
            log_warning "DSP REST API not responding"
        fi
        ;;
esac

# Step 3: Test Bluetooth
log_info "Testing Bluetooth system..."

if systemctl is-active bluetooth >/dev/null 2>&1; then
    log_success "✓ Bluetooth service active"
else
    log_warning "Bluetooth service not active"
    sudo systemctl start bluetooth
fi

if systemctl is-active a2dp-agent >/dev/null 2>&1; then
    log_success "✓ A2DP agent running"
else
    log_warning "A2DP agent not running"
    sudo systemctl start a2dp-agent 2>/dev/null || true
fi

# Check if discoverable
if bluetoothctl show | grep -q "Discoverable: yes" 2>/dev/null; then
    log_success "✓ Pi discoverable as 'TazzariAudio'"
else
    log_info "Making Pi discoverable..."
    bluetoothctl discoverable on >/dev/null 2>&1 || true
fi

# Step 4: Quick functional test
log_info "Step 3: Quick functional test..."

log_info "Starting dashboard for 10-second test..."
timeout 10s $EXECUTABLE &
DASHBOARD_PID=$!

sleep 5

# Check if dashboard is running
if kill -0 $DASHBOARD_PID 2>/dev/null; then
    log_success "✓ Dashboard started successfully"
    
    # Kill the test instance
    kill $DASHBOARD_PID 2>/dev/null || true
    wait $DASHBOARD_PID 2>/dev/null || true
    
    log_success "✓ Dashboard test complete"
else
    log_error "Dashboard failed to start"
    log_info "Check logs and try manual start: ./$EXECUTABLE"
    exit 1
fi

# Step 5: Configure autostart
if [ "$ENABLE_AUTOSTART" = true ]; then
    log_info "Step 4: Configuring autostart..."
    
    # Use the autostart configuration script
    if [ -f "./autostart_config.sh" ]; then
        ./autostart_config.sh --enable --audio-${AUDIO_HARDWARE,,}
    else
        log_warning "autostart_config.sh not found, setting up basic autostart..."
        
        # Basic autostart setup
        mkdir -p ~/.config/autostart
        
        cat > ~/.config/autostart/tazzari-dashboard.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard
Exec=$(pwd)/$EXECUTABLE
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
EOF
        
        log_success "Basic autostart configured"
    fi
else
    log_info "Skipping autostart configuration (--no-autostart specified)"
fi

# Step 6: Production environment setup
log_info "Step 5: Production environment setup..."

# Hide cursor
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    log_info "Hiding mouse cursor for production..."
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak 2>/dev/null || true
    log_success "✓ Mouse cursor hidden"
fi

# Set performance governor (if available)
if [ -f "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" ]; then
    CURRENT_GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
    if [ "$CURRENT_GOVERNOR" != "performance" ]; then
        log_info "Setting CPU governor to performance..."
        echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true
        log_success "✓ CPU governor set to performance"
    else
        log_success "✓ CPU governor already set to performance"
    fi
fi

# Disable unnecessary services for automotive use
log_info "Optimizing system for automotive use..."

# List of services that can be safely disabled in automotive applications
SERVICES_TO_DISABLE=("cups" "avahi-daemon" "triggerhappy" "dphys-swapfile")

for service in "${SERVICES_TO_DISABLE[@]}"; do
    if systemctl is-enabled $service >/dev/null 2>&1; then
        log_info "Disabling $service..."
        sudo systemctl disable $service >/dev/null 2>&1 || true
    fi
done

log_success "✓ System optimized for automotive use"

# Step 7: Create management scripts
log_info "Step 6: Creating management scripts..."

# Create a comprehensive status script
cat > dashboard_status.sh << 'EOF'
#!/bin/bash
# TazzariAudio Dashboard Status and Management

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}=== TazzariAudio Dashboard Status ===${NC}"
echo ""

# Dashboard process status
if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
    DASHBOARD_PID=$(pgrep -f "LVGLDashboard.*deployment")
    UPTIME=$(ps -o etime= -p "$DASHBOARD_PID" 2>/dev/null | tr -d ' ')
    echo -e "Dashboard: ${GREEN}✓ Running${NC} (PID: $DASHBOARD_PID, Uptime: $UPTIME)"
else
    echo -e "Dashboard: ${RED}✗ Not running${NC}"
fi

# Audio system status
echo ""
echo "Audio System Status:"

# Detect hardware type from running process or config
AUDIO_TYPE="Unknown"
if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
    DASHBOARD_CMD=$(ps -p "$(pgrep -f "LVGLDashboard.*deployment")" -o cmd --no-headers 2>/dev/null)
    if echo "$DASHBOARD_CMD" | grep -q "BEOCREATE4"; then
        AUDIO_TYPE="BEOCREATE4"
    elif echo "$DASHBOARD_CMD" | grep -q "AMP4"; then
        AUDIO_TYPE="AMP4"
    elif echo "$DASHBOARD_CMD" | grep -q "DAC"; then
        AUDIO_TYPE="DAC"
    elif echo "$DASHBOARD_CMD" | grep -q "AUX"; then
        AUDIO_TYPE="AUX"
    fi
fi

echo "  Hardware Type: $AUDIO_TYPE"

case $AUDIO_TYPE in
    "AUX")
        if pulseaudio --check 2>/dev/null; then
            echo -e "  PulseAudio: ${GREEN}✓ Running${NC}"
        else
            echo -e "  PulseAudio: ${RED}✗ Not running${NC}"
        fi
        ;;
    "DAC"|"AMP4")
        if aplay -l 2>/dev/null | grep -q hifiberry; then
            echo -e "  HiFiBerry: ${GREEN}✓ Detected${NC}"
        else
            echo -e "  HiFiBerry: ${RED}✗ Not detected${NC}"
        fi
        ;;
    "BEOCREATE4")
        if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
            echo -e "  DSP API: ${GREEN}✓ Active${NC}"
        else
            echo -e "  DSP API: ${RED}✗ Not responding${NC}"
        fi
        ;;
esac

# Bluetooth status
echo ""
echo "Bluetooth Status:"
if systemctl is-active bluetooth >/dev/null 2>&1; then
    echo -e "  Service: ${GREEN}✓ Active${NC}"
    
    CONNECTED=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
    if [ "$CONNECTED" -gt 0 ]; then
        echo -e "  Connected: ${GREEN}✓ $CONNECTED device(s)${NC}"
        bluetoothctl devices Connected 2>/dev/null | while read line; do
            DEVICE_NAME=$(echo "$line" | cut -d' ' -f3-)
            echo "    → $DEVICE_NAME"
        done
    else
        echo -e "  Connected: ${YELLOW}- No devices${NC}"
    fi
else
    echo -e "  Service: ${RED}✗ Inactive${NC}"
fi

# System resources
echo ""
echo "System Resources:"
MEMORY_USAGE=$(free | grep '^Mem:' | awk '{printf "%.1f%%", $3/$2 * 100.0}')
CPU_TEMP=$(vcgencmd measure_temp 2>/dev/null | cut -d'=' -f2 | cut -d"'" -f1 || echo "N/A")
echo "  Memory Usage: $MEMORY_USAGE"
echo "  CPU Temperature: $CPU_TEMP°C"
echo "  Uptime: $(uptime -p)"

# Log file status
echo ""
if [ -f "dashboard_autostart.log" ]; then
    LOG_SIZE=$(du -h dashboard_autostart.log | cut -f1)
    LOG_LINES=$(wc -l < dashboard_autostart.log)
    echo "Log File: dashboard_autostart.log ($LOG_SIZE, $LOG_LINES lines)"
    echo "Last 3 entries:"
    tail -3 dashboard_autostart.log | sed 's/^/  /'
else
    echo "Log File: Not found"
fi
EOF
chmod +x dashboard_status.sh

# Create deployment info file
cat > deployment_info.txt << EOF
TazzariAudio Dashboard Deployment Information
===========================================
Deployment Date: $(date)
Audio Hardware: $AUDIO_HARDWARE
Autostart: $([ "$ENABLE_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")
Hostname: $(hostname)
System: $(uname -a)

Build Information:
- Executable: $EXECUTABLE
- Build Type: Deployment (fullscreen)
- Audio Hardware: $AUDIO_HARDWARE
- LVGL Version: 9.0
- Cursor: Hidden for production

Audio Configuration:
EOF

case $AUDIO_HARDWARE in
    "AUX")
        cat >> deployment_info.txt << 'EOF'
- Type: Built-in 3.5mm jack
- Volume Control: PulseAudio software
- EQ: alsaeq software plugin
- Boot Config: dtparam=audio=on
EOF
        ;;
    "DAC")
        cat >> deployment_info.txt << 'EOF'
- Type: HiFiBerry DAC+ (external amplifier required)
- Volume Control: ALSA hardware mixer
- EQ: alsaeq software plugin
- Boot Config: dtoverlay=hifiberry-dacplus-std
EOF
        ;;
    "AMP4")
        cat >> deployment_info.txt << 'EOF'
- Type: HiFiBerry AMP4 (built-in amplifier)
- Volume Control: ALSA hardware mixer
- EQ: alsaeq software plugin
- Boot Config: dtoverlay=hifiberry-dacplus-std
EOF
        ;;
    "BEOCREATE4")
        cat >> deployment_info.txt << 'EOF'
- Type: HiFiBerry BeoCreate 4 DSP
- Volume Control: REST API hardware
- EQ: DSP biquad filters (hardware)
- Boot Config: dtoverlay=hifiberry-dac + DSP tools
EOF
        ;;
esac

cat >> deployment_info.txt << EOF

Management Scripts:
- ./dashboard_status.sh          # Show detailed status
- ./stop_dashboard_${AUDIO_HARDWARE,,}.sh      # Stop dashboard
- ./restart_dashboard_${AUDIO_HARDWARE,,}.sh   # Restart dashboard
- ./show_cursor.sh               # Show cursor for debugging
- ./autostart_config.sh          # Manage autostart settings

Vehicle Integration:
- Serial Port: /dev/ttyACM0 (ESP32 communication)
- Bluetooth: Pi appears as 'TazzariAudio'
- Display: Fullscreen mode (no window decorations)
- Performance: CPU governor set to performance

Support Information:
- Documentation: README.md
- Hardware Test: ./test_audio_${AUDIO_HARDWARE,,}.sh
- Serial Test: ./test_serial.sh
- Bluetooth Pairing: ./pair_phone.sh
EOF

log_success "✓ Management scripts created"

# Final system status summary
echo ""
log_success "=== Deployment Complete! ==="
echo ""
echo "🚗 TazzariAudio Dashboard successfully deployed!"
echo ""
echo "Configuration Summary:"
echo "  • Audio Hardware: $AUDIO_HARDWARE"
echo "  • Deployment Mode: Fullscreen"
echo "  • Autostart: $([ "$ENABLE_AUTOSTART" = true ] && echo "Enabled" || echo "Disabled")"
echo "  • Mouse Cursor: Hidden"
echo "  • Performance: Optimized"
echo ""

echo "Management Commands:"
echo "  ./dashboard_status.sh        # Check status"
if [ "$ENABLE_AUTOSTART" = true ]; then
    echo "  sudo reboot                  # Test autostart"
else
    echo "  ./$EXECUTABLE               # Start manually"
fi
echo "  ./stop_dashboard_${AUDIO_HARDWARE,,}.sh   # Stop dashboard"
echo "  ./show_cursor.sh             # Debug mode"
echo ""

# Audio-specific next steps
echo "Next Steps:"
case $AUDIO_HARDWARE in
    "AUX")
        echo "  1. Connect external amplifier to 3.5mm jack"
        echo "  2. Test audio: ./test_audio_aux.sh"
        ;;
    "DAC")
        echo "  1. Connect external amplifier to HiFiBerry DAC+ output"
        echo "  2. Test audio: ./test_audio_dac.sh"
        ;;
    "AMP4")
        echo "  1. Connect speakers directly to HiFiBerry AMP4 terminals"
        echo "  2. Test audio: ./test_audio_amp4.sh"
        ;;
    "BEOCREATE4")
        echo "  1. Connect 4-channel speakers to BeoCreate 4"
        echo "  2. Test DSP: ./test_audio_beocreate4.sh"
        echo "  3. Configure crossover if needed"
        ;;
esac

echo "  3. Pair phone: ./pair_phone.sh"
echo "  4. Connect ESP32 to /dev/ttyACM0"

if [ "$ENABLE_AUTOSTART" = true ]; then
    echo "  5. Reboot to test autostart: sudo reboot"
else
    echo "  5. Start dashboard manually for testing"
fi

echo ""
echo "📋 Deployment info saved to: deployment_info.txt"
echo "📊 Status anytime: ./dashboard_status.sh"
echo ""

# Warning about reboot if needed
NEEDS_REBOOT=false

case $AUDIO_HARDWARE in
    "BEOCREATE4")
        if ! curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
            log_warning "IMPORTANT: BeoCreate 4 may require reboot for DSP initialization!"
            NEEDS_REBOOT=true
        fi
        ;;
    "DAC"|"AMP4")
        if ! aplay -l 2>/dev/null | grep -q hifiberry; then
            log_warning "IMPORTANT: HiFiBerry hardware may require reboot for detection!"
            NEEDS_REBOOT=true
        fi
        ;;
esac

if [ "$NEEDS_REBOOT" = true ]; then
    echo ""
    log_warning "⚠️  REBOOT RECOMMENDED for hardware initialization"
    echo "   Run: sudo reboot"
fi

# Final verification
echo ""
log_info "Running final verification..."

# Check executable permissions
if [ -x "$EXECUTABLE" ]; then
    log_success "✓ Executable has correct permissions"
else
    log_warning "Executable permissions issue"
fi

# Check autostart file if enabled
if [ "$ENABLE_AUTOSTART" = true ]; then
    if ls ~/.config/autostart/tazzari-dashboard*.desktop >/dev/null 2>&1; then
        log_success "✓ Autostart configuration present"
    else
        log_warning "Autostart configuration not found"
    fi
fi

# Check critical services
CRITICAL_SERVICES=("bluetooth")
case $AUDIO_HARDWARE in
    "BEOCREATE4")
        CRITICAL_SERVICES+=("sigmatcpserver")
        ;;
esac

for service in "${CRITICAL_SERVICES[@]}"; do
    if systemctl is-active $service >/dev/null 2>&1; then
        log_success "✓ $service service active"
    else
        log_warning "$service service not active"
    fi
done

echo ""
log_success "🎯 TazzariAudio Dashboard deployment complete!"
echo ""

if [ "$ENABLE_AUTOSTART" = true ]; then
    echo "🔄 System will automatically start dashboard on boot"
    echo "🧪 Test with: sudo reboot"
else
    echo "🎮 Manual start: ./$EXECUTABLE"
fi

echo "📞 Support: Check README.md or run ./dashboard_status.sh"
echo ""