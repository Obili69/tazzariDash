#!/bin/bash
# setup.sh - Complete LVGL Dashboard setup with HiFiBerry BeoCreate 4 + Auto-start + Splash

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

DEVICE_NAME="TazzariAudio"
DSP_PROFILE="/usr/share/hifiberry/dspprofiles/beocreate-universal-11.xml"
DASHBOARD_PATH="$(pwd)"

log_info() { echo -e "${BLUE}[SETUP]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

if [[ $EUID -eq 0 ]]; then
   echo "Error: Don't run as root. Use regular user with sudo access."
   exit 1
fi

# Check for splash image
if [ ! -f "img/splash.png" ]; then
    echo "Error: img/splash.png not found!"
    echo "Please add your splash image to img/splash.png"
    exit 1
fi

log_info "=== Complete TazzariAudio Dashboard Setup ==="
log_info "This installs:"
log_info "  • HiFiBerry BeoCreate 4 DSP with REST API"
log_info "  • Bluetooth A2DP audio sink (TazzariAudio)"
log_info "  • Custom boot splash screen with your image"
log_info "  • Auto-start dashboard with hidden cursor"
log_info "  • Serial communication + LVGL library"

read -p "Install everything? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_info "Setup cancelled"
    exit 0
fi

# Update system
log_info "Updating system packages..."
sudo apt update

# Install build tools
log_info "Installing build tools..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    curl \
    libcurl4-openssl-dev

# Install LVGL dependencies  
log_info "Installing LVGL dependencies..."
sudo apt install -y \
    libsdl2-dev \
    libsdl2-image-dev

# Install HiFiBerry + Audio + Bluetooth
log_info "Installing HiFiBerry BeoCreate 4 + Bluetooth..."

curl -Ls https://tinyurl.com/hbosrepo | bash
sudo apt update -qq
sudo apt install -y \
    hifiberry-dsp \
    hifiberry-dspprofiles \
    bluetooth \
    bluez \
    bluez-tools \
    pulseaudio \
    pulseaudio-module-bluetooth \
    python3-dbus \
    python3-gi \
    alsa-utils \
    plymouth \
    plymouth-themes

# Configure boot settings for BeoCreate 4
log_info "Configuring boot settings..."

sudo sed -i '/^dtparam=i2c_arm=/d' /boot/firmware/config.txt
sudo sed -i '/^dtparam=i2s=/d' /boot/firmware/config.txt  
sudo sed -i '/^dtparam=spi=/d' /boot/firmware/config.txt
sudo sed -i '/^dtparam=audio=/d' /boot/firmware/config.txt
sudo sed -i '/^dtoverlay=hifiberry-dac/d' /boot/firmware/config.txt

sudo tee -a /boot/firmware/config.txt > /dev/null <<EOF

# TazzariAudio - HiFiBerry BeoCreate 4
dtparam=i2c_arm=on
dtparam=i2s=on
dtparam=spi=on
dtparam=audio=off
dtoverlay=hifiberry-dac
EOF

# Set hostname
log_info "Setting hostname to $DEVICE_NAME..."
sudo hostnamectl set-hostname "$DEVICE_NAME"
sudo sed -i "s/raspberrypi/$DEVICE_NAME/g" /etc/hosts

# Configure Bluetooth
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

# Create custom boot splash with your image
log_info "Creating boot splash with your splash.png..."

sudo mkdir -p /usr/share/plymouth/themes/tazzari
sudo cp img/splash.png /usr/share/plymouth/themes/tazzari/logo.png

sudo tee /usr/share/plymouth/themes/tazzari/tazzari.plymouth > /dev/null <<'EOF'
[Plymouth Theme]
Name=TazzariAudio
Description=TazzariAudio Electric Vehicle Dashboard
ModuleName=script

[script]
ImageDir=/usr/share/plymouth/themes/tazzari
ScriptFile=/usr/share/plymouth/themes/tazzari/tazzari.script
EOF

sudo tee /usr/share/plymouth/themes/tazzari/tazzari.script > /dev/null <<'EOF'
# TazzariAudio boot splash

Window.SetBackgroundTopColor(0.05, 0.05, 0.05);
Window.SetBackgroundBottomColor(0.1, 0.1, 0.1);

logo_image = Image("logo.png");
logo_sprite = Sprite(logo_image);
logo_sprite.SetX(Window.GetWidth() / 2 - logo_image.GetWidth() / 2);
logo_sprite.SetY(Window.GetHeight() / 2 - logo_image.GetHeight() / 2);

progress = 0;
dots_sprite = Sprite();

fun refresh_callback() {
    progress++;
    if (progress > 120) progress = 0;
    
    dots = "";
    dot_count = Math.Int(progress / 30) % 4;
    for (i = 0; i <= dot_count; i++) {
        dots += "●";
    }
    
    loading_text = "Starting" + dots;
    loading_image = Image.Text(loading_text, 0.8, 0.8, 0.8, 1, "Ubuntu 16");
    dots_sprite.SetImage(loading_image);
    dots_sprite.SetX(Window.GetWidth() / 2 - loading_image.GetWidth() / 2);
    dots_sprite.SetY(Window.GetHeight() / 2 + logo_image.GetHeight() / 2 + 30);
}

Plymouth.SetRefreshFunction(refresh_callback);
Plymouth.SetMessageFunction(fun(text) {});
EOF

sudo plymouth-set-default-theme tazzari
sudo update-initramfs -u

if ! grep -q "splash" /boot/firmware/cmdline.txt; then
    sudo sed -i 's/$/ splash quiet/' /boot/firmware/cmdline.txt
fi

# Hide cursor using simple rename method
log_info "Hiding mouse cursor..."
if [ -f "/usr/share/icons/PiXflat/cursors/left_ptr" ] && [ ! -f "/usr/share/icons/PiXflat/cursors/left_ptr.bak" ]; then
    sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak
    log_success "Cursor hidden"
fi

# Setup dashboard auto-start
log_info "Setting up dashboard auto-start..."

cat > ~/start_tazzari_dashboard.sh << EOF
#!/bin/bash
sleep 15
cd $DASHBOARD_PATH

for i in {1..20}; do
    if curl -s http://localhost:13141/checksum >/dev/null 2>&1; then
        break
    fi
    sleep 3
done

while true; do
    ./build/LVGLDashboard_deployment
    sleep 3
done
EOF

chmod +x ~/start_tazzari_dashboard.sh

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

# Enable services
sudo systemctl daemon-reload
sudo systemctl enable bluetooth a2dp-agent sigmatcpserver bluetooth-discoverable

# Make discoverable now
/usr/local/bin/make-discoverable &

# Install serial tools
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

# DSP setup script (to run after reboot)
cat > setup_dsp.sh << 'EOF'
#!/bin/bash
echo "=== Setting up BeoCreate 4 DSP Profile ==="
echo "Loading universal profile..."

# Wait for SigmaTCP server to be ready
sleep 5

# Load the DSP profile via REST API
curl -s -X POST http://localhost:13141/dspprofile \
  -H "Content-Type: application/json" \
  -d '{"file": "/usr/share/hifiberry/dspprofiles/beocreate-universal-11.xml"}' && \
echo "✓ DSP profile loaded successfully" || \
echo "✗ DSP profile loading failed"

echo ""
echo "Testing DSP REST API..."
curl -s http://localhost:13141/metadata | python3 -m json.tool 2>/dev/null && \
echo "✓ DSP REST API working" || \
echo "✗ DSP REST API not responding"
EOF
chmod +x setup_dsp.sh

# Bluetooth pairing helper
cat > pair_phone.sh << 'EOF'
#!/bin/bash
echo "=== TazzariAudio Bluetooth Pairing ==="
echo ""
echo "Your Pi is now 'TazzariAudio' - HiFiBerry BeoCreate 4 DSP"
echo ""
echo "On your phone:"
echo "  1. Settings → Bluetooth"
echo "  2. Look for 'TazzariAudio'"
echo "  3. Tap to connect (no PIN needed)"
echo "  4. Play music - should route through BeoCreate 4 DSP"
echo ""
echo "Current Bluetooth status:"
hciconfig hci0 2>/dev/null | grep -E "(Name|UP|RUNNING)" || echo "Bluetooth not ready"
echo ""
echo "Connected devices:"
bluetoothctl devices Connected 2>/dev/null || echo "No devices connected"
echo ""
echo "DSP REST API status:"
curl -s http://localhost:13141/checksum >/dev/null && echo "✓ DSP API responding" || echo "✗ DSP API not ready"
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

# Media controls test script
cat > test_media_controls.sh << 'EOF'
#!/bin/bash
echo "=== Testing Bluetooth Media Controls ==="

CONNECTED=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
if [ "$CONNECTED" -eq 0 ]; then
    echo "❌ No Bluetooth devices connected"
    echo "Connect your phone first with ./pair_phone.sh"
    exit 1
fi

DEVICE=$(bluetoothctl devices Connected 2>/dev/null | head -1 | cut -d' ' -f3-)
echo "✅ Connected to: $DEVICE"
echo ""

echo "Getting current media info..."
echo "info" | bluetoothctl | grep -E "(Title|Artist|Album|Status):" || echo "No media info available"
echo ""

echo "Testing media controls:"
echo "1. Play command..."
echo "player.play" | bluetoothctl >/dev/null 2>&1
sleep 2

echo "2. Getting status after play..."
echo "info" | bluetoothctl | grep "Status:" || echo "Status not available"
echo ""

echo "3. Pause command..." 
echo "player.pause" | bluetoothctl >/dev/null 2>&1
sleep 2

echo "4. Getting status after pause..."
echo "info" | bluetoothctl | grep "Status:" || echo "Status not available"
echo ""

echo "✅ Media control test complete"
EOF
chmod +x test_media_controls.sh

log_success "=== Initial Setup Complete! ==="
log_info ""
log_info "✓ Dependencies installed"
log_info "✓ HiFiBerry BeoCreate 4 configured"
log_info "✓ Bluetooth A2DP configured (TazzariAudio)"
log_info "✓ LVGL library ready (v9.0)"
log_info "✓ Serial communication ready"
log_info "✓ Helper scripts created"
log_info ""
log_warning "CRITICAL: REBOOT REQUIRED for DSP hardware to initialize!"
log_info ""
log_info "After reboot:"
log_info "  ./setup_dsp.sh          # Load DSP profile (run once)"
log_info "  ./build.sh              # Build dashboard"
log_info "  ./pair_phone.sh         # Pair your phone"
log_info "  ./test_serial.sh        # Test ESP32 connection"
log_info ""
log_info "The Pi will appear as 'TazzariAudio' with BeoCreate 4 DSP control"
log_warning "REBOOT NOW: sudo reboot"