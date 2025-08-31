#!/bin/bash
# setup_beocreate_bluetooth.sh - BeoCreate 4 DSP + Simple Bluetooth for Pi 3B+

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[SETUP]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

if [[ $EUID -eq 0 ]]; then
   log_error "Don't run as root. Use regular user with sudo access."
   exit 1
fi

# Detect Pi model
if grep -q "Raspberry Pi 3 Model B Plus" /proc/cpuinfo 2>/dev/null; then
    log_info "Detected Raspberry Pi 3B+ - optimizing for BCM43438"
    IS_PI3B_PLUS=true
else
    log_warning "Not Pi 3B+ - some optimizations may not apply"
    IS_PI3B_PLUS=false
fi

log_info "=== BeoCreate 4 DSP + Simple Bluetooth Setup ==="
log_info ""
log_info "This will configure:"
log_info "  • HiFiBerry BeoCreate 4 device tree overlay"
log_info "  • DSP volume control via ALSA"
log_info "  • Simple Bluetooth A2DP (optimized for Pi 3B+)"
log_info "  • Remove complex PulseAudio setup"
log_info ""

read -p "Continue with setup? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Setup cancelled"
    exit 0
fi

# Update system
log_info "Updating system packages..."
sudo apt update

# Install basic dependencies
log_info "Installing dependencies..."
sudo apt install -y \
    alsa-utils \
    libasound2-dev \
    bluetooth \
    bluez \
    bluez-tools

# Remove complex audio packages that cause issues
log_info "Removing complex audio packages..."
sudo apt remove --purge -y pulseaudio pulseaudio-module-bluetooth blueman 2>/dev/null || true
sudo apt autoremove -y

# Configure BeoCreate 4 device tree overlay
log_info "Configuring BeoCreate 4 overlay..."

# Backup config.txt
sudo cp /boot/config.txt /boot/config.txt.backup.$(date +%Y%m%d_%H%M%S)

# Remove any existing HiFiBerry overlays
sudo sed -i '/dtoverlay=hifiberry/d' /boot/config.txt
sudo sed -i '/dtoverlay=iqaudio/d' /boot/config.txt

# Add BeoCreate overlay
if ! grep -q "dtoverlay=hifiberry-dacplus" /boot/config.txt; then
    echo "" | sudo tee -a /boot/config.txt
    echo "# HiFiBerry BeoCreate 4" | sudo tee -a /boot/config.txt
    echo "dtoverlay=hifiberry-dacplus" | sudo tee -a /boot/config.txt
    log_success "Added BeoCreate overlay"
fi

# Enable SPI for DSP communication
if ! grep -q "dtparam=spi=on" /boot/config.txt; then
    echo "dtparam=spi=on" | sudo tee -a /boot/config.txt
    log_success "Enabled SPI"
fi

# Disable onboard audio to avoid conflicts
if ! grep -q "dtparam=audio=off" /boot/config.txt; then
    echo "dtparam=audio=off" | sudo tee -a /boot/config.txt
    log_success "Disabled onboard audio"
fi

# Configure ALSA for BeoCreate
log_info "Configuring ALSA for BeoCreate..."

# Create ALSA configuration
cat > ~/.asoundrc << 'EOF'
# ALSA configuration for BeoCreate 4
pcm.!default {
    type hw
    card 0
    device 0
}

ctl.!default {
    type hw
    card 0
}

# Software volume control fallback
pcm.softvol {
    type softvol
    slave {
        pcm "hw:0,0"
    }
    control {
        name "Master"
        card 0
    }
}
EOF

# Simple Bluetooth configuration for Pi 3B+
if [ "$IS_PI3B_PLUS" = true ]; then
    log_info "Configuring simple Bluetooth for Pi 3B+..."
    
    # Simple main.conf - no complex features
    sudo tee /etc/bluetooth/main.conf > /dev/null << 'EOF'
[General]
Name = TazzariAudio
Class = 0x240404
DiscoverableTimeout = 0
Discoverable = yes
Pairable = yes

[Policy]
AutoEnable = yes
EOF

    log_success "Simple Bluetooth configured"
fi

# Create simple Bluetooth service
log_info "Creating simple Bluetooth service..."

sudo tee /usr/local/bin/simple-bluetooth-setup << 'EOF'
#!/bin/bash
# Simple Bluetooth setup for automotive use

# Wait for system to be ready
sleep 5

# Configure Bluetooth controller
hciconfig hci0 up 2>/dev/null || true
hciconfig hci0 name 'TazzariAudio' 2>/dev/null || true
hciconfig hci0 class 0x240404 2>/dev/null || true

# Set discoverable
bluetoothctl power on 2>/dev/null || true
bluetoothctl discoverable on 2>/dev/null || true
bluetoothctl pairable on 2>/dev/null || true

# Keep running with periodic maintenance
while true; do
    # Ensure discoverable every 60 seconds
    bluetoothctl discoverable on >/dev/null 2>&1 || true
    sleep 60
done
EOF

sudo chmod +x /usr/local/bin/simple-bluetooth-setup

# Create service file
sudo tee /etc/systemd/system/simple-bluetooth.service > /dev/null << 'EOF'
[Unit]
Description=Simple Bluetooth for TazzariAudio
After=bluetooth.service
Wants=bluetooth.service

[Service]
Type=simple
ExecStart=/usr/local/bin/simple-bluetooth-setup
Restart=always
RestartSec=10
User=root

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable simple-bluetooth

# Create test scripts
log_info "Creating test scripts..."

# BeoCreate volume test
cat > ~/test_beocreate_volume.sh << 'EOF'
#!/bin/bash
echo "=== BeoCreate 4 Volume Test ==="
echo ""

echo "Sound cards:"
cat /proc/asound/cards 2>/dev/null || echo "No sound cards found"

echo ""
echo "ALSA controls:"
if amixer controls | grep -q .; then
    echo "Available controls:"
    amixer controls | head -5
    
    echo ""
    echo "Testing volume controls:"
    
    # Test DSPVolume (BeoCreate specific)
    if amixer sget DSPVolume >/dev/null 2>&1; then
        echo "✓ DSPVolume control found"
        amixer sget DSPVolume | grep -E '\[.*%\]'
        
        echo "Setting DSPVolume to 50%..."
        amixer sset DSPVolume 50% 2>/dev/null || echo "Failed to set DSPVolume"
    else
        echo "- DSPVolume not found (try rebooting after overlay setup)"
    fi
    
    # Test Master volume
    if amixer sget Master >/dev/null 2>&1; then
        echo "✓ Master volume found"
        amixer sget Master | grep -E '\[.*%\]'
    else
        echo "- Master volume not found"
    fi
    
else
    echo "No ALSA controls found - check hardware setup"
fi

echo ""
echo "Test audio output:"
echo "  speaker-test -c 2 -t wav -l 1"
EOF

# Simple Bluetooth test
cat > ~/test_simple_bluetooth.sh << 'EOF'
#!/bin/bash
echo "=== Simple Bluetooth Test ==="
echo ""

echo "Bluetooth controller:"
if hciconfig hci0 >/dev/null 2>&1; then
    hciconfig hci0 | grep -E "(Name|UP|RUNNING|Class)"
else
    echo "Bluetooth controller not ready"
fi

echo ""
echo "Service status:"
systemctl is-active bluetooth && echo "✓ bluetooth: active" || echo "✗ bluetooth: inactive"
systemctl is-active simple-bluetooth && echo "✓ simple-bluetooth: active" || echo "✗ simple-bluetooth: inactive"

echo ""
echo "Pairing info:"
echo "On your phone: Settings → Bluetooth → Look for 'TazzariAudio'"
echo "No PIN required - should connect automatically"

echo ""
echo "Connected devices:"
bluetoothctl devices Connected 2>/dev/null || echo "No devices connected"

echo ""
echo "To connect manually:"
echo "  bluetoothctl"
echo "  > scan on"
echo "  > pair XX:XX:XX:XX:XX:XX"
echo "  > connect XX:XX:XX:XX:XX:XX"
EOF

# Audio routing script
cat > ~/route_to_beocreate.sh << 'EOF'
#!/bin/bash
echo "Configuring audio routing for BeoCreate..."

# Set BeoCreate as default
export ALSA_CARD=0

echo "Available audio devices:"
cat /proc/asound/cards

echo ""
echo "Current ALSA configuration:"
echo "  Default card: $(cat /proc/asound/cards | head -1 | cut -d':' -f1 | tr -d ' ')"

echo ""
echo "Testing audio output:"
echo "  aplay /usr/share/sounds/alsa/Front_Left.wav"
echo "  speaker-test -c 2 -t wav -l 1"

# Ensure BeoCreate is card 0
if grep -q "sndrpihifiberry" /proc/asound/cards; then
    echo "✓ BeoCreate detected as audio device"
else
    echo "⚠ BeoCreate not detected - check overlay and reboot"
fi
EOF

# Make all scripts executable
chmod +x ~/test_beocreate_volume.sh
chmod +x ~/test_simple_bluetooth.sh
chmod +x ~/route_to_beocreate.sh

# Enable services
log_info "Enabling services..."
sudo systemctl enable bluetooth

log_success "=== Setup Complete! ==="
log_info ""
log_success "✓ BeoCreate 4 overlay configured"
log_success "✓ ALSA configured for DSP audio"
log_success "✓ Simple Bluetooth service created"
log_success "✓ Complex PulseAudio removed"
log_success "✓ Test scripts created"
log_info ""
log_warning "REBOOT REQUIRED to load device tree overlay!"
log_info ""
log_info "After reboot:"
log_info "  ./test_beocreate_volume.sh    # Test DSP volume"
log_info "  ./test_simple_bluetooth.sh    # Test Bluetooth"
log_info "  ./route_to_beocreate.sh       # Test audio routing"
log_info ""
log_info "Your Pi will appear as 'TazzariAudio' on phones"
log_info "Volume control via: amixer sset DSPVolume 75%"
log_info ""

if [ "$IS_PI3B_PLUS" = true ]; then
    log_info "Pi 3B+ optimizations applied:"
    log_info "  • BCM43438 specific Bluetooth config"
    log_info "  • Removed PulseAudio conflicts"
    log_info "  • Simple service for reliability"
fi

echo ""
read -p "Reboot now? (y/N): " -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    log_info "Rebooting..."
    sudo reboot
else
    echo ""
    log_warning "Remember to reboot before testing!"
fi