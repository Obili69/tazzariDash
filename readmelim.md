# LVGL Tazzari Dashboard

Modern automotive dashboard with Bluetooth audio for electric vehicles.

## Features
- **Vehicle Data**: Speed, battery, gear, lighting via ESP32 serial
- **Bluetooth Audio**: Phone streams music through dashboard to car speakers  
- **Real-time Charts**: Voltage/current monitoring
- **Touch Controls**: Volume, media control, trip reset

## Quick Start

```bash
# 1. One-time setup (installs everything)
chmod +x setup.sh
./setup.sh
# Logout/login for permissions

# 2. Build dashboard  
./build.sh

# 3. Connect phone to Bluetooth
./pair_phone.sh

# 4. Run dashboard
./run_dev.sh              # Development (windowed)
./run_deployment.sh       # Production (fullscreen)
```

## Hardware
- **Platform**: Raspberry Pi 4/5 or Linux PC
- **Display**: 1024x600 touchscreen
- **Vehicle Data**: ESP32 via USB serial 
- **Audio**: Built-in 3.5mm jack to car speakers

## Usage
- **Trip Reset**: Tap trip distance
- **Volume**: Use arc control 
- **Media**: Play/pause/skip buttons
- **Bluetooth**: Pi appears as "TazzariAudio" on phones

## Development

```bash
./build.sh                # Debug build (windowed)
./build.sh --deployment   # Release build (fullscreen)
./build.sh --no-bluetooth # Disable Bluetooth features
./clean.sh                # Clean build files
```

## Troubleshooting

```bash
./test_serial.sh          # Check ESP32 connection
./route_audio.sh          # Fix audio routing
./pair_phone.sh           # Bluetooth pairing help
```

**Common Issues:**
- **No serial access**: Logout/login after setup
- **No Bluetooth audio**: Run `./route_audio.sh`
- **Build errors**: Check all source files in `src/` and `ui/`

## Project Structure
```
tazzariDash/
├── src/           # C++ source files
├── include/       # Header files  
├── ui/            # LVGL UI files
├── build/         # Build output (generated)
├── setup.sh       # One-time setup
├── build.sh       # Build script
└── *.sh           # Helper scripts (generated)
```

Built with LVGL, SDL2, and love for electric vehicles! 🚗⚡