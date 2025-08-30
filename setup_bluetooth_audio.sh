#!/bin/bash

# Bluetooth Audio Setup Script for LVGL Dashboard
# Sets up Bluetooth audio for Raspberry Pi 3B+ and Pi 5

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

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   log_error "This script should not be run as root. Run as regular user with sudo access."
   exit 1
fi

log_info "Setting up Bluetooth Audio for LVGL Dashboard..."
log_info "This will configure:"
log_info "  • Bluetooth system (BlueZ)"
log_info "  • Audio routing (PulseAudio)"
log_info "  • D-Bus communication"
log_info "  • Auto-pairing and media control"
log_info "  • Analog audio jack output"

read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Setup cancelled"
    exit 0
fi

# Detect Raspberry Pi model
PI_MODEL=""
if grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    PI_MODEL=$(grep "Raspberry Pi" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    log_info "Detected: $PI_MODEL"
else
    log_info "Running on generic Linux system"
fi

# Update system
log_info "Updating package lists..."
sudo apt update

# Install Bluetooth packages
log_info "Installing Bluetooth support..."
sudo apt install -y \
    bluetooth \
    bluez \
    bluez-tools \
    bluez-firmware \
    libdbus-1-dev \
    dbus \
    dbus-user-session

# Install audio packages (if not already installed)
log_info "Installing/updating audio support..."
sudo apt install -y \
    pulseaudio \
    pulseaudio-module-bluetooth \
    pulseaudio-utils \
    alsa-utils \
    libasound2-dev \
    libpulse-dev

# Enable and start services
log_info "Configuring system services..."
sudo systemctl enable bluetooth
sudo systemctl start bluetooth
sudo systemctl enable dbus
sudo systemctl start dbus

# Configure PulseAudio for user session
log_info "Configuring PulseAudio for user session..."
systemctl --user enable pulseaudio
systemctl --user start pulseaudio || true

# Add user to bluetooth group
log_info "Adding user to bluetooth group..."
sudo usermod -a -G bluetooth $USER

# Configure Bluetooth for auto-pairing
log_info "Configuring Bluetooth for automotive use..."

# Create Bluetooth main.conf if it doesn't exist
BLUEZ_CONF="/etc/bluetooth/main.conf"
if [ ! -f "$BLUEZ_CONF" ]; then
    sudo touch "$BLUEZ_CONF"
fi

# Backup original config
sudo cp "$BLUEZ_CONF" "$BLUEZ_CONF.backup" 2>/dev/null || true

# Configure Bluetooth settings for automotive dashboard
sudo tee "$BLUEZ_CONF" > /dev/null << 'EOF'
[General]
Name = TazzariDash
Class = 0x200408
DiscoverableTimeout = 0
PairableTimeout = 0
Discoverable = true
Pairable = true
AutoEnable = true

[Policy]
AutoEnable = true

[LE]
EnableAdvRxMonitoring = false
EOF

# Configure PulseAudio for Bluetooth
log_info "Configuring PulseAudio for Bluetooth audio..."

# Create PulseAudio config directory
mkdir -p ~/.config/pulse

# Configure PulseAudio to auto-switch to Bluetooth when connected
cat > ~/.config/pulse/default.pa << 'EOF'
#!/usr/bin/pulseaudio -nF

# Load default configuration
.include /etc/pulse/default.pa

# Automatically switch to Bluetooth when A2DP device connects
load-module module-switch-on-connect

# Set analog output as fallback
set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo
EOF

# Create audio routing script
log_info "Creating audio routing helper..."
cat > ~/route_audio_to_jack.sh << 'EOF'
#!/bin/bash
# Route audio to 3.5mm jack (fallback if Bluetooth disconnects)

# Pi 3B+ / Pi 4 analog output
pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo 2>/dev/null ||
# Pi 5 analog output (may have different naming)
pactl set-default-sink alsa_output.platform-bcm2835_headphones.analog-stereo 2>/dev/null ||
# Generic fallback
pactl set-default-sink 0 2>/dev/null ||
echo "Could not set analog audio output"

echo "Audio routed to analog jack"
EOF
chmod +x ~/route_audio_to_jack.sh

# Create Bluetooth pairing helper script
log_info "Creating Bluetooth pairing helper..."
cat > ~/pair_bluetooth_device.sh << 'EOF'
#!/bin/bash
# Interactive Bluetooth device pairing for dashboard

echo "=== Bluetooth Device Pairing ==="
echo ""
echo "Make sure your phone/device is in pairing mode, then:"
echo "1. Put your device in Bluetooth pairing mode"
echo "2. This script will scan for 10 seconds"
echo "3. Select your device from the list"
echo ""

read -p "Press Enter to start scanning..."

# Start scan
bluetoothctl scan on &
SCAN_PID=$!

echo "Scanning for devices (10 seconds)..."
sleep 10

# Stop scan
kill $SCAN_PID 2>/dev/null || true
bluetoothctl scan off

echo ""
echo "Available devices:"
bluetoothctl devices

echo ""
read -p "Enter device MAC address to pair (e.g., AA:BB:CC:DD:EE:FF): " MAC_ADDRESS

if [ -n "$MAC_ADDRESS" ]; then
    echo "Pairing with $MAC_ADDRESS..."
    bluetoothctl pair "$MAC_ADDRESS"
    bluetoothctl trust "$MAC_ADDRESS"
    bluetoothctl connect "$MAC_ADDRESS"
    echo "Pairing complete!"
else
    echo "No device specified"
fi
EOF
chmod +x ~/pair_bluetooth_device.sh

# Configure audio jack settings for Pi models
if [[ -n "$PI_MODEL" ]]; then
    log_info "Configuring Raspberry Pi audio settings..."
    
    # Force audio to 3.5mm jack
    sudo raspi-config nonint do_audio 1 2>/dev/null || true
    
    # Add audio configuration to boot config if not present
    BOOT_CONFIG="/boot/config.txt"
    if [ -f "$BOOT_CONFIG" ]; then
        if ! grep -q "dtparam=audio=on" "$BOOT_CONFIG"; then
            echo "dtparam=audio=on" | sudo tee -a "$BOOT_CONFIG"
            log_info "Enabled audio in boot configuration"
        fi
        
        # Ensure adequate audio buffer for automotive use
        if ! grep -q "audio_pwm_mode" "$BOOT_CONFIG"; then
            echo "dtparam=audio_pwm_mode=2" | sudo tee -a "$BOOT_CONFIG"
            log_info "Set audio PWM mode for better quality"
        fi
    fi
fi

# Restart Bluetooth service with new configuration
log_info "Restarting Bluetooth service..."
sudo systemctl restart bluetooth

# Wait for service to start
sleep 2

# Configure Bluetooth controller
log_info "Configuring Bluetooth controller..."
sudo hciconfig hci0 up 2>/dev/null || log_warning "Could not bring up Bluetooth controller"

# Initial Bluetooth setup
bluetoothctl power on
bluetoothctl agent on
bluetoothctl default-agent
bluetoothctl discoverable on
bluetoothctl pairable on

# Test audio system
log_info "Testing audio system..."
if command -v speaker-test >/dev/null 2>&1; then
    log_info "Run 'speaker-test -c 2 -t sine' to test audio output"
else
    log_warning "speaker-test not available"
fi

# Create systemd service for auto-start (optional)
log_info "Creating dashboard auto-start service..."
cat > ~/.config/systemd/user/tazzari-dashboard.service << EOF
[Unit]
Description=Tazzari Dashboard
After=graphical-session.target bluetooth.target

[Service]
Type=simple
ExecStart=$PWD/build/LVGLDashboard_deployment
WorkingDirectory=$PWD
Restart=always
RestartSec=5
Environment=DISPLAY=:0

[Install]
WantedBy=default.target
EOF

# Enable service (but don't start it automatically)
systemctl --user daemon-reload
log_info "Dashboard service created (not enabled). Enable with: systemctl --user enable tazzari-dashboard.service"

log_success "Bluetooth Audio setup complete!"
log_info ""
log_info "Next steps:"
log_info "1. LOGOUT and LOGIN to apply group changes"
log_info "2. Build project: ./build.sh"
log_info "3. Pair your phone: ./pair_bluetooth_device.sh"
log_info "4. Test audio routing: ./route_audio_to_jack.sh"
log_info "5. Run dashboard: ./build/LVGLDashboard_dev"
log_info ""
log_info "Bluetooth Status:"
echo "  Controller: $(hciconfig hci0 | grep -o 'UP RUNNING' || echo 'Not active')"
echo "  Discoverable: $(bluetoothctl show | grep 'Discoverable: yes' > /dev/null && echo 'Yes' || echo 'No')"
echo "  Audio modules: $(pulseaudio --check && echo 'PulseAudio running' || echo 'PulseAudio not running')"

log_info ""
log_warning "Remember: LOGOUT and LOGIN required for group permissions!"
log_info "Use 'bluetoothctl' for manual Bluetooth control"
log_info "Use 'pactl list sinks' to check audio outputs"