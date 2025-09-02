#!/bin/bash
# setup_autostart.sh - Complete auto-start and cursor hiding setup

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[AUTOSTART]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

SERVICE_NAME="tazzari-dashboard"
DASHBOARD_PATH="$(pwd)"
USER="pi"

log_info "Setting up TazzariAudio Dashboard auto-start..."

# Check if we have the deployment executable
if [ ! -f "build/LVGLDashboard_deployment" ]; then
    echo "Error: Deployment executable not found!"
    echo "Run: ./build.sh --deployment"
    exit 1
fi

# 1. Install cursor hiding tools
log_info "Installing cursor hiding tools..."
sudo apt update -qq
sudo apt install -y unclutter

# 2. Create cursor hiding service
log_info "Creating cursor hiding service..."
sudo tee /etc/systemd/system/hide-cursor.service > /dev/null <<EOF
[Unit]
Description=Hide Mouse Cursor for TazzariAudio Dashboard
After=graphical-session.target
Requires=graphical-session.target

[Service]
Type=simple
User=$USER
Environment=DISPLAY=:0
Environment=XDG_RUNTIME_DIR=/run/user/1000
ExecStart=/usr/bin/unclutter -idle 0.1 -root -noevents
Restart=always
RestartSec=1

[Install]
WantedBy=graphical.target
EOF

# 3. Create X11 cursor configuration
log_info "Creating X11 cursor configuration..."
sudo mkdir -p /etc/X11/xorg.conf.d

sudo tee /etc/X11/xorg.conf.d/99-hide-cursor.conf > /dev/null <<'EOF'
# Hide mouse cursor for automotive dashboard
Section "InputClass"
    Identifier "Hide cursor"
    MatchIsPointer "on"
    MatchIsTouchpad "off"
    Option "Cursor" "none"
EndSection
EOF

# 4. Create dashboard auto-start service
log_info "Creating dashboard auto-start service..."
sudo tee /etc/systemd/system/${SERVICE_NAME}.service > /dev/null <<EOF
[Unit]
Description=TazzariAudio Automotive Dashboard
After=graphical-session.target bluetooth.service sigmatcpserver.service hide-cursor.service
Wants=bluetooth.service sigmatcpserver.service hide-cursor.service
Requires=graphical-session.target

[Service]
Type=simple
User=$USER
Group=$USER
WorkingDirectory=$DASHBOARD_PATH
Environment=HOME=/home/$USER
Environment=XDG_RUNTIME_DIR=/run/user/1000
Environment=DISPLAY=:0
Environment=WAYLAND_DISPLAY=wayland-0

# Start dashboard in deployment mode (fullscreen, no cursor)
ExecStart=$DASHBOARD_PATH/build/LVGLDashboard_deployment

# Auto-restart if crashes
Restart=always
RestartSec=5

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=tazzari-dashboard

[Install]
WantedBy=graphical.target
EOF

# 5. Enable services
log_info "Enabling services..."
sudo systemctl daemon-reload
sudo systemctl enable hide-cursor.service
sudo systemctl enable ${SERVICE_NAME}.service

# 6. Create manual control scripts
log_info "Creating control scripts..."

cat > start_dashboard.sh << 'EOF'
#!/bin/bash
echo "Starting TazzariAudio Dashboard service..."
sudo systemctl start tazzari-dashboard
sudo systemctl status tazzari-dashboard --no-pager
EOF
chmod +x start_dashboard.sh

cat > stop_dashboard.sh << 'EOF'
#!/bin/bash
echo "Stopping TazzariAudio Dashboard service..."
sudo systemctl stop tazzari-dashboard
sudo systemctl status tazzari-dashboard --no-pager
EOF
chmod +x stop_dashboard.sh

cat > dashboard_logs.sh << 'EOF'
#!/bin/bash
echo "=== TazzariAudio Dashboard Logs ==="
echo "Press Ctrl+C to exit log view"
echo ""
sudo journalctl -u tazzari-dashboard -f --no-pager
EOF
chmod +x dashboard_logs.sh

cat > disable_autostart.sh << 'EOF'
#!/bin/bash
echo "Disabling TazzariAudio Dashboard auto-start..."
sudo systemctl stop tazzari-dashboard
sudo systemctl disable tazzari-dashboard
sudo systemctl stop hide-cursor
sudo systemctl disable hide-cursor
echo "Auto-start disabled. Use ./start_dashboard.sh to start manually."
EOF
chmod +x disable_autostart.sh

log_success "Auto-start setup complete!"
log_info ""
log_info "Configuration:"
log_info "  ✓ Dashboard auto-starts on boot (fullscreen)"
log_info "  ✓ Mouse cursor hidden multiple ways"
log_info "  ✓ Auto-restart if dashboard crashes"
log_info "  ✓ Waits for Bluetooth and DSP services"
log_info ""
log_info "Control scripts created:"
log_info "  ./start_dashboard.sh     # Start dashboard service"
log_info "  ./stop_dashboard.sh      # Stop dashboard service"  
log_info "  ./dashboard_logs.sh      # View live logs"
log_info "  ./disable_autostart.sh   # Disable auto-start"
log_info ""
log_info "Service commands:"
log_info "  sudo systemctl status tazzari-dashboard"
log_info "  sudo journalctl -u tazzari-dashboard"
log_info ""
log_warning "REBOOT REQUIRED for full auto-start and cursor hiding!"
log_info ""
log_info "After reboot, the dashboard will:"
log_info "  1. Wait for Bluetooth and DSP to be ready"
log_info "  2. Start automatically in fullscreen"
log_info "  3. Hide the mouse cursor"
log_info "  4. Restart if it crashes"