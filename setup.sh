#!/bin/bash
# setup.sh - Complete LVGL Dashboard setup (dependencies + Bluetooth audio)

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[SETUP]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

if [[ $EUID -eq 0 ]]; then
   echo "Error: Don't run as root. Use regular user with sudo access."
   exit 1
fi

log_info "=== Complete LVGL Dashboard Setup ==="
log_info "This installs everything you need:"
log_info "  • System dependencies (build tools, libraries)"
log_info "  • Bluetooth A2DP audio sink (appear as TazzariAudio speaker)"
log_info "  • Serial communication setup"
log_info "  • LVGL library"

read -p "Install everything? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Setup cancelled"
    exit 0
fi

# Update system
log_info "Updating package lists..."
sudo apt update

# Install build tools
log_info "Installing build tools..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config

# Install LVGL dependencies  
log_info "Installing LVGL dependencies..."
sudo apt install -y \
    libsdl2-dev \
    libsdl2-image-dev

# Install audio system
log_info "Installing audio system..."
sudo apt install -y \
    libasound2-dev \
    libpulse-dev \
    pulseaudio \
    pulseaudio-utils \
    alsa-utils

# Install Bluetooth A2DP audio sink
log_info "Installing Bluetooth A2DP audio support..."
sudo apt install -y \
    bluetooth \
    bluez \
    bluez-tools \
    bluez-firmware \
    libbluetooth-dev \
    pulseaudio \
    pulseaudio-module-bluetooth \
    libdbus-1-dev \
    dbus-user-session \
    expect

# Configure Bluetooth for A2DP audio sink
log_info "Configuring Bluetooth as TazzariAudio A2DP sink..."

sudo tee /etc/bluetooth/main.conf > /dev/null << 'EOF'
[General]
Name = TazzariAudio
# Audio device class (renders as headphones/speaker)
Class = 0x240404
DiscoverableTimeout = 0
PairableTimeout = 0
Discoverable = yes
Pairable = yes
AutoEnable = yes

[Policy]
AutoEnable = yes
# Re-connect A2DP automatically
ReconnectUUIDs = 0000110a-0000-1000-8000-00805f9b34fb,0000110b-0000-1000-8000-00805f9b34fb,0000110c-0000-1000-8000-00805f9b34fb,0000110d-0000-1000-8000-00805f9b34fb
ReconnectAttempts = 7
ReconnectIntervals = 1,2,4,8,16,32,64
EOF

# Configure PulseAudio for A2DP sink
log_info "Configuring PulseAudio to receive Bluetooth audio..."

mkdir -p ~/.config/pulse

cat > ~/.config/pulse/default.pa << 'EOF'
#!/usr/bin/pulseaudio -nF

# Load standard configuration
.include /etc/pulse/default.pa

# Load Bluetooth A2DP sink module (receive audio FROM phone)
load-module module-bluetooth-policy
load-module module-bluetooth-discover

# CRITICAL: Load loopback to route Bluetooth audio to analog jack
# This pipes incoming Bluetooth audio directly to your car speakers
load-module module-loopback latency_msec=50

# Set analog jack as default output (never route to Bluetooth speakers)
set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo
EOF

# Create A2DP service
log_info "Creating A2DP audio sink service..."

sudo tee /usr/bin/bluetooth-a2dp-sink << 'EOF'
#!/bin/bash
# A2DP sink service

# Configure Bluetooth controller  
hciconfig hci0 up
hciconfig hci0 piscan
hciconfig hci0 sspmode 1

# Start bluetoothctl agent for auto-pairing
bluetoothctl << BTCTL_EOF
power on
discoverable on
pairable on
agent NoInputNoOutput
default-agent
BTCTL_EOF

# Keep service running
while true; do
    # Check if we need to restart discoverable mode
    if ! bluetoothctl show | grep -q "Discoverable: yes"; then
        bluetoothctl discoverable on
    fi
    sleep 30
done
EOF

sudo chmod +x /usr/bin/bluetooth-a2dp-sink

sudo tee /etc/systemd/system/bluetooth-a2dp.service > /dev/null << 'EOF'
[Unit]
Description=Bluetooth A2DP Audio Sink
After=bluetooth.service pulseaudio.service
Requires=bluetooth.service

[Service]
Type=simple
User=root
ExecStart=/usr/bin/bluetooth-a2dp-sink
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Enable Bluetooth services
sudo systemctl enable bluetooth
sudo systemctl enable bluetooth-a2dp

# Install serial tools
log_info "Installing serial communication tools..."
sudo apt install -y \
    screen \
    minicom \
    picocom

# Add user to dialout group
log_info "Adding user to dialout group..."
sudo usermod -a -G dialout $USER

# Create udev rules for serial devices
log_info "Creating udev rules for USB serial devices..."
sudo tee /etc/udev/rules.d/99-usb-serial.rules > /dev/null << 'EOF'
# ESP32 and Arduino USB serial devices
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="dialout" 
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="239a", MODE="0666", GROUP="dialout"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

# Setup LVGL if not present
if [ ! -d "lvgl" ]; then
    log_info "Setting up LVGL library..."
    git clone --recurse-submodules https://github.com/lvgl/lvgl.git
    cd lvgl
    git checkout release/v9.0
    cd ..
    log_success "LVGL v9.0 installed"
else
    log_info "LVGL already exists"
fi

# Create helper scripts
log_info "Creating helper scripts..."

# Serial test script
cat > test_serial.sh << 'EOF'
#!/bin/bash
echo "=== Serial Port Test ==="
echo "Available USB serial ports:"
ls /dev/tty{USB,ACM}* 2>/dev/null || echo "No USB serial ports found"
echo ""
echo "Connected USB devices:"
lsusb | grep -E "(Arduino|ESP32|CH340|CP210|FTDI)" || echo "No known serial devices found"
echo ""
echo "User groups (should include 'dialout'):"
groups
echo ""
echo "Test connection:"
echo "  screen /dev/ttyUSB0 115200"
echo "  (Ctrl+A then K then Y to exit)"
EOF
chmod +x test_serial.sh

# Audio routing script
cat > route_audio.sh << 'EOF'
#!/bin/bash
# Route all audio (including Bluetooth) to analog jack

echo "Routing audio to car speakers via 3.5mm jack..."

# Ensure PulseAudio is running
pulseaudio --check || pulseaudio --start

# Force default sink to analog jack
pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo 2>/dev/null ||
pactl set-default-sink alsa_output.platform-bcm2835_headphones.analog-stereo 2>/dev/null ||
pactl set-default-sink 0

# Move ALL existing audio streams to analog jack
pactl list short sink-inputs | cut -f1 | while read input; do
    pactl move-sink-input "$input" @DEFAULT_SINK@ 2>/dev/null || true
done

echo "✓ All audio routed to analog jack (car speakers)"
echo ""
echo "Current audio setup:"
echo "Default sink: $(pactl get-default-sink)"
echo "Available sinks:"
pactl list sinks short
EOF
chmod +x route_audio.sh

# Simple pairing helper
cat > pair_phone.sh << 'EOF'
#!/bin/bash
echo "=== TazzariAudio Bluetooth Pairing ==="
echo ""
echo "Your Pi is now 'TazzariAudio' - appears as speakers/headphones"
echo ""
echo "On your phone:"
echo "  1. Settings → Bluetooth"
echo "  2. Look for 'TazzariAudio'"
echo "  3. Tap to connect (no PIN needed)"
echo "  4. Play music - TazzariAudio should appear in audio output options"
echo ""
echo "Current Bluetooth status:"
hciconfig hci0 2>/dev/null | grep -E "(Name|UP|RUNNING)" || echo "Bluetooth not ready"
echo ""
echo "Connected devices:"
bluetoothctl devices Connected 2>/dev/null || echo "No devices connected"
echo ""
echo "If TazzariAudio doesn't appear:"
echo "  sudo systemctl restart bluetooth-a2dp"
echo "  ./route_audio.sh"
EOF
chmod +x pair_phone.sh

# Clean script  
cat > clean.sh << 'EOF'
#!/bin/bash
echo "Cleaning build artifacts..."
rm -rf build/
rm -f run_*.sh
echo "Clean complete!"
EOF
chmod +x clean.sh

# Start Bluetooth services
log_info "Starting Bluetooth services..."

# Restart services
sudo systemctl restart bluetooth
sleep 2
sudo systemctl start bluetooth-a2dp

# Restart PulseAudio
pulseaudio --kill 2>/dev/null || true
sleep 1
pulseaudio --start

# Configure Bluetooth controller
sudo hciconfig hci0 name 'TazzariAudio' 2>/dev/null || true
sudo hciconfig hci0 class 0x240404 2>/dev/null || true

# Make discoverable
bluetoothctl power on 2>/dev/null || true
bluetoothctl discoverable on 2>/dev/null || true
bluetoothctl pairable on 2>/dev/null || true

log_success "=== Complete Setup Finished! ==="
log_info ""
log_info "✓ Dependencies installed"
log_info "✓ LVGL library ready (v9.0)"
log_info "✓ Bluetooth A2DP configured (TazzariAudio)"
log_info "✓ Serial communication ready"
log_info "✓ Helper scripts created"
log_info ""
log_warning "IMPORTANT: LOGOUT and LOGIN to apply permissions!"
log_info ""
log_info "After logout/login:"
log_info "  ./build.sh              # Build dashboard"
log_info "  ./pair_phone.sh         # Pair your phone"
log_info "  ./route_audio.sh        # Route audio to speakers"
log_info "  ./test_serial.sh        # Test ESP32 connection"
log_info ""
log_info "Build options:"
log_info "  ./build.sh              # Development (windowed)"
log_info "  ./build.sh --deployment # Production (fullscreen)"
log_info ""

# Show current status
echo "Current status:"
systemctl is-active bluetooth >/dev/null && echo "✓ Bluetooth: Active" || echo "✗ Bluetooth: Failed"
systemctl is-active bluetooth-a2dp >/dev/null && echo "✓ A2DP Sink: Active" || echo "✗ A2DP Sink: Failed"  
pulseaudio --check && echo "✓ PulseAudio: Running" || echo "✗ PulseAudio: Not running"
hciconfig hci0 >/dev/null 2>&1 && echo "✓ Bluetooth Controller: Ready" || echo "✗ Bluetooth Controller: Not ready"

log_info ""
log_info "Your Pi should now appear as 'TazzariAudio' on phones!"
log_warning "Remember: LOGOUT/LOGIN required for serial port access"