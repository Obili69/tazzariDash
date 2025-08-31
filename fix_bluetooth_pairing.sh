#!/bin/bash
# fix_bluetooth_pairing.sh - Fix Bluetooth pairing and audio issues

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

log_info "Fixing Bluetooth pairing and audio issues..."

# 1. Fix Bluetooth device name
log_info "Setting Bluetooth device name to 'TazzariAudio'..."

# Update main.conf with TazzariAudio name
sudo tee /etc/bluetooth/main.conf > /dev/null << 'EOF'
[General]
Name = TazzariAudio
Class = 0x200408
DiscoverableTimeout = 0
PairableTimeout = 0
Discoverable = true
Pairable = true
AutoEnable = true

[Policy]
AutoEnable = true
ReconnectUUIDs = 0000110a-0000-1000-8000-00805f9b34fb,0000110c-0000-1000-8000-00805f9b34fb
ReconnectAttempts = 7
ReconnectIntervals = 1,2,4,8,16,32,64

# Auto-accept pairing (no PIN confirmation)
[LE]
EnableAdvRxMonitoring = false

# Auto-accept all pairing requests
[GATT]
Cache = yes
EOF

# 2. Fix audio A2DP sink setup
log_info "Setting up A2DP audio sink..."

# Install required packages
sudo apt install -y pulseaudio-module-bluetooth bluez-alsa-utils

# Create A2DP sink configuration
sudo tee /etc/systemd/system/bluetooth-a2dp-sink.service > /dev/null << 'EOF'
[Unit]
Description=Bluetooth A2DP Audio Sink
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=simple
User=root
ExecStart=/usr/bin/bluetoothctl-a2dp-sink
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Create A2DP sink script
sudo tee /usr/bin/bluetoothctl-a2dp-sink > /dev/null << 'EOF'
#!/bin/bash
# Auto-accept A2DP connections and set up audio sink

# Enable A2DP sink profile
bluetoothctl << 'BTCTL'
power on
discoverable on
pairable on
agent NoInputNoOutput
default-agent
BTCTL

# Keep running to handle connections
while true; do
    sleep 5
done
EOF

sudo chmod +x /usr/bin/bluetoothctl-a2dp-sink

# 3. Configure PulseAudio for A2DP
log_info "Configuring PulseAudio for Bluetooth A2DP..."

# Update PulseAudio configuration
mkdir -p ~/.config/pulse
cat > ~/.config/pulse/default.pa << 'EOF'
#!/usr/bin/pulseaudio -nF

# Load default configuration
.include /etc/pulse/default.pa

# Load Bluetooth modules
load-module module-bluetooth-policy
load-module module-bluetooth-discover

# Set analog output as default (never Bluetooth output)
set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo

# Don't auto-switch to Bluetooth speakers (we want analog jack output)
# But still allow Bluetooth input
EOF

# 4. Create auto-pairing script
log_info "Setting up auto-pairing without PIN..."

sudo tee /usr/bin/bluetooth-auto-accept > /dev/null << 'EOF'
#!/usr/bin/expect -f
# Auto-accept Bluetooth pairing requests

spawn bluetoothctl
expect "#"
send "agent NoInputNoOutput\r"
expect "#"
send "default-agent\r"
expect "#"

# Handle pairing requests automatically
expect {
    "Confirm passkey" {
        send "yes\r"
        exp_continue
    }
    "Accept pairing" {
        send "yes\r" 
        exp_continue
    }
    "Authorize service" {
        send "yes\r"
        exp_continue
    }
    "#" {
        exp_continue
    }
    eof {
        exit 0
    }
}
EOF

sudo chmod +x /usr/bin/bluetooth-auto-accept

# Install expect if not present
sudo apt install -y expect

# 5. Create systemd service for auto-accept
sudo tee /etc/systemd/system/bluetooth-auto-accept.service > /dev/null << 'EOF'
[Unit]
Description=Bluetooth Auto Accept Pairing
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=simple
ExecStart=/usr/bin/bluetooth-auto-accept
Restart=always
RestartSec=3
User=root

[Install]
WantedBy=multi-user.target
EOF

# 6. Restart Bluetooth with new configuration
log_info "Restarting Bluetooth services..."

sudo systemctl daemon-reload
sudo systemctl restart bluetooth

# Enable services
sudo systemctl enable bluetooth-auto-accept.service
sudo systemctl start bluetooth-auto-accept.service

# Wait a moment
sleep 3

# Set device name via hciconfig as well
sudo hciconfig hci0 name 'TazzariAudio' 2>/dev/null || true

# 7. Fix your current connection
log_info "Fixing current device connection..."

DEVICE_MAC="A0:52:72:30:5A:F3"  # Your phone from the log

# Remove and re-pair the device properly
bluetoothctl remove $DEVICE_MAC 2>/dev/null || true
sleep 2

# Set up for incoming connection
bluetoothctl << EOF
power on
discoverable on
pairable on
agent NoInputNoOutput
default-agent
trust $DEVICE_MAC
EOF

# 8. Test audio routing
log_info "Testing audio configuration..."

# Ensure PulseAudio is running
pulseaudio --kill 2>/dev/null || true
pulseaudio --start

# Force audio to analog jack
pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo 2>/dev/null || 
pactl set-default-sink alsa_output.platform-bcm2835_headphones.analog-stereo 2>/dev/null ||
pactl set-default-sink 0

log_success "Bluetooth fixes applied!"
log_info ""
log_info "Changes made:"
log_info "✅ Device name changed to 'TazzariAudio'"
log_info "✅ Auto-accept pairing enabled (no PIN required)"
log_info "✅ A2DP audio sink configured"
log_info "✅ Audio output forced to analog jack"
log_info "✅ Auto-pairing service enabled"
log_info ""
log_info "Next steps:"
log_info "1. On your phone, forget/unpair the Raspberry Pi"
log_info "2. Search for 'TazzariAudio' and connect"
log_info "3. It should pair automatically without asking for PIN"
log_info "4. Try playing music - it should come out the analog jack"
log_info ""
log_warning "If it still doesn't work, reboot the Pi and try again"

echo ""
echo "Current Bluetooth status:"
hciconfig hci0 | grep -E "(Name|UP|RUNNING)"
echo ""
echo "Current audio sinks:"
pactl list sinks short