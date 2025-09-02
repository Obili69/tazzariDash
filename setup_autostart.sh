#!/bin/bash
# setup_dashboard_autostart.sh - Simple auto-start with your splash image

set -e

DASHBOARD_PATH="$(pwd)"

echo "Setting up TazzariAudio Dashboard auto-start..."

# 1. Build deployment version
if [ ! -f "build/LVGLDashboard_deployment" ]; then
    echo "Building deployment version..."
    ./build.sh --deployment
fi

# 2. Hide cursor using your elegant method
echo "Hiding mouse cursor..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak
    echo "✓ Cursor hidden (renamed to .bak)"
fi

# 3. Create startup script
echo "Creating startup script..."

cat > ~/start_tazzari_dashboard.sh << EOF
#!/bin/bash
# TazzariAudio Dashboard auto-start

sleep 15  # Wait for desktop

cd $DASHBOARD_PATH

# Wait for DSP with timeout
echo "Starting TazzariAudio..."
for i in {1..20}; do
    if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
        break
    fi
    sleep 3
done

# Start dashboard
while true; do
    ./build/LVGLDashboard_deployment
    sleep 3
done
EOF

chmod +x ~/start_tazzari_dashboard.sh

# 4. Desktop autostart
mkdir -p ~/.config/autostart

cat > ~/.config/autostart/tazzari-dashboard.desktop << EOF
[Desktop Entry]
Type=Application
Name=TazzariAudio Dashboard
Exec=/home/pi/start_tazzari_dashboard.sh
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
EOF

# 5. Control scripts
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
    echo "Cursor restored - restart desktop to see"
fi
EOF
chmod +x show_cursor.sh

cat > disable_autostart.sh << 'EOF'
#!/bin/bash
rm -f ~/.config/autostart/tazzari-dashboard.desktop
./stop_dashboard.sh
echo "Auto-start disabled"
EOF
chmod +x disable_autostart.sh

echo ""
echo "✓ Auto-start configured"
echo "✓ Cursor hidden using rename method"
echo ""
echo "Controls:"
echo "  ./stop_dashboard.sh      # Stop dashboard"
echo "  ./show_cursor.sh         # Show cursor for debugging"  
echo "  ./disable_autostart.sh   # Disable auto-start"
echo ""
echo "Test: sudo reboot"