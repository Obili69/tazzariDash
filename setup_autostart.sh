#!/bin/bash
# setup_final_autostart.sh - Simple autostart with cursor rename method

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[AUTOSTART]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }

DASHBOARD_PATH="$(pwd)"

log_info "Setting up TazzariAudio Dashboard auto-start..."

# 1. Build deployment version
if [ ! -f "build/LVGLDashboard_deployment" ]; then
    log_info "Building deployment version..."
    ./build.sh --deployment
fi

# 2. Hide cursor using simple rename method
log_info "Hiding mouse cursor..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak
    log_success "Cursor hidden (renamed to .bak)"
else
    log_info "Cursor already hidden"
fi

# 3. Create startup script
log_info "Creating startup script..."

cat > ~/start_tazzari_dashboard.sh << EOF
#!/bin/bash
# TazzariAudio Dashboard auto-start

# Wait for desktop to be ready
sleep 15

cd $DASHBOARD_PATH

# Wait for DSP (with timeout)
echo "Waiting for TazzariAudio DSP..."
for i in {1..30}; do
    if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
        echo "DSP ready!"
        break
    fi
    echo "Waiting... (\$i/30)"
    sleep 2
done

# Start dashboard with auto-restart
echo "Starting TazzariAudio Dashboard..."
while true; do
    ./build/LVGLDashboard_deployment 2>/dev/null || true
    echo "Dashboard restarting..."
    sleep 3
done
EOF

chmod +x ~/start_tazzari_dashboard.sh

# 4. Add to desktop autostart
mkdir -p ~/.config/autostart

cat > ~/.config/autostart/tazzari-dashboard.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard
Exec=/home/pi/start_tazzari_dashboard.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
StartupNotify=false
EOF

# 5. Create control scripts
cat > start_dashboard.sh << 'EOF'
#!/bin/bash
~/start_tazzari_dashboard.sh &
echo "Dashboard started manually"
EOF
chmod +x start_dashboard.sh

cat > stop_dashboard.sh << 'EOF'
#!/bin/bash
pkill -f LVGLDashboard_deployment
pkill -f start_tazzari_dashboard.sh
echo "Dashboard stopped"
EOF
chmod +x stop_dashboard.sh

cat > show_cursor.sh << 'EOF'
#!/bin/bash
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr.bak /usr/share/icons/PiXflat/cursors/left_ptr
    echo "Cursor restored - restart desktop to see changes"
else
    echo "Cursor backup not found"
fi
EOF
chmod +x show_cursor.sh

cat > hide_cursor.sh << 'EOF'
#!/bin/bash
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak
    echo "Cursor hidden - restart desktop to see changes"
else
    echo "Cursor already hidden"
fi
EOF
chmod +x hide_cursor.sh

cat > disable_autostart.sh << 'EOF'
#!/bin/bash
rm -f ~/.config/autostart/tazzari-dashboard.desktop
./stop_dashboard.sh
echo "Auto-start disabled"
EOF
chmod +x disable_autostart.sh

log_success "Auto-start setup complete!"
echo ""
echo "Controls:"
echo "  ./start_dashboard.sh     # Start manually"
echo "  ./stop_dashboard.sh      # Stop dashboard"
echo "  ./show_cursor.sh         # Show cursor (for debugging)"
echo "  ./hide_cursor.sh         # Hide cursor again"
echo "  ./disable_autostart.sh   # Disable auto-start"
echo ""
echo "Test: ./start_dashboard.sh"
echo "Then: sudo reboot (to test auto-start)"