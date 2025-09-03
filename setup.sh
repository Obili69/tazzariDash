#!/bin/bash
# setup.sh - Multi-Audio Hardware TazzariAudio Dashboard Setup

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

DEVICE_NAME="TazzariAudio"
DASHBOARD_PATH="$(pwd)"
AUDIO_HARDWARE=""

# Parse arguments for audio hardware selection
while [[ $# -gt 0 ]]; do
    case $1 in
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
            echo "TazzariAudio Dashboard Setup with Multi-Audio Hardware Support"
            echo "Usage: $0 [audio-option]"
            echo ""
            echo "Audio Hardware Options:"
            echo "  --audio-aux         Built-in 3.5mm jack (PulseAudio + alsaeq)"
            echo "  --audio-dac         HiFiBerry DAC+ (ALSA hardware volume + alsaeq)"
            echo "  --audio-amp4        HiFiBerry AMP4 (ALSA hardware volume + alsaeq)"
            echo "  --audio-beocreate4  HiFiBerry BeoCreate 4 (DSP REST API)"
            echo ""
            echo "If no option is specified, an interactive menu will be shown."
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

# Interactive menu if no hardware specified
if [ -z "$AUDIO_HARDWARE" ]; then
    log_info "=== TazzariAudio Dashboard Audio Hardware Selection ==="
    echo ""
    echo "Select your audio hardware:"
    echo "  1) Built-in 3.5mm Jack (AUX)"
    echo "     - Uses Pi's built-in audio output"
    echo "     - Software volume control via PulseAudio"
    echo "     - Software EQ via alsaeq plugin"
    echo "     - No additional hardware required"
    echo ""
    echo "  2) HiFiBerry DAC+ (DAC)"
    echo "     - High-quality DAC with line output"
    echo "     - Hardware volume control via ALSA"
    echo "     - Software EQ via alsaeq plugin"
    echo "     - Requires external amplifier"
    echo ""
    echo "  3) HiFiBerry AMP4 (AMP4)"
    echo "     - High-quality DAC with built-in amplifier"
    echo "     - Hardware volume control via ALSA"  
    echo "     - Software EQ via alsaeq plugin"
    echo "     - Direct speaker connection"
    echo ""
    echo "  4) HiFiBerry BeoCreate 4 DSP (BEOCREATE4)"
    echo "     - Professional 4-channel DSP + amplifiers"
    echo "     - Hardware volume control via REST API"
    echo "     - Hardware EQ via DSP biquad filters"
    echo "     - Advanced crossover and room correction"
    echo ""
    
    while true; do
        read -p "Choose [1-4]: " choice
        case $choice in
            1) AUDIO_HARDWARE="AUX"; break;;
            2) AUDIO_HARDWARE="DAC"; break;;
            3) AUDIO_HARDWARE="AMP4"; break;;
            4) AUDIO_HARDWARE="BEOCREATE4"; break;;
            *) echo "Invalid choice. Please enter 1-4.";;
        esac
    done
fi

log_info "=== TazzariAudio Dashboard Setup - $AUDIO_HARDWARE ==="

# Display hardware-specific information
case $AUDIO_HARDWARE in
    "AUX")
        log_info "Setting up built-in 3.5mm jack audio system"
        log_info "  • PulseAudio for volume control"
        log_info "  • alsaeq plugin for equalization"
        log_info "  • No additional hardware required"
        ;;
    "DAC")
        log_info "Setting up HiFiBerry DAC+ audio system"
        log_info "  • Hardware volume control via ALSA 'Digital' mixer"
        log_info "  • alsaeq plugin for equalization"
        log_info "  • Requires external amplifier"
        ;;
    "AMP4")
        log_info "Setting up HiFiBerry AMP4 audio system"
        log_info "  • Hardware volume control via ALSA 'Digital' mixer"
        log_info "  • alsaeq plugin for equalization"
        log_info "  • Built-in amplifier for direct speaker connection"
        ;;
    "BEOCREATE4")
        log_info "Setting up HiFiBerry BeoCreate 4 DSP audio system"
        log_info "  • Hardware volume control via REST API"
        log_info "  • Hardware EQ via DSP biquad filters"
        log_info "  • Professional 4-channel amplifier with crossover"
        ;;
esac

read -p "Continue with $AUDIO_HARDWARE setup? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Setup cancelled"
    exit 0
fi

# Update system
log_info "Updating system packages..."
sudo apt update

# Install common dependencies
log_info "Installing common dependencies..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    libsdl2-dev \
    libsdl2-image-dev \
    bluetooth \
    bluez \
    bluez-tools \
    python3-dbus \
    python3-gi

# Install audio hardware specific dependencies
case $AUDIO_HARDWARE in
    "AUX")
        log_info "Installing PulseAudio dependencies..."
        sudo apt install -y \
            pulseaudio \
            pulseaudio-module-bluetooth \
            libasound2-plugin-equal \
            alsa-utils \
            libpulse-dev
        ;;
        
    "DAC"|"AMP4")
        log_info "Installing ALSA dependencies for HiFiBerry..."
        sudo apt install -y \
            libasound2-dev \
            libasound2-plugin-equal \
            alsa-utils
        ;;
        
    "BEOCREATE4")
        log_info "Installing HiFiBerry BeoCreate 4 dependencies..."
        # Install HiFiBerry repository
        curl -1sLf 'https://dl.cloudsmith.io/public/hifiberry/hifiberry/setup.deb.sh' | sudo -E bash
        sudo apt update
        sudo apt install -y \
            hifiberry-dsp \
            hifiberry-dspprofiles \
            sigmatcpserver \
            libcurl4-openssl-dev
        ;;
esac

# Configure boot settings based on audio hardware
log_info "Configuring boot settings..."

# Remove old audio settings
sudo sed -i '/^dtparam=audio=/d' /boot/firmware/config.txt 2>/dev/null || sudo sed -i '/^dtparam=audio=/d' /boot/config.txt
sudo sed -i '/^dtoverlay=hifiberry-/d' /boot/firmware/config.txt 2>/dev/null || sudo sed -i '/^dtoverlay=hifiberry-/d' /boot/config.txt

# Add hardware-specific configuration
case $AUDIO_HARDWARE in
    "AUX")
        if [ -f "/boot/firmware/config.txt" ]; then
            echo "dtparam=audio=on" | sudo tee -a /boot/firmware/config.txt
        else
            echo "dtparam=audio=on" | sudo tee -a /boot/config.txt
        fi
        log_success "Built-in audio enabled in boot config"
        ;;
        
    "DAC"|"AMP4")
        if [ -f "/boot/firmware/config.txt" ]; then
            sudo tee -a /boot/firmware/config.txt > /dev/null <<EOF
dtparam=audio=off
dtoverlay=hifiberry-dacplus-std
EOF
        else
            sudo tee -a /boot/config.txt > /dev/null <<EOF
dtparam=audio=off
dtoverlay=hifiberry-dacplus-std
EOF
        fi
        log_success "HiFiBerry $AUDIO_HARDWARE configuration added to boot config"
        ;;
        
    "BEOCREATE4")
        if [ -f "/boot/firmware/config.txt" ]; then
            sudo tee -a /boot/firmware/config.txt > /dev/null <<EOF
dtparam=audio=off
dtparam=i2c_arm=on
dtparam=i2s=on
dtparam=spi=on
dtoverlay=hifiberry-dac
EOF
        else
            sudo tee -a /boot/config.txt > /dev/null <<EOF
dtparam=audio=off
dtparam=i2c_arm=on
dtparam=i2s=on
dtparam=spi=on
dtoverlay=hifiberry-dac
EOF
        fi
        log_success "HiFiBerry BeoCreate 4 configuration added to boot config"
        ;;
esac

# Set hostname
log_info "Setting hostname to $DEVICE_NAME..."
sudo hostnamectl set-hostname "$DEVICE_NAME"
sudo sed -i "s/raspberrypi/$DEVICE_NAME/g" /etc/hosts 2>/dev/null || true

# Configure Bluetooth (common to all hardware)
log_info "Configuring Bluetooth..."
sudo tee /etc/bluetooth/main.conf > /dev/null <<EOF
[General]
Name = $DEVICE_NAME
Class = 0x2C0414
DiscoverableTimeout = 0
EOF

# Create A2DP agent
sudo tee /usr/local/bin/a2dp-agent > /dev/null <<'EOF'
#!/usr/bin/env python3
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

class Agent(dbus.service.Object):
    @dbus.service.method("org.bluez.Agent1", in_signature="os", out_signature="")
    def AuthorizeService(self, device, uuid): return
    @dbus.service.method("org.bluez.Agent1", in_signature="o", out_signature="s") 
    def RequestPinCode(self, device): return "0000"
    @dbus.service.method("org.bluez.Agent1", in_signature="ou", out_signature="")
    def RequestConfirmation(self, device, passkey): return
    @dbus.service.method("org.bluez.Agent1", in_signature="o", out_signature="")
    def RequestAuthorization(self, device): return
    @dbus.service.method("org.bluez.Agent1", in_signature="", out_signature="")
    def Cancel(self): return

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SystemBus()
agent = Agent(bus, "/test/agent")
manager = dbus.Interface(bus.get_object("org.bluez", "/org/bluez"), "org.bluez.AgentManager1")
manager.RegisterAgent("/test/agent", "NoInputNoOutput")
manager.RequestDefaultAgent("/test/agent")
GLib.MainLoop().run()
EOF

sudo chmod +x /usr/local/bin/a2dp-agent

# Create services
sudo tee /etc/systemd/system/a2dp-agent.service > /dev/null <<EOF
[Unit]
Description=Bluetooth A2DP Agent
After=bluetooth.service
[Service]
ExecStart=/usr/local/bin/a2dp-agent
Restart=always
[Install]
WantedBy=multi-user.target
EOF

sudo tee /usr/local/bin/make-discoverable > /dev/null <<'EOF'
#!/bin/bash
sleep 5
bluetoothctl power on
bluetoothctl discoverable on
bluetoothctl pairable on
EOF
sudo chmod +x /usr/local/bin/make-discoverable

sudo tee /etc/systemd/system/bluetooth-discoverable.service > /dev/null <<EOF
[Unit]
Description=Make Bluetooth Discoverable
After=bluetooth.service
[Service]
Type=oneshot
ExecStart=/usr/local/bin/make-discoverable
RemainAfterExit=yes
[Install]
WantedBy=multi-user.target
EOF

# Install serial tools
log_info "Installing serial communication tools..."
sudo apt install -y screen minicom picocom
sudo usermod -a -G dialout $USER

# Create udev rules
sudo tee /etc/udev/rules.d/99-usb-serial.rules > /dev/null << 'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="dialout" 
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2341", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="239a", MODE="0666", GROUP="dialout"
EOF

sudo udevadm control --reload-rules

# Setup LVGL
if [ ! -d "lvgl" ]; then
    log_info "Setting up LVGL library..."
    git clone --recurse-submodules https://github.com/lvgl/lvgl.git
    cd lvgl
    git checkout release/v9.0
    cd ..
fi

# Enable services
sudo systemctl daemon-reload

# Enable Bluetooth services
sudo systemctl enable bluetooth a2dp-agent bluetooth-discoverable

# Enable hardware-specific services
if [ "$AUDIO_HARDWARE" = "BEOCREATE4" ]; then
    sudo systemctl enable sigmatcpserver
fi

# Create hardware-specific helper scripts
log_info "Creating helper scripts..."

# Universal test script
cat > test_audio_${AUDIO_HARDWARE,,}.sh << EOF
#!/bin/bash
echo "=== $AUDIO_HARDWARE Audio Test ==="

EOF

case $AUDIO_HARDWARE in
    "AUX")
        cat >> test_audio_${AUDIO_HARDWARE,,}.sh << 'EOF'
echo "Testing built-in 3.5mm jack audio..."
echo ""

echo "PulseAudio status:"
if pulseaudio --check 2>/dev/null; then
    echo "  ✓ PulseAudio running"
    pactl list sinks short | head -5
else
    echo "  ✗ PulseAudio not running"
    echo "  Starting PulseAudio..."
    pulseaudio --start
fi

echo ""
echo "Testing audio output:"
echo "Playing test tone to built-in audio..."
speaker-test -t sine -f 1000 -l 1 -s 1 2>/dev/null || echo "speaker-test not available"

echo ""
echo "Volume control test:"
CURRENT_VOL=$(pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]*%' | head -1 | tr -d '%')
echo "Current volume: ${CURRENT_VOL}%"
echo "To control volume: pactl set-sink-volume @DEFAULT_SINK@ 50%"
EOF
        ;;
        
    "DAC"|"AMP4")
        cat >> test_audio_${AUDIO_HARDWARE,,}.sh << 'EOF'
echo "Testing HiFiBerry hardware..."
echo ""

echo "ALSA devices:"
aplay -l | grep -i hifiberry || echo "  ✗ HiFiBerry device not found"

echo ""
echo "ALSA mixer controls:"
if amixer | grep -q "Digital"; then
    echo "  ✓ 'Digital' volume control found"
    CURRENT_VOL=$(amixer get Digital | grep -o '[0-9]*%' | head -1)
    echo "  Current volume: $CURRENT_VOL"
else
    echo "  ✗ 'Digital' volume control not found"
fi

echo ""
echo "Testing audio output:"
echo "Playing test tone to HiFiBerry..."
speaker-test -D hw:0,0 -t sine -f 1000 -l 1 -s 1 2>/dev/null || echo "Test failed - check hardware connection"

echo ""
echo "Volume control commands:"
echo "  Set volume: amixer set Digital 80%"
echo "  Get volume: amixer get Digital"
EOF
        ;;
        
    "BEOCREATE4")
        cat >> test_audio_${AUDIO_HARDWARE,,}.sh << 'EOF'
echo "Testing HiFiBerry BeoCreate 4 DSP..."
echo ""

echo "SigmaTCP server status:"
if systemctl is-active sigmatcpserver >/dev/null 2>&1; then
    echo "  ✓ SigmaTCP server running"
else
    echo "  ✗ SigmaTCP server not running"
    echo "  Start with: sudo systemctl start sigmatcpserver"
fi

echo ""
echo "DSP REST API test:"
if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
    echo "  ✓ REST API responding"
    
    # Get DSP info
    CHECKSUM=$(curl -s http://localhost:13141/checksum 2>/dev/null | grep -o '"checksum":"[^"]*"' | cut -d'"' -f4)
    echo "  DSP checksum: $CHECKSUM"
    
    # Check if profile is loaded
    METADATA=$(curl -s http://localhost:13141/metadata 2>/dev/null)
    if echo "$METADATA" | grep -q "profileName"; then
        PROFILE=$(echo "$METADATA" | grep -o '"profileName":"[^"]*"' | cut -d'"' -f4)
        echo "  ✓ Loaded profile: $PROFILE"
    else
        echo "  - No DSP profile loaded"
        echo "    Load profile with: ./setup_dsp_${AUDIO_HARDWARE,,}.sh"
    fi
else
    echo "  ✗ REST API not responding"
    echo "  Check: sudo systemctl status sigmatcpserver"
fi

echo ""
echo "Volume control test:"
echo "Current DSP status available via REST API"
echo "Use dashboard controls or setup_dsp_${AUDIO_HARDWARE,,}.sh"
EOF
        ;;
esac

chmod +x test_audio_${AUDIO_HARDWARE,,}.sh

# Create setup DSP script for BeoCreate 4
if [ "$AUDIO_HARDWARE" = "BEOCREATE4" ]; then
    cat > setup_dsp_${AUDIO_HARDWARE,,}.sh << 'EOF'
#!/bin/bash
echo "=== BeoCreate 4 DSP Profile Setup ==="

# Wait for SigmaTCP server
echo "Waiting for SigmaTCP server..."
for i in {1..10}; do
    if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
        echo "✓ SigmaTCP server ready"
        break
    fi
    sleep 2
done

# Load universal profile
echo "Loading BeoCreate Universal profile..."
PROFILE_PATH="/usr/share/hifiberry/dspprofiles/beocreate-universal-11.xml"

if [ -f "$PROFILE_PATH" ]; then
    curl -s -X POST http://localhost:13141/dspprofile \
      -H "Content-Type: application/json" \
      -d "{\"file\": \"$PROFILE_PATH\"}" && \
    echo "✓ DSP profile loaded successfully" || \
    echo "✗ DSP profile loading failed"
else
    echo "✗ Profile file not found: $PROFILE_PATH"
    echo "Available profiles:"
    ls -la /usr/share/hifiberry/dspprofiles/*.xml 2>/dev/null || echo "No profiles found"
fi

echo ""
echo "Testing DSP functionality..."
curl -s http://localhost:13141/metadata | python3 -m json.tool 2>/dev/null | head -20
EOF
    chmod +x setup_dsp_${AUDIO_HARDWARE,,}.sh
fi

# Bluetooth pairing helper (common)
cat > pair_phone.sh << EOF
#!/bin/bash
echo "=== $DEVICE_NAME Bluetooth Pairing ==="
echo ""
echo "Your Pi is configured as '$DEVICE_NAME' with $AUDIO_HARDWARE audio"
echo ""
echo "On your phone:"
echo "  1. Settings → Bluetooth"
echo "  2. Look for '$DEVICE_NAME'"
echo "  3. Tap to connect (no PIN needed)"
echo "  4. Play music - audio will route through $AUDIO_HARDWARE"
echo ""
echo "Current Bluetooth status:"
hciconfig hci0 2>/dev/null | grep -E "(Name|UP|RUNNING)" || echo "Bluetooth not ready"
echo ""
echo "Connected devices:"
bluetoothctl devices Connected 2>/dev/null || echo "No devices connected"
echo ""

EOF

case $AUDIO_HARDWARE in
    "AUX")
        echo 'echo "Audio will play through built-in 3.5mm jack"' >> pair_phone.sh
        ;;
    "DAC")
        echo 'echo "Connect external amplifier to HiFiBerry DAC+ output"' >> pair_phone.sh
        ;;
    "AMP4")
        echo 'echo "Connect speakers directly to HiFiBerry AMP4 terminals"' >> pair_phone.sh
        ;;
    "BEOCREATE4")
        cat >> pair_phone.sh << 'EOF'
echo "DSP status:"
curl -s http://localhost:13141/checksum >/dev/null && echo "✓ BeoCreate 4 DSP ready" || echo "✗ DSP not ready"
EOF
        ;;
esac

chmod +x pair_phone.sh

# Serial test script (common)
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

# Clean script
cat > clean.sh << 'EOF'
#!/bin/bash
echo "Cleaning build artifacts..."
rm -rf build/
rm -f run_*.sh
rm -f LVGLDashboard_*
echo "Clean complete!"
EOF
chmod +x clean.sh

# Make Bluetooth discoverable now
/usr/local/bin/make-discoverable &

log_success "=== Setup Complete! ==="
log_info ""
log_info "✓ $AUDIO_HARDWARE audio hardware configured"
log_info "✓ Bluetooth A2DP configured (TazzariAudio)"
log_info "✓ LVGL library ready (v9.0)"
log_info "✓ Serial communication ready"
log_info "✓ Helper scripts created"
log_info ""

case $AUDIO_HARDWARE in
    "AUX")
        log_info "Next steps for built-in audio:"
        log_info "  ./build.sh --audio-aux       # Build for built-in audio"
        ;;
    "DAC")
        log_info "Next steps for HiFiBerry DAC+:"
        log_info "  ./build.sh --audio-dac       # Build for DAC+"
        log_info "  Connect external amplifier to DAC+ output"
        ;;
    "AMP4")
        log_info "Next steps for HiFiBerry AMP4:"
        log_info "  ./build.sh --audio-amp4      # Build for AMP4"
        log_info "  Connect speakers to AMP4 terminals"
        ;;
    "BEOCREATE4")
        log_info "Next steps for BeoCreate 4:"
        log_info "  sudo reboot                  # Required for DSP hardware"
        log_info "  ./setup_dsp_beocreate4.sh    # Load DSP profile (after reboot)"
        log_info "  ./build.sh --audio-beocreate4 # Build for BeoCreate 4"
        ;;
esac

log_info ""
log_info "Common next steps:"
log_info "  ./test_audio_${AUDIO_HARDWARE,,}.sh     # Test audio system"
log_info "  ./pair_phone.sh              # Pair Bluetooth device"
log_info "  ./test_serial.sh             # Test ESP32 connection"
log_info ""

if [ "$AUDIO_HARDWARE" = "BEOCREATE4" ]; then
    log_warning "CRITICAL: REBOOT REQUIRED for BeoCreate 4 DSP hardware!"
    log_warning "REBOOT NOW: sudo reboot"
else
    log_info "Hardware configuration complete. You can start building now!"
    log_info "Or reboot to ensure all settings take effect: sudo reboot"
fi