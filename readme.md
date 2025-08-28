# LVGL Automotive Dashboard

A real-time automotive dashboard built with LVGL, designed for electric vehicles with comprehensive BMS integration and audio system control.

## Overview

This project implements a modern automotive dashboard interface that receives data from ESP32-based vehicle controllers via USB serial communication. The dashboard displays critical vehicle information including speed, battery management, lighting states, and provides audio system control through HiFiBerry DSP integration.

## ToDo
- **Audio**: Add bluetooth handeling and beocreate specific audio handeling.
- **Setup**: Add Windowless mode for deployment builds and use windowed build only in development
- **Project**: Add config files and build flags to adjust project. (eg dev build and factory build, beocreate specific code or not) Also a cleaner main would be nice.
- **Documentation**: Add custom UI building instructions.
## Features

### Vehicle Data Display
- **Speed & Distance**: Real-time speed display with integrated odometer and trip counter
- **Gear Indication**: Visual gear state (D/N/R) with opacity-based highlighting
- **Battery Management**: SOC percentage, voltage ranges, temperature monitoring
- **Lighting System**: Complex hierarchy supporting DRL, low/high beam, fog lights, indicators

### Audio Integration
- **Volume Control**: Interactive arc-based volume adjustment
- **EQ Controls**: 3-band equalizer (Bass/Mid/High) with real-time adjustment
- **Media Controls**: Play/pause/skip functionality for Bluetooth audio
- **DSP Integration**: HiFiBerry BeoCreate DSP control via SigmaTCP

### Data Visualization
- **Real-time Charts**: Voltage and current plotting with dual series
- **Battery Warnings**: ThunderSky Winston LiFePO4 specific safety alerts
- **Icon Management**: Contextual lighting and warning icon display

### System Features
- **Data Persistence**: Automatic odometer/trip data saving
- **Startup Sequence**: 2-second icon test followed by normal operation
- **Timeout Handling**: Graceful degradation when data sources disconnect
- **Debug Logging**: Comprehensive event and data reception logging

## Hardware Requirements

### Minimum Requirements
- **Platform**: Raspberry Pi 4/5 or Linux PC with USB port
- **Display**: 1024x600 resolution (7" automotive display recommended)
- **Memory**: 2GB RAM minimum, 4GB recommended
- **Storage**: 8GB microSD/SSD minimum

### Recommended Setup
- **SBC**: Raspberry Pi 5 with active cooling
- **Display**: 7" IPS touchscreen with automotive-grade housing
- **Audio**: HiFiBerry DAC+ ADC Pro with BeoCreate 4-channel amplifier
- **Connectivity**: ESP32 via USB-C for vehicle data
- **Power**: 12V to 5V buck converter with clean power filtering

### Supported Hardware
- **Vehicle Controllers**: ESP32, Arduino with compatible serial protocol
- **Audio DSPs**: HiFiBerry BeoCreate series
- **Displays**: Any SDL2-compatible display or development window

## Installation

### Quick Install
```bash
# Clone repository
git clone https://github.com/Obili69/tazzariDash.git
cd lvgl-dashboard

# Run installation script
chmod +x install_dashboard_deps.sh
./install_dashboard_deps.sh

# Logout/login to apply group changes
# Copy your source files to src/ and ui/ directories

# Build project
./build.sh

# Test serial connection
./test_serial.sh
```

### Manual Installation
See [INSTALLATION_SUMMARY.md](INSTALLATION_SUMMARY.md) for detailed dependency information.

## Configuration

### Serial Communication
```cpp
// Default configuration in main.cpp
const char* serial_port = "/dev/ttyUSB0";  // Change as needed
const int baud_rate = 115200;
```

Common ports:
- ESP32: `/dev/ttyUSB0`
- Arduino Uno: `/dev/ttyUSB0` or `/dev/ttyACM0`

### Display Settings
```cpp
// Window size configuration
lv_display_t* disp = lv_sdl_window_create(1024, 600);
```

### BeoCreate DSP
```bash
# Activate Python environment
source activate_beocreate.sh

# Configure DSP (example)
beocreate-dsp configure --program automotive.xml
```

## Usage

### Basic Operation
1. Connect ESP32 via USB cable
2. Power on the display system
3. Run the dashboard: `./build/LVGLDashboard`
4. Dashboard shows 2-second startup test, then live data

### Controls
- **Trip Reset**: Click on trip distance value
- **Volume**: Use arc control (0-100%)
- **EQ**: Adjust bass/mid/high frequency sliders
- **Media**: Play/pause/skip buttons for Bluetooth audio

### Data Sources
The dashboard expects data from ESP32 in specific packet format:
- **Automotive Packet**: Speed, gear, lighting states
- **BMS Packet**: Voltage, current, SOC, temperatures

## Architecture

### Core Components
- **Dashboard Class**: Main application controller
- **Serial Handler**: USB communication with vehicle systems
- **UI Manager**: LVGL-based interface rendering
- **Data Processor**: Vehicle data interpretation and display
- **Storage Manager**: Persistent data handling

### Data Flow
```
ESP32 → USB Serial → Packet Parser → Data Processor → LVGL UI → Display
                                  ↓
                              File Storage ← Persistence Manager
```

### Threading Model
Single-threaded design with:
- Non-blocking serial I/O
- LVGL timer-based updates
- Event-driven UI interactions

## Development

### Project Structure
```
lvgl-dashboard/
├── src/                    # Main application source
│   └── main.cpp
├── ui/                     # EEZ Studio generated UI
│   ├── ui.c/h
│   └── screens.c/h
├── include/                # Headers and configuration
│   └── lv_conf.h
├── lvgl/                   # LVGL library (submodule)
├── build/                  # Build output
└── scripts/                # Utility scripts
```

### Build System
Uses CMake with automatic dependency detection:
```bash
# Development build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Adding Features
1. **New UI Elements**: Modify EEZ Studio project, regenerate UI files
2. **Data Sources**: Extend packet parser for additional vehicle systems
3. **Audio Features**: Add SigmaTCP commands in DSP controller
4. **Visualization**: Create new LVGL chart types or custom widgets

## Vehicle Integration

### Data Protocol
The system uses a custom packet protocol with checksums:
```
[START_BYTE][TYPE][LENGTH][DATA][CHECKSUM][END_BYTE]
```

### Supported Data Types
- **BMS Data**: Current, voltage, SOC, cell voltages, temperatures
- **Automotive Data**: Speed, gear state, all lighting functions
- **Extensible**: Add new packet types for additional vehicle systems

### Lighting Logic
Implements complex automotive lighting hierarchy:
- DRL mode (no lights active)
- Low beam operation
- High beam override
- Light-ON mode (running lights)
- Independent fog/indicator/brake lights

## Troubleshooting

### Common Issues
1. **Serial Port Access**: Ensure user in `dialout` group, logout/login required
2. **Build Errors**: Check CMakeLists.txt paths match your file structure
3. **No Data**: Verify ESP32 connection and baud rate (115200)
4. **Display Issues**: Confirm SDL2 libraries installed correctly

### Debug Options
```bash
# Monitor serial traffic
screen /dev/ttyUSB0 115200

# Check USB devices
lsusb | grep -E "(Arduino|ESP32)"

# View debug output
./build/LVGLDashboard 2>&1 | tee dashboard.log
```

### Performance Optimization
- Use Release build for production
- Consider dedicated graphics memory on Pi
- Optimize chart update intervals for smooth operation

## License

[License information to be added]

## Contributing

[Contributing guidelines to be added]

## Acknowledgments

- **LVGL**: Graphics library foundation
- **HiFiBerry**: Audio DSP integration
- **SDL2**: Cross-platform display support

## Technical Specifications

### Performance Targets
- **Update Rate**: 100ms display refresh
- **Serial Latency**: <50ms data processing
- **Memory Usage**: <128MB RSS typical
- **CPU Usage**: <25% on Raspberry Pi 4

### Supported Protocols
- **Serial**: 115200 baud, 8N1, custom packet format
- **Audio**: SigmaTCP over TCP/IP (port 8086)
- **Display**: SDL2 framebuffer or hardware acceleration

### Environmental Requirements
- **Operating Temperature**: -20°C to +70°C (automotive grade components)
- **Input Voltage**: 12V nominal (9-16V range)
- **Power Consumption**: <15W typical, <25W peak
