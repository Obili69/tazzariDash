#!/bin/bash
# improved_bt_setup.sh - Reliable Bluetooth A2DP setup for Pi 3B+ and Pi 5

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[BT-SETUP]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

if [[ $EUID -eq 0 ]]; then
   echo "Error: Don't run as root. Use regular user with sudo access."
   exit 1
fi

log_info "=== Improved Bluetooth A2DP Audio Setup ==="

# Install dependencies
log_info "Installing required packages..."
sudo apt update
sudo apt install -y \
    bluetooth \
    bluez \
    bluez-tools \
    pulseaudio \
    pulseaudio-module-bluetooth \
    libdbus-1-dev \
    libpulse-dev \
    libasound2-dev \
    pavucontrol

# Create optimized Bluetooth configuration
log_info "Configuring Bluetooth for A2DP sink..."

sudo tee /etc/bluetooth/main.conf > /dev/null << 'EOF'
[General]
# Make Pi appear as "TazzariAudio" 
Name = TazzariAudio
# Audio device class - appears as speaker/headphones
Class = 0x240404
DiscoverableTimeout = 0
PairableTimeout = 0
Discoverable = yes
Pairable = yes
AutoEnable = yes
# Fast connection - important for automotive use
FastConnectable = true

[Policy]
AutoEnable = yes
# Auto-reconnect A2DP profiles quickly
ReconnectUUIDs = 0000110a-0000-1000-8000-00805f9b34fb,0000110b-0000-1000-8000-00805f9b34fb
ReconnectAttempts = 5
ReconnectIntervals = 1,2,4

# Trust devices automatically after first pair
JustWorksRepairing = confirm
EOF

# Configure PulseAudio for reliable A2DP sink
log_info "Configuring PulseAudio..."

mkdir -p ~/.config/pulse

cat > ~/.config/pulse/default.pa << 'EOF'
#!/usr/bin/pulseaudio -nF
# Load default configuration
.include /etc/pulse/default.pa

# Bluetooth A2DP sink configuration
load-module module-bluetooth-policy auto_switch=0
load-module module-bluetooth-discover headset=auto

# Route ALL Bluetooth audio to analog jack
load-module module-loopback source=bluez_source.* sink=alsa_output.* latency_msec=50

# Prevent auto-switching away from analog output
unload-module module-switch-on-connect
EOF

# Create systemd service for reliable Bluetooth management
log_info "Creating Bluetooth management service..."

sudo tee /usr/bin/tazzari-bluetooth << 'EOF'
#!/bin/bash
# TazzariAudio Bluetooth management

# Ensure Bluetooth is ready
sleep 2
hciconfig hci0 up
hciconfig hci0 name 'TazzariAudio'
hciconfig hci0 class 0x240404
hciconfig hci0 piscan

# Make discoverable with auto-accept pairing
bluetoothctl << BTCTL_EOF
power on
discoverable on  
pairable on
agent NoInputNoOutput
default-agent
BTCTL_EOF

# Monitor and maintain discoverable state
while true; do
    if ! bluetoothctl show | grep -q "Discoverable: yes"; then
        bluetoothctl discoverable on
    fi
    
    # Force audio routing every minute
    pactl set-default-sink alsa_output.* 2>/dev/null || true
    
    sleep 60
done
EOF

sudo chmod +x /usr/bin/tazzari-bluetooth

sudo tee /etc/systemd/system/tazzari-bluetooth.service > /dev/null << 'EOF'
[Unit]
Description=TazzariAudio Bluetooth A2DP Service
After=bluetooth.service pulseaudio.service
Wants=bluetooth.service

[Service]
Type=simple
User=root
ExecStart=/usr/bin/tazzari-bluetooth
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Pi model detection and specific configuration
log_info "Detecting Raspberry Pi model..."

if grep -q "Raspberry Pi 5" /proc/cpuinfo; then
    log_info "Raspberry Pi 5 detected - configuring audio routing..."
    echo 'ANALOG_SINK="alsa_output.platform-bcm2835_headphones.analog-stereo"' > ~/.config/tazzari_audio
elif grep -q "Raspberry Pi 4\|Raspberry Pi 3" /proc/cpuinfo; then
    log_info "Raspberry Pi 3/4 detected - configuring audio routing..."
    echo 'ANALOG_SINK="alsa_output.platform-bcm2835_audio.analog-stereo"' > ~/.config/tazzari_audio
else
    log_info "Generic Linux detected - using default audio routing..."
    echo 'ANALOG_SINK="0"' > ~/.config/tazzari_audio
fi

# Create audio routing helper
cat > ~/route_bt_audio.sh << 'EOF'
#!/bin/bash
# Route Bluetooth audio to car speakers via 3.5mm jack

source ~/.config/tazzari_audio 2>/dev/null || ANALOG_SINK="0"

echo "Routing Bluetooth audio to car speakers..."

# Ensure PulseAudio is running
pulseaudio --check || pulseaudio --start

# Set default output to analog jack
pactl set-default-sink "$ANALOG_SINK" 2>/dev/null || pactl set-default-sink 0

# Move all current streams to analog output  
pactl list short sink-inputs | while read -r input_id sink_id; do
    pactl move-sink-input "$input_id" "$ANALOG_SINK" 2>/dev/null || true
done

# Create loopback for future Bluetooth connections
pactl unload-module module-loopback 2>/dev/null || true
pactl load-module module-loopback source=bluez_source sink="$ANALOG_SINK" latency_msec=50 2>/dev/null || true

echo "✓ Audio routing configured - Bluetooth → Analog Jack → Car Speakers"
pactl info | grep "Default Sink"
EOF

chmod +x ~/route_bt_audio.sh

# Simple pairing helper
cat > ~/pair_device.sh << 'EOF'
#!/bin/bash
echo "=== TazzariAudio Bluetooth Pairing ==="
echo ""
echo "Your Raspberry Pi appears as 'TazzariAudio' to phones"
echo ""
echo "To pair your phone:"
echo "1. Phone Settings → Bluetooth"  
echo "2. Look for 'TazzariAudio' in available devices"
echo "3. Tap to connect (no PIN required)"
echo "4. Play music → Select 'TazzariAudio' as audio output"
echo ""
echo "Current Bluetooth status:"
hciconfig hci0 2>/dev/null | head -3 || echo "Bluetooth not ready"
echo ""
echo "Connected devices:"
bluetoothctl devices Connected || echo "No devices connected yet"
echo ""
echo "If pairing fails:"
echo "  sudo systemctl restart tazzari-bluetooth"
echo "  ./route_bt_audio.sh"
EOF

chmod +x ~/pair_device.sh

# Enable and start services
log_info "Enabling Bluetooth services..."

sudo systemctl daemon-reload
sudo systemctl enable bluetooth
sudo systemctl enable tazzari-bluetooth

# Start services
sudo systemctl start bluetooth
sleep 2
sudo systemctl start tazzari-bluetooth

# Configure audio routing
log_info "Setting up audio routing..."
./route_bt_audio.sh

# Test Bluetooth functionality
log_info "Testing Bluetooth functionality..."

if hciconfig hci0 | grep -q "UP RUNNING"; then
    log_success "Bluetooth adapter is active"
else
    log_warning "Bluetooth adapter may not be ready"
fi

if bluetoothctl show | grep -q "Discoverable: yes"; then
    log_success "Device is discoverable as TazzariAudio"
else
    log_warning "Device may not be discoverable"
fi

if pactl info >/dev/null 2>&1; then
    log_success "PulseAudio is running"
else
    log_warning "PulseAudio is not running"
fi

log_success "=== Bluetooth A2DP Setup Complete! ==="
log_info ""
log_info "✓ TazzariAudio Bluetooth service configured"
log_info "✓ Audio routing to analog jack (car speakers)"
log_info "✓ Auto-pairing enabled (no PIN required)"
log_info "✓ Pi 3B+ and Pi 5 compatible"
log_info ""
log_info "Usage:"
log_info "  ./pair_device.sh        # Pairing instructions"
log_info "  ./route_bt_audio.sh     # Fix audio routing"
log_info ""
log_info "Service management:"
log_info "  sudo systemctl status tazzari-bluetooth"
log_info "  sudo systemctl restart tazzari-bluetooth"
log_info ""
log_info "Your Pi is now discoverable as 'TazzariAudio'!"
log_info "Phones will see it as wireless speakers/headphones."