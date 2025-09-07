# TazzariAudio Dashboard

Modern automotive dashboard for electric vehicles with **4 audio hardware options**. Works with ESP32 for vehicle data and Bluetooth for phone audio streaming.

![Dashboard Preview](https://img.shields.io/badge/Platform-Raspberry%20Pi-red) ![Audio](https://img.shields.io/badge/Audio-4%20Hardware%20Options-blue) ![Display](https://img.shields.io/badge/Display-LVGL%20Touch-green)

## 🎵 Audio Hardware Support

| Hardware | Description | Volume | EQ | Use Case |
|----------|-------------|---------|-----|----------|
| **Built-in (AUX)** | Pi's 3.5mm jack | Software | Software | Development, basic setup |
| **HiFiBerry DAC+** | High-quality DAC | Hardware | Software | External amplifier |
| **HiFiBerry AMP4** | DAC + amplifier | Hardware | Software | Direct speakers |
| **BeoCreate 4** | 4-channel DSP | Hardware | Hardware | Professional install |

## 🚀 Quick Start

### 1. Setup (One Time)
```bash
# Download and setup
git clone https://github.com/Obili69/tazzariDash.git
cd tazzariDash
chmod +x *.sh
# Auto-setup with hardware selection
./setup.sh --audio-amp4        # or --audio-aux, --audio-dac, --audio-beocreate4
sudo reboot                     # Required for hardware changes
```

### 2. Build & Deploy
```bash
# Development build (windowed)
./build.sh --dev --audio-amp4

# Production build (fullscreen + autostart)
./build.sh --deployment --audio-amp4

# Complete deployment setup
./pi5_autostart_setup.sh AMP4
```

### 3. Run
```bash
# Manual start
export DISPLAY=:0
./build/LVGLDashboard_AMP4_deployment

# Or auto-start on boot
sudo reboot
```

## 🔧 Hardware Setup

### Built-in Audio (AUX)
- **Hardware**: None required
- **Connection**: 3.5mm jack → external amplifier
- **Setup**: `./setup.sh --audio-aux`

### HiFiBerry DAC+ 
- **Hardware**: HiFiBerry DAC+ hat
- **Connection**: DAC+ line out → amplifier → speakers
- **Setup**: `./setup.sh --audio-dac`
- **Boot config**: `dtoverlay=hifiberry-dacplus-std`

### HiFiBerry AMP4
- **Hardware**: HiFiBerry AMP4 hat  
- **Connection**: Speakers directly to AMP4 terminals
- **Setup**: `./setup.sh --audio-amp4`
- **Boot config**: `dtoverlay=hifiberry-dacplus-std`

### HiFiBerry BeoCreate 4
- **Hardware**: HiFiBerry BeoCreate 4 DSP hat
- **Connection**: 4 speakers (front L/R, rear L/R)
- **Setup**: `./setup.sh --audio-beocreate4`
- **Boot config**: `dtoverlay=hifiberry-dac` + DSP tools

## 📱 Vehicle Integration

### ESP32 Connection
- **Port**: `/dev/ttyACM0` (USB)
- **Baud**: 115200
- **Data**: Speed, battery, lights, gear

### Bluetooth Audio
- **Device Name**: `TazzariAudio`
- **Profile**: A2DP (music streaming)
- **Pairing**: `./pair_phone.sh`

## 🛠️ Management

```bash
# Check status
./dashboard_status.sh

# Stop dashboard  
./stop_dashboard.sh

# Test audio hardware
./test_audio_amp4.sh

# Show cursor (debug mode)
./show_cursor.sh

# Disable autostart
./disable_autostart.sh
```

## 🐛 Troubleshooting

### "Segmentation fault"
**Cause**: Display environment (common on Pi 5)
```bash
# Fix: Set display and run from desktop
export DISPLAY=:0
./build/LVGLDashboard_AUX_deployment
```

### "HiFiBerry device not detected"
```bash
# Check boot config
sudo nano /boot/firmware/config.txt

# Add for DAC+/AMP4:
dtparam=audio=off
dtoverlay=hifiberry-dacplus-std

# Reboot
sudo reboot
```

### "No audio output"
```bash
# Test your hardware
./test_audio_[hardware].sh

# Check connections and volume levels
```

### "Dashboard won't autostart"
```bash
# Check autostart status
./dashboard_status.sh

# Re-setup autostart
./pi5_autostart_setup.sh [HARDWARE]
```

## 📊 Dashboard Features

- **Real-time vehicle data** (speed, battery, range)
- **Universal audio controls** (volume, EQ, media)
- **Bluetooth music streaming** from phone
- **Battery monitoring** with voltage/temp graphs
- **Lighting status** (headlights, indicators, etc.)
- **Touch controls** for all functions

## 💡 Development Tips

```bash
# Development workflow
./build.sh --dev --audio-aux          # Build for testing
./run_aux_dev.sh                      # Run in window

# Switch hardware easily  
./build.sh --dev --audio-amp4         # Now build for AMP4
./run_amp4_dev.sh                     # Test AMP4 config

# Production deployment
./build.sh --deployment --audio-amp4  # Fullscreen build
./pi5_autostart_setup.sh AMP4         # Setup autostart
sudo reboot                           # Test autostart
```

## 🔧 Hardware Compatibility

| Pi Model | Built-in | DAC+ | AMP4 | BeoCreate 4 |
|----------|----------|------|------|-------------|
| **Pi 4** | ✅ | ✅ | ✅ | ✅ |
| **Pi 5** | ✅ | ✅ | ✅ | ✅ |
| **Pi 3B+** | ✅ | ✅ | ✅ | ⚠️ (slower) |

## 📁 Project Structure

```
tazzariDash/
├── src/                    # C++ source code
├── include/                # Headers  
├── ui/                     # LVGL UI files
├── build/                  # Compiled executables
├── setup.sh               # Hardware setup
├── build.sh               # Build system
├── pi5_autostart_setup.sh  # Autostart configuration
└── test_audio_*.sh        # Hardware test scripts
```

## 🎯 Quick Commands Reference

```bash
# Setup & Build
./setup.sh --audio-amp4 && sudo reboot
./build.sh --deployment --audio-amp4

# Autostart
./pi5_autostart_setup.sh AMP4
sudo reboot

# Control
./dashboard_status.sh      # Status
./stop_dashboard.sh        # Stop  
./show_cursor.sh          # Debug
```

---

**🚗 Ready for your electric vehicle!** Choose your audio hardware, run the setup, and deploy to your Pi dashboard.
