# LVGL Tazzari Dashboard with HiFiBerry BeoCreate 4

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