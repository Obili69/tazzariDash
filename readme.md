# LVGL Tazzari Dashboard with HiFiBerry BeoCreate 4

Complete automotive dashboard with professional DSP audio, custom boot splash, and auto-start for electric vehicles.

## Features
- **Vehicle Data**: Speed, battery, gear, lighting via ESP32 serial
- **HiFiBerry BeoCreate 4**: Professional 4-channel DSP with REST API control
- **Hardware Volume**: 0-100% direct DSP register control (not software)  
- **3-Band EQ**: Bass/Mid/High biquad filters via DSP
- **Bluetooth Audio**: Phone streams to dashboard → BeoCreate 4 → car speakers
- **Custom Boot Splash**: Your splash.png shown during Pi boot
- **Auto-start**: Boots directly to fullscreen dashboard (no desktop visible)
- **Hidden Cursor**: Professional automotive interface (no mouse pointer)
- **Real-time Charts**: Voltage/current monitoring with touch controls

## Quick Start

```bash
# 1. Add your splash image
mkdir -p img
# Copy your splash.png to img/splash.png

# 2. One-time complete setup
chmod +x setup.sh
./setup.sh
sudo reboot

# 3. After reboot - Load DSP profile  
./setup_dsp.sh

# 4. Build and run
./build.sh --deployment
```

**That's it!** The dashboard will auto-start on every boot with your custom splash screen.

## Complete Boot Sequence

```
Pi Boot → Custom Splash (your image) → Desktop (hidden) → Dashboard (fullscreen, no cursor)
```

1. **Boot Splash**: Shows your `img/splash.png` with animated loading dots
2. **Cursor Hidden**: System-wide cursor removal (elegant file rename method)
3. **Auto-start**: Dashboard launches automatically in fullscreen
4. **Audio Ready**: Appears as "TazzariAudio" for Bluetooth pairing
5. **DSP Control**: Hardware volume and EQ via BeoCreate 4

## Control Scripts

```bash
./stop_dashboard.sh     # Stop dashboard (show desktop)
./show_cursor.sh        # Restore cursor for debugging
./disable_autostart.sh  # Disable auto-start completely
./pair_phone.sh         # Bluetooth pairing help
./test_serial.sh        # Check ESP32 connection
```

## Hardware Requirements

- **Platform**: Raspberry Pi 4/5 
- **Audio**: HiFiBerry BeoCreate 4 (4-channel amplifier + DSP)
- **Display**: 1024x600 touchscreen (automotive recommended)
- **Vehicle Data**: ESP32 via USB serial
- **Speakers**: 4-channel car audio system
- **Power**: 12V to 5V converter with clean power

## Audio System Architecture

```
Phone (Bluetooth) → Pi (TazzariAudio) → BeoCreate 4 DSP → 4 Car Speakers
                                        ↓
Dashboard Touch Controls ←→ REST API ←→ Hardware DSP Registers
(Volume/EQ/Media)          (localhost:13141)  (Real-time Control)
```

## Development vs Production

```bash
# Development (windowed, cursor visible)
./build.sh
./build/LVGLDashboard_dev

# Production (fullscreen, auto-start, no cursor)  
./build.sh --deployment
sudo reboot  # Auto-starts
```

## Troubleshooting

```bash
# Check system status
curl http://localhost:13141/checksum  # DSP status
sudo systemctl status sigmatcpserver  # DSP service
sudo systemctl status a2dp-agent      # Bluetooth agent
bluetoothctl devices Connected        # Connected devices

# Audio issues
aplay -l                              # Check audio hardware
pactl list sinks short               # Check audio routing

# Serial issues  
./test_serial.sh                      # ESP32 connection
ls /dev/tty{USB,ACM}*                # Available ports

# Dashboard issues
./stop_dashboard.sh                   # Stop if hung
./show_cursor.sh                      # Show cursor for debugging
sudo reboot                           # Full restart
```

**Common Issues:**
- **No audio**: Check HiFiBerry hardware connection, run `./setup_dsp.sh`
- **No serial**: Logout/login after setup for dialout group permissions
- **Dashboard won't start**: Check `./build.sh --deployment` completed successfully
- **No Bluetooth**: Pi should appear as "TazzariAudio" in phone settings
- **Cursor visible**: Run `./show_cursor.sh` then restart desktop

## Project Structure
```
tazzariDash/
├── img/
│   └── splash.png              # Your custom splash image
├── src/                        # C++ source files
│   ├── main.cpp               # Main dashboard application
│   ├── SerialCommunication.cpp
│   └── SimplifiedAudioManager.cpp  # HiFiBerry REST API
├── include/                   # Header files  
├── ui/                       # LVGL UI files
├── lvgl/                     # LVGL library (v9.0)
├── build/                    # Build output
├── setup.sh                  # Complete one-time setup
├── build.sh                  # Build script
├── setup_dsp.sh              # DSP profile loader
└── *.sh                      # Control scripts
```

## Technical Details

### Custom Boot Experience
- **Plymouth Splash**: Your `splash.png` displayed during boot with loading animation
- **Seamless Transition**: Boot splash → hidden desktop → fullscreen dashboard
- **Professional Look**: No visible Linux desktop or cursor

### Audio Control Architecture
- **Hardware Volume**: Direct BeoCreate 4 DSP register writes (0-100%)
- **EQ Filters**: Real-time biquad filter calculation and DSP upload
- **Media Control**: Bluetooth AVRCP commands with internal state tracking
- **Auto-pairing**: Python D-Bus agent for seamless phone connection

### Cursor Hiding Method
- **Simple & Reliable**: Renames cursor file (`left_ptr` → `left_ptr.bak`)
- **Universal**: Works on X11, Wayland, any display server
- **Easy Recovery**: Simple file rename to restore functionality

### Dashboard Integration
- **REST API**: Full HiFiBerry DSP control via libcurl HTTP calls
- **State Management**: Internal tracking for reliable media play/pause
- **Data Persistence**: Vehicle odometer and settings auto-save
- **Real-time Updates**: 100ms refresh rate for smooth vehicle data display

## Development Workflow

```bash
# Development mode (windowed, cursor visible)
./build.sh
./show_cursor.sh              # If cursor was hidden
./build/LVGLDashboard_dev

# Production mode (fullscreen, auto-start)
./build.sh --deployment
sudo mv /usr/share/icons/PiXflat/cursors/left_ptr /usr/share/icons/PiXflat/cursors/left_ptr.bak
sudo reboot
```

## Customization

### Splash Screen
- Replace `img/splash.png` with your custom image
- Modify Plymouth script in `/usr/share/plymouth/themes/tazzari/tazzari.script`
- Adjust colors, animations, or text

### Audio Settings
- EQ band frequencies: Modify `SimplifiedAudioManager.cpp` bass/mid/high frequencies
- Volume curve: Adjust DSP value calculation in `setDSPVolume()`
- Additional DSP features: Use HiFiBerry REST API documentation

### Vehicle Data
- Serial protocol: Modify packet structures in `SerialCommunication.h`
- New sensors: Extend data structures for additional vehicle systems
- Display layout: Customize LVGL UI elements

Built with LVGL, SDL2, HiFiBerry BeoCreate 4, and precision engineering for electric vehicles!# LVGL Tazzari Dashboard with HiFiBerry BeoCreate 4

Modern automotive dashboard with Bluetooth audio and professional DSP for electric vehicles.

## Features
- **Vehicle Data**: Speed, battery, gear, lighting via ESP32 serial
- **HiFiBerry BeoCreate 4**: Professional 4-channel DSP with REST API control
- **Hardware Volume Control**: 0-100% via DSP registers (not software)  
- **3-Band EQ**: Bass/Mid/High frequency control via DSP biquad filters
- **Bluetooth Audio**: Phone streams music through dashboard to BeoCreate 4 amplifier
- **Real-time Charts**: Voltage/current monitoring
- **Touch Controls**: Volume, EQ, media control, trip reset

## Hardware Requirements
- **Platform**: Raspberry Pi 4/5 
- **Audio DSP**: HiFiBerry BeoCreate 4 (4-channel amplifier + DSP)
- **Display**: 1024x600 touchscreen
- **Vehicle Data**: ESP32 via USB serial
- **Speakers**: 4-channel car audio system

## Quick Start

```bash
# 1. One-time setup (installs HiFiBerry + Bluetooth)
chmod +x setup.sh
./setup.sh
sudo reboot

# 2. After reboot - Load DSP profile
./setup_dsp.sh

# 3. Build dashboard  
./build.sh

# 4. Connect phone to Bluetooth
./pair_phone.sh

# 5. Run dashboard
./run_dev.sh              # Development (windowed)
./run_deployment.sh       # Production (fullscreen)
```

## Audio System Architecture

```
Phone (Bluetooth A2DP) → Pi (TazzariAudio) → HiFiBerry BeoCreate 4 → 4 Car Speakers
                                              ↓
Dashboard Controls ←→ REST API ←→ SigmaTCP Server ←→ DSP Registers
(Volume/EQ)          (localhost:13141)                (Hardware Control)
```

## Usage
- **Volume Control**: Arc control directly adjusts DSP hardware volume (0-100%)
- **EQ Controls**: Bass/Mid/High sliders create biquad filters on DSP
- **Media Controls**: Play/pause/skip buttons via Bluetooth
- **Trip Reset**: Tap trip distance
- **Bluetooth**: Pi appears as "TazzariAudio" on phones

## Development

```bash
./build.sh                # Debug build (windowed)
./build.sh --deployment   # Release build (fullscreen)
./clean.sh                # Clean build files
```

## Troubleshooting

```bash
./test_serial.sh          # Check ESP32 connection
./setup_dsp.sh            # Reload DSP profile
./pair_phone.sh           # Bluetooth pairing help

# Check DSP status
curl http://localhost:13141/checksum

# Check services
sudo systemctl status sigmatcpserver
sudo systemctl status a2dp-agent
```

**Common Issues:**
- **No serial access**: Logout/login after setup
- **No DSP control**: Run `./setup_dsp.sh` after reboot
- **No Bluetooth audio**: Check Pi appears as "TazzariAudio" 
- **Build errors**: Install libcurl with `sudo apt install libcurl4-openssl-dev`

## Project Structure
```
tazzariDash/
├── src/                    # C++ source files
│   ├── main.cpp           # Main dashboard application
│   ├── SerialCommunication.cpp
│   └── SimplifiedAudioManager.cpp  # HiFiBerry REST API integration
├── include/               # Header files  
├── ui/                   # LVGL UI files
├── build/                # Build output (generated)
├── setup.sh              # One-time HiFiBerry + Bluetooth setup
├── build.sh              # Build script
├── setup_dsp.sh          # DSP profile loader (generated)
└── *.sh                  # Helper scripts (generated)
```

## Technical Details

### Audio Control
- **Hardware Volume**: Direct DSP register control (not software mixing)
- **REST API**: Full HiFiBerry DSP REST API integration via libcurl
- **EQ Filters**: Real-time biquad filter generation and DSP upload
- **Automatic Storage**: Volume/EQ settings saved in DSP profile

### DSP Integration
- **Profile**: BeoCreate Universal (4-channel crossover)
- **Control Protocol**: SigmaTCP → REST API → libcurl
- **Volume Register**: `volumeControlRegister` from profile metadata
- **EQ Bands**: PeakingEq filters at 100Hz/1kHz/10kHz

### Bluetooth Setup
- **Device Name**: "TazzariAudio" 
- **Class**: 0x2C0414 (Audio device)
- **Auto-pairing**: Python D-Bus agent with NoInputNoOutput
- **A2DP Sink**: Receives audio FROM phones

Built with LVGL, SDL2, HiFiBerry BeoCreate 4, and love for electric vehicles! 🚗⚡