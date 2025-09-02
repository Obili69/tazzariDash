#!/bin/bash
# setup_safe_autostart.sh - Safe auto-start that won't brick the system

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[AUTOSTART]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

DASHBOARD_PATH="$(pwd)"

log_info "Setting up safe auto-start for TazzariAudio Dashboard..."

# 1. Build deployment version first
if [ ! -f "build/LVGLDashboard_deployment" ]; then
    log_info "Building deployment version..."
    ./build.sh --deployment
fi

# 2. Install cursor hiding tools
log_info "Installing cursor hiding tools..."
sudo apt install -y unclutter xinput

# 3. Create startup script (NOT a systemd service)
log_info "Creating startup script..."

cat > ~/start_tazzari_dashboard.sh << EOF
#!/bin/bash
# TazzariAudio Dashboard startup script

# Wait for system to be fully ready
sleep 10

# Hide cursor using multiple methods
export SDL_VIDEO_CURSOR=0
export SDL_MOUSE_CURSOR=0

# Start unclutter to hide cursor
unclutter -idle 0.1 -root -noevents &

# Disable mouse input entirely for automotive use
xinput --list | grep -i mouse | while read line; do
    mouse_id=\$(echo "\$line" | grep -o 'id=[0-9]*' | cut -d= -f2)
    if [ ! -z "\$mouse_id" ]; then
        xinput disable \$mouse_id 2>/dev/null || true
    fi
done

# Change to dashboard directory
cd $DASHBOARD_PATH

# Wait for audio services to be ready
while ! curl -s http://localhost:13141/checksum >/dev/null 2>&1; do
    echo "Waiting for DSP to be ready..."
    sleep 2
done

echo "Starting TazzariAudio Dashboard..."

# Start dashboard with error recovery
while true; do
    ./build/LVGLDashboard_deployment
    echo "Dashboard exited, restarting in 3 seconds..."
    sleep 3
done
EOF

chmod +x ~/start_tazzari_dashboard.sh

# 4. Add to user's desktop autostart (safer than systemd)
log_info "Adding to desktop autostart..."

mkdir -p ~/.config/autostart

cat > ~/.config/autostart/tazzari-dashboard.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard
Comment=Automotive Dashboard with BeoCreate 4 DSP
Exec=/home/pi/start_tazzari_dashboard.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
StartupNotify=false
EOF

# 5. Create control scripts
log_info "Creating control scripts..."

cat > kill_dashboard.sh << 'EOF'
#!/bin/bash
echo "Stopping TazzariAudio Dashboard..."
pkill -f LVGLDashboard_deployment
pkill -f start_tazzari_dashboard.sh
echo "Dashboard stopped."
EOF
chmod +x kill_dashboard.sh

cat > start_dashboard_manual.sh << 'EOF'
#!/bin/bash
echo "Starting TazzariAudio Dashboard manually..."
~/start_tazzari_dashboard.sh &
echo "Dashboard started in background."
EOF
chmod +x start_dashboard_manual.sh

cat > disable_autostart.sh << 'EOF'
#!/bin/bash
echo "Disabling auto-start..."
rm -f ~/.config/autostart/tazzari-dashboard.desktop
./kill_dashboard.sh
echo "Auto-start disabled. Use ./start_dashboard_manual.sh to start manually."
EOF
chmod +x disable_autostart.sh

cat > enable_mouse.sh << 'EOF'
#!/bin/bash
echo "Re-enabling mouse input..."
xinput --list | grep -i mouse | while read line; do
    mouse_id=$(echo "$line" | grep -o 'id=[0-9]*' | cut -d= -f2)
    if [ ! -z "$mouse_id" ]; then
        xinput enable $mouse_id 2>/dev/null || true
        echo "Enabled mouse ID: $mouse_id"
    fi
done
pkill unclutter 2>/dev/null || true
echo "Mouse input restored."
EOF
chmod +x enable_mouse.sh

log_success "Safe auto-start setup complete!"
log_info ""
log_info "Configuration:"
log_info "  ✓ Desktop autostart (not systemd service)"
log_info "  ✓ Mouse cursor hidden and input disabled"
log_info "  ✓ Waits for DSP to be ready before starting"
log_info "  ✓ Auto-restart if dashboard crashes"
log_info "  ✓ Easy to disable if issues occur"
log_info ""
log_info "Control scripts:"
log_info "  ./kill_dashboard.sh         # Stop dashboard immediately"
log_info "  ./start_dashboard_manual.sh # Start dashboard manually"
log_info "  ./disable_autostart.sh      # Disable auto-start"
log_info "  ./enable_mouse.sh           # Re-enable mouse for debugging"
log_info ""
log_info "Safety features:"
log_info "  • SSH always works (mouse disable doesn't affect SSH)"
log_info "  • Can disable autostart easily"
log_info "  • Logs to terminal, not hidden in systemd"
log_info ""
log_warning "Test first: ./start_dashboard_manual.sh"
log_info "If it works well, reboot to test auto-start"
log_info ""
log_info "Emergency recovery:"
log_info "  SSH in and run: ./disable_autostart.sh"