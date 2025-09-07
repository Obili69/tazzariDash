#!/bin/bash
# autostart_config.sh - Configure TazzariAudio Dashboard autostart for specific audio hardware

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[AUTOSTART]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

AUDIO_HARDWARE=""
ACTION=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --enable)
            ACTION="enable"
            shift
            ;;
        --disable)
            ACTION="disable"
            shift
            ;;
        --status)
            ACTION="status"
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
            echo "TazzariAudio Dashboard Autostart Configuration"
            echo "Usage: $0 [action] [audio-hardware]"
            echo ""
            echo "Actions:"
            echo "  --enable      Enable autostart"
            echo "  --disable     Disable autostart"
            echo "  --status      Show autostart status"
            echo ""
            echo "Audio Hardware:"
            echo "  --audio-aux         Built-in 3.5mm jack"
            echo "  --audio-dac         HiFiBerry DAC+"
            echo "  --audio-amp4        HiFiBerry AMP4"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4"
            echo ""
            echo "Examples:"
            echo "  $0 --enable --audio-amp4     # Enable autostart for AMP4"
            echo "  $0 --disable                 # Disable all autostart"
            echo "  $0 --status                  # Show current status"
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

# Detect audio hardware if not specified
if [ -z "$AUDIO_HARDWARE" ] && [ "$ACTION" != "disable" ] && [ "$ACTION" != "status" ]; then
    log_info "Detecting audio hardware configuration..."
    
    # Check for existing executables
    if [ -f "build/LVGLDashboard_BEOCREATE4_deployment" ]; then
        AUDIO_HARDWARE="BEOCREATE4"
    elif [ -f "build/LVGLDashboard_AMP4_deployment" ]; then
        AUDIO_HARDWARE="AMP4"
    elif [ -f "build/LVGLDashboard_DAC_deployment" ]; then
        AUDIO_HARDWARE="DAC"
    elif [ -f "build/LVGLDashboard_AUX_deployment" ]; then
        AUDIO_HARDWARE="AUX"
    else
        log_error "No deployment executable found. Build with --deployment first."
        echo "Run: ./build.sh --deployment --audio-[hardware]"
        exit 1
    fi
    
    log_info "Detected audio hardware: $AUDIO_HARDWARE"
fi

# Set default action
if [ -z "$ACTION" ]; then
    ACTION="enable"
fi

# Function to show status
show_status() {
    log_info "TazzariAudio Dashboard Autostart Status"
    echo ""
    
    # Check for autostart files
    AUTOSTART_FILES=(~/.config/autostart/tazzari-dashboard-*.desktop)
    STARTUP_SCRIPTS=(~/start_tazzari_dashboard_*.sh)
    
    if ls ~/.config/autostart/tazzari-dashboard-*.desktop >/dev/null 2>&1; then
        echo "Active autostart configurations:"
        for file in ~/.config/autostart/tazzari-dashboard-*.desktop; do
            if [ -f "$file" ]; then
                HARDWARE=$(basename "$file" .desktop | sed 's/tazzari-dashboard-//' | tr '[:lower:]' '[:upper:]')
                echo "  ✓ $HARDWARE ($(basename "$file"))"
            fi
        done
    else
        echo "No autostart configurations found"
    fi
    
    echo ""
    
    # Check cursor status
    if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
        echo "Mouse cursor: Hidden (deployment mode)"
    else
        echo "Mouse cursor: Visible (development mode)"
    fi
    
    echo ""
    
    # Check running processes
    if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
        echo "Dashboard process: Running"
        RUNNING_PROCESS=$(pgrep -f "LVGLDashboard.*deployment" | head -1)
        RUNNING_CMD=$(ps -p "$RUNNING_PROCESS" -o cmd --no-headers 2>/dev/null | sed 's/.*LVGLDashboard_//' | sed 's/_deployment.*//')
        echo "  Hardware: $RUNNING_CMD"
    else
        echo "Dashboard process: Not running"
    fi
    
    echo ""
    
    # Check available executables
    echo "Available deployment builds:"
    for hw in AUX DAC AMP4 BEOCREATE4; do
        EXEC_PATH="build/LVGLDashboard_${hw}_deployment"
        if [ -f "$EXEC_PATH" ]; then
            echo "  ✓ $hw ($EXEC_PATH)"
        else
            echo "  - $hw (not built)"
        fi
    done
}

# Function to enable autostart
enable_autostart() {
    local hardware=$1
    
    log_info "Enabling autostart for $hardware audio hardware..."
    
    # Check if deployment executable exists
    EXECUTABLE="build/LVGLDashboard_${hardware}_deployment"
    if [ ! -f "$EXECUTABLE" ]; then
        log_error "Deployment executable not found: $EXECUTABLE"
        echo "Build deployment version first:"
        echo "  ./build.sh --deployment --audio-${hardware,,}"
        exit 1
    fi
    
    # Create autostart directory
    mkdir -p ~/.config/autostart
    
    # Get absolute paths
    DASHBOARD_PATH="$(pwd)"
    EXECUTABLE_FULL_PATH="$DASHBOARD_PATH/$EXECUTABLE"
    HARDWARE_LOWER=$(echo "$hardware" | tr '[:upper:]' '[:lower:]')
    
    # Remove any existing autostart configurations
    rm -f ~/.config/autostart/tazzari-dashboard-*.desktop 2>/dev/null || true
    rm -f ~/start_tazzari_dashboard_*.sh 2>/dev/null || true
    
    # Create startup script
    log_info "Creating startup script for $hardware..."
    cat > ~/start_tazzari_dashboard_${HARDWARE_LOWER}.sh << EOF
#!/bin/bash
# TazzariAudio Dashboard auto-start for $hardware
# Generated on $(date)

echo "=== TazzariAudio Dashboard Auto-Start ==="
echo "Audio Hardware: $hardware"
echo "Build: Deployment Mode"
echo "Starting at: \$(date)"

# Wait for desktop environment and system services
sleep 15

cd "$DASHBOARD_PATH"

# Hardware-specific startup sequence
case "$hardware" in
    "AUX")
        echo "Initializing built-in audio system..."
        # Ensure PulseAudio is running
        if ! pulseaudio --check 2>/dev/null; then
            echo "Starting PulseAudio..."
            pulseaudio --start
            sleep 3
        fi
        
        # Test audio output
        if command -v pactl >/dev/null; then
            pactl list sinks short | head -1
            echo "✓ PulseAudio ready"
        fi
        ;;
        
    "DAC"|"AMP4")
        echo "Initializing HiFiBerry $hardware system..."
        # Wait for HiFiBerry device to be detected
        for i in {1..10}; do
            if aplay -l 2>/dev/null | grep -q hifiberry; then
                echo "✓ HiFiBerry device detected"
                # Test hardware volume control
                if command -v amixer >/dev/null; then
                    amixer get Digital >/dev/null 2>&1 && echo "✓ Hardware volume control ready"
                fi
                break
            fi
            echo "Waiting for HiFiBerry device... (\$i/10)"
            sleep 2
        done
        ;;
        
    "BEOCREATE4")
        echo "Initializing HiFiBerry BeoCreate 4 DSP..."
        # Wait for SigmaTCP server and DSP
        for i in {1..20}; do
            if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
                echo "✓ DSP REST API ready"
                
                # Check for loaded profile
                DSP_RESPONSE=\$(curl -s http://localhost:13141/metadata 2>/dev/null)
                if echo "\$DSP_RESPONSE" | grep -q "profileName"; then
                    PROFILE_NAME=\$(echo "\$DSP_RESPONSE" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
                    echo "✓ DSP Profile loaded: \$PROFILE_NAME"
                else
                    echo "⚠ No DSP profile loaded - will use default settings"
                fi
                break
            fi
            echo "Waiting for BeoCreate 4 DSP... (\$i/20)"
            sleep 3
        done
        ;;
esac

# Start Bluetooth A2DP if not running
if ! systemctl --user is-active pulseaudio-module-bluetooth >/dev/null 2>&1; then
    echo "Starting Bluetooth audio support..."
    systemctl --user restart pulseaudio 2>/dev/null || true
fi

# Final system check
echo ""
echo "System Status Check:"
echo "  Audio Hardware: $hardware"
echo "  Dashboard Path: $DASHBOARD_PATH"
echo "  Executable: $EXECUTABLE_FULL_PATH"
echo "  Bluetooth: \$(systemctl is-active bluetooth 2>/dev/null || echo 'inactive')"
echo "  A2DP Agent: \$(systemctl is-active a2dp-agent 2>/dev/null || echo 'inactive')"
echo ""

# Start dashboard with restart capability
echo "Starting TazzariAudio Dashboard..."
LOG_FILE="$DASHBOARD_PATH/dashboard_autostart.log"

# Function to log with timestamp
log_with_time() {
    echo "\$(date '+%Y-%m-%d %H:%M:%S'): \$1" | tee -a "\$LOG_FILE"
}

log_with_time "=== Dashboard startup sequence initiated ==="
log_with_time "Audio Hardware: $hardware"

# Main dashboard loop with restart capability
RESTART_COUNT=0
MAX_RESTARTS=10

while [ \$RESTART_COUNT -lt \$MAX_RESTARTS ]; do
    log_with_time "Starting dashboard (attempt \$((RESTART_COUNT + 1)))"
    
    # Start the dashboard
    $EXECUTABLE_FULL_PATH 2>&1 | while read line; do
        log_with_time "DASH: \$line"
    done
    
    EXIT_CODE=\$?
    RESTART_COUNT=\$((RESTART_COUNT + 1))
    
    log_with_time "Dashboard exited with code \$EXIT_CODE"
    
    # If exit code is 0, it was a normal shutdown - don't restart
    if [ \$EXIT_CODE -eq 0 ]; then
        log_with_time "Normal shutdown detected, not restarting"
        break
    fi
    
    # If we've hit max restarts, wait longer
    if [ \$RESTART_COUNT -ge \$MAX_RESTARTS ]; then
        log_with_time "Maximum restart attempts reached, waiting 30 seconds before trying again"
        sleep 30
        RESTART_COUNT=0
    else
        log_with_time "Restarting in 5 seconds..."
        sleep 5
    fi
done

log_with_time "=== Dashboard autostart terminated ==="
EOF
    chmod +x ~/start_tazzari_dashboard_${HARDWARE_LOWER}.sh
    
    # Create desktop autostart entry
    log_info "Creating desktop autostart entry..."
    cat > ~/.config/autostart/tazzari-dashboard-${HARDWARE_LOWER}.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard ($hardware)
Exec=/home/$(whoami)/start_tazzari_dashboard_${HARDWARE_LOWER}.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Comment=TazzariAudio Automotive Dashboard with $hardware audio hardware
Icon=applications-multimedia
StartupNotify=false
StartupWMClass=LVGLDashboard
Categories=AudioVideo;Audio;
EOF
    
    # Hide cursor for deployment builds
    log_info "Configuring deployment environment..."
    if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
        log_info "Hiding mouse cursor..."
        sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak 2>/dev/null || true
        log_success "Mouse cursor hidden"
    fi
    
    # Create control scripts
    log_info "Creating control scripts..."
    
    # Stop script
    cat > stop_dashboard_${HARDWARE_LOWER}.sh << EOF
#!/bin/bash
# Stop TazzariAudio Dashboard for $hardware

echo "Stopping TazzariAudio Dashboard ($hardware)..."

# Kill dashboard processes
pkill -f "LVGLDashboard_${hardware}_deployment" && echo "✓ Dashboard process stopped"
pkill -f "start_tazzari_dashboard_${HARDWARE_LOWER}" && echo "✓ Startup script stopped"

# Show status
sleep 2
if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
    echo "⚠ Dashboard still running"
    exit 1
else
    echo "✓ Dashboard completely stopped"
    exit 0
fi
EOF
    chmod +x stop_dashboard_${HARDWARE_LOWER}.sh
    
    # Restart script
    cat > restart_dashboard_${HARDWARE_LOWER}.sh << EOF
#!/bin/bash
# Restart TazzariAudio Dashboard for $hardware

echo "Restarting TazzariAudio Dashboard ($hardware)..."

# Stop first
./stop_dashboard_${HARDWARE_LOWER}.sh

# Wait a moment
sleep 3

# Start again
echo "Starting dashboard..."
~/start_tazzari_dashboard_${HARDWARE_LOWER}.sh &

echo "✓ Dashboard restarted in background"
echo "Check status with: ./status_dashboard.sh"
EOF
    chmod +x restart_dashboard_${HARDWARE_LOWER}.sh
    
    # Status script
    cat > status_dashboard.sh << 'EOF'
#!/bin/bash
# Show TazzariAudio Dashboard status

echo "=== TazzariAudio Dashboard Status ==="
echo ""

# Check running processes
if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
    DASHBOARD_PID=$(pgrep -f "LVGLDashboard.*deployment")
    DASHBOARD_CMD=$(ps -p "$DASHBOARD_PID" -o cmd --no-headers 2>/dev/null)
    echo "Dashboard: ✓ Running (PID: $DASHBOARD_PID)"
    echo "  Command: $DASHBOARD_CMD"
    
    # Show resource usage
    if command -v ps >/dev/null; then
        PS_OUTPUT=$(ps -p "$DASHBOARD_PID" -o pid,pcpu,pmem,time --no-headers 2>/dev/null)
        echo "  Resources: $PS_OUTPUT"
    fi
else
    echo "Dashboard: ✗ Not running"
fi

echo ""

# Check startup script
if pgrep -f "start_tazzari_dashboard" >/dev/null; then
    STARTUP_PID=$(pgrep -f "start_tazzari_dashboard")
    echo "Startup Script: ✓ Running (PID: $STARTUP_PID)"
else
    echo "Startup Script: ✗ Not running"
fi

echo ""

# Check autostart configuration
if ls ~/.config/autostart/tazzari-dashboard-*.desktop >/dev/null 2>&1; then
    echo "Autostart: ✓ Enabled"
    for file in ~/.config/autostart/tazzari-dashboard-*.desktop; do
        if [ -f "$file" ]; then
            HARDWARE=$(basename "$file" .desktop | sed 's/tazzari-dashboard-//' | tr '[:lower:]' '[:upper:]')
            echo "  Configuration: $HARDWARE ($(basename "$file"))"
        fi
    done
else
    echo "Autostart: ✗ Disabled"
fi

echo ""

# Check recent logs
LOG_FILE="dashboard_autostart.log"
if [ -f "$LOG_FILE" ]; then
    echo "Recent Log Entries (last 5 lines):"
    tail -5 "$LOG_FILE" | sed 's/^/  /'
else
    echo "Log File: Not found ($LOG_FILE)"
fi

echo ""

# Show system info
echo "System Information:"
echo "  Uptime: $(uptime -p 2>/dev/null || uptime)"
echo "  Audio Hardware Detection:"

# Check audio systems
if command -v pactl >/dev/null && pulseaudio --check 2>/dev/null; then
    echo "    PulseAudio: ✓ Running"
elif command -v amixer >/dev/null && amixer get Master >/dev/null 2>&1; then
    echo "    ALSA: ✓ Available"
fi

if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
    echo "    BeoCreate 4 DSP: ✓ Active"
fi

# Bluetooth status
if systemctl is-active bluetooth >/dev/null 2>&1; then
    echo "    Bluetooth: ✓ Active"
    CONNECTED=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
    if [ "$CONNECTED" -gt 0 ]; then
        echo "      Connected devices: $CONNECTED"
    fi
fi
EOF
    chmod +x status_dashboard.sh
    
    # Show cursor script (for debugging)
    cat > show_cursor.sh << 'EOF'
#!/bin/bash
echo "Restoring mouse cursor for debugging..."

if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr 2>/dev/null
    echo "✓ Mouse cursor restored"
    echo "Note: Restart desktop environment to see effect"
    echo "      Or reboot: sudo reboot"
else
    echo "- Cursor backup not found (cursor may already be visible)"
fi
EOF
    chmod +x show_cursor.sh
    
    log_success "Autostart enabled for $hardware!"
    echo ""
    echo "Created files:"
    echo "  ~/start_tazzari_dashboard_${HARDWARE_LOWER}.sh   # Main startup script"
    echo "  ~/.config/autostart/tazzari-dashboard-${HARDWARE_LOWER}.desktop  # Desktop autostart"
    echo "  ./stop_dashboard_${HARDWARE_LOWER}.sh      # Stop dashboard"
    echo "  ./restart_dashboard_${HARDWARE_LOWER}.sh   # Restart dashboard"
    echo "  ./status_dashboard.sh         # Show status"
    echo "  ./show_cursor.sh              # Show cursor for debugging"
    echo ""
    echo "Dashboard will automatically start on boot."
    echo "Test now: sudo reboot"
}

# Function to disable autostart
disable_autostart() {
    log_info "Disabling TazzariAudio Dashboard autostart..."
    
    # Stop any running dashboards
    if pgrep -f "LVGLDashboard.*deployment" >/dev/null; then
        log_info "Stopping running dashboard..."
        pkill -f "LVGLDashboard.*deployment"
        pkill -f "start_tazzari_dashboard"
        sleep 2
    fi
    
    # Remove autostart files
    REMOVED_COUNT=0
    
    if ls ~/.config/autostart/tazzari-dashboard-*.desktop >/dev/null 2>&1; then
        for file in ~/.config/autostart/tazzari-dashboard-*.desktop; do
            if [ -f "$file" ]; then
                rm "$file"
                REMOVED_COUNT=$((REMOVED_COUNT + 1))
                log_info "Removed: $(basename "$file")"
            fi
        done
    fi
    
    # Remove startup scripts
    if ls ~/start_tazzari_dashboard_*.sh >/dev/null 2>&1; then
        for file in ~/start_tazzari_dashboard_*.sh; do
            if [ -f "$file" ]; then
                rm "$file"
                REMOVED_COUNT=$((REMOVED_COUNT + 1))
                log_info "Removed: $(basename "$file")"
            fi
        done
    fi
    
    # Restore cursor
    if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
        sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr 2>/dev/null || true
        log_info "Mouse cursor restored"
    fi
    
    if [ $REMOVED_COUNT -gt 0 ]; then
        log_success "Autostart disabled ($REMOVED_COUNT files removed)"
    else
        log_warning "No autostart configuration found"
    fi
    
    echo ""
    echo "Dashboard autostart is now disabled."
    echo "To manually start: ./run_*_deployment.sh"
}

# Main execution
case $ACTION in
    "enable")
        if [ -z "$AUDIO_HARDWARE" ]; then
            log_error "Audio hardware must be specified for enable action"
            echo "Use: $0 --enable --audio-[hardware]"
            exit 1
        fi
        enable_autostart "$AUDIO_HARDWARE"
        ;;
    "disable")
        disable_autostart
        ;;
    "status")
        show_status
        ;;
    *)
        log_error "Unknown action: $ACTION"
        echo "Use --help for usage information"
        exit 1
        ;;
esac