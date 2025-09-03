# LVGL Tazzari Dashboard with Multi-Audio Hardware Support

Modern automotive dashboard with selectable audio hardware: built-in audio, HiFiBerry DAC+, AMP4, or BeoCreate 4 DSP for electric vehicles.

## Features
- **Vehicle Data**: Speed, battery, gear, lighting via ESP32 serial
- **Multi-Audio Hardware**: Support for 4 different audio configurations
- **Universal Controls**: Volume, EQ, and media controls work across all hardware
- **Bluetooth Audio**: Phone streams music through dashboard to selected audio hardware
- **Real-time Charts**: Voltage/current monitoring
- **Touch Controls**: Volume, EQ, media control, trip reset

## Audio Hardware Options

| Hardware | Description | Volume Control | EQ Control | Requirements |
|----------|-------------|----------------|------------|--------------|
| **Built-in (AUX)** | Pi's 3.5mm jack | PulseAudio software | alsaeq software | None |
| **HiFiBerry DAC+** | High-quality DAC | ALSA hardware mixer | alsaeq software | External amplifier |
| **HiFiBerry AMP4** | DAC + amplifier | ALSA hardware mixer | alsaeq software | Direct speaker connection |
| **BeoCreate 4** | 4-channel DSP | REST API hardware | DSP biquad filters | 4-channel speaker system |

## Quick Start

```bash
# 1. One-time setup with audio hardware selection
chmod +x setup.sh
./setup.sh                    # Interactive menu
# OR
./setup.sh --audio-amp4       # Direct selection

# 2. Reboot (required for hardware changes)
sudo reboot

# 3. Load DSP profile (BeoCreate 4 only)
./setup_dsp_beocreate4.sh     # If using BeoCreate 4

# 4. Build for your audio hardware  
./build.sh --audio-amp4       # Build for AMP4
./build.sh --audio-dac        # Build for DAC+
./build.sh --audio-aux        # Build for built-in
./build.sh --audio-beocreate4 # Build for BeoCreate 4

# 5. Connect phone to Bluetooth and run
./pair_phone.sh               # Bluetooth pairing help
./run_amp4_dev.sh             # Run development version
```

## Build Examples

```bash
# Development builds (windowed, with cursor)
./build.sh --dev --audio-aux         # Built-in 3.5mm jack
./build.sh --dev --audio-dac         # HiFiBerry DAC+
./build.sh --dev --audio-amp4        # HiFiBerry AMP4
./build.sh --dev --audio-beocreate4  # BeoCreate 4 DSP

# Production builds (fullscreen, no cursor)
./build.sh --deployment --audio-amp4        # Production AMP4
./build.sh --deployment --audio-beocreate4  # Production BeoCreate 4
```

## Audio System Architectures

### Built-in Audio (AUX)
```
Phone (Bluetooth) → Pi (TazzariAudio) → Built-in DAC → 3.5mm Jack → External Amp
                                        ↓
Dashboard Controls ←→ PulseAudio ←→ alsaeq Plugin ←→ Software Volume/EQ
```

### HiFiBerry DAC+ / AMP4  
```
Phone (Bluetooth) → Pi (TazzariAudio) → HiFiBerry PCM512x → [AMP4: Amplifier] → Speakers
                                        ↓
Dashboard Controls ←→ ALSA Mixer ←→ "Digital" Hardware Volume + alsaeq EQ
```

### HiFiBerry BeoCreate 4
```
Phone (Bluetooth) → Pi (TazzariAudio) → BeoCreate 4 DSP → 4-Channel Amplifiers → Speakers
                                        ↓
Dashboard Controls ←→ REST API ←→ SigmaTCP Server ←→ Hardware DSP Registers
```

## Hardware Setup

### Built-in Audio (AUX)
- **Hardware**: None required
- **Boot Config**: `dtparam=audio=on`
- **Connection**: 3.5mm jack to external amplifier
- **Volume**: Software control via PulseAudio (0-100%)
- **EQ**: Software 10-band equalizer via alsaeq

### HiFiBerry DAC+ 
- **Hardware**: HiFiBerry DAC+ + external amplifier
- **Boot Config**: `dtoverlay=hifiberry-dacplus-std`
- **Connection**: DAC+ line output to amplifier input
- **Volume**: Hardware control via ALSA "Digital" mixer (0-100%)
- **EQ**: Software 10-band equalizer via alsaeq

### HiFiBerry AMP4
- **Hardware**: HiFiBerry AMP4 (built-in amplifier)
- **Boot Config**: `dtoverlay=hifiberry-dacplus-std`
- **Connection**: Direct speaker wires to AMP4 terminals
- **Volume**: Hardware control via ALSA "Digital" mixer (0-100%)
- **EQ**: Software 10-band equalizer via alsaeq

### HiFiBerry BeoCreate 4
- **Hardware**: HiFiBerry BeoCreate 4 DSP + 4-channel amplifier
- **Boot Config**: `dtoverlay=hifiberry-dac` + DSP tools
- **Connection**: 4 speakers (front L/R, rear L/R) with crossover
- **Volume**: Hardware control via DSP REST API (0-100%)
- **EQ**: Hardware 3-band EQ via DSP biquad filters (-10 to +10dB)

## Development Workflow

```bash
# Choose your audio hardware during development
./build.sh --help              # See all options

# Development cycle
./build.sh --audio-amp4        # Build for AMP4
./test_audio_amp4.sh           # Test audio system
./run_amp4_dev.sh              # Run in window

# Switch hardware easily
./build.sh --audio-dac         # Now build for DAC+
./run_dac_dev.sh               # Run with DAC+ configuration

# Production deployment
./build.sh --deployment --audio-amp4  # Production build
sudo reboot                            # Auto-starts dashboard
```

## Troubleshooting by Hardware

### Built-in Audio (AUX)
```bash
./test_audio_aux.sh            # Test built-in audio
pulseaudio --check             # Check PulseAudio status
pactl list sinks short         # List audio devices
speaker-test -t sine -f 1000   # Test audio output
```

**Common Issues:**
- **No audio**: Check 3.5mm connection, restart PulseAudio
- **No EQ**: Install with `sudo apt install libasound2-plugin-equal`
- **No volume control**: Check PulseAudio mixer: `pavucontrol`

### HiFiBerry DAC+ / AMP4
```bash
./test_audio_dac.sh            # Test DAC+ (or AMP4)
aplay -l | grep hifiberry      # Check ALSA detection  
amixer get Digital             # Check hardware volume
alsamixer                      # Interactive mixer control
```

**Common Issues:**
- **Device not detected**: Add `dtoverlay=hifiberry-dacplus-std` to boot config
- **No volume control**: Check ALSA mixer: `amixer sget Digital`
- **No EQ**: Run `alsamixer -D equal` after installing alsaeq
- **Boot config conflict**: Ensure `dtparam=audio=off` (disable built-in)

### HiFiBerry BeoCreate 4
```bash
./test_audio_beocreate4.sh     # Test BeoCreate 4 system
curl http://localhost:13141/checksum    # Test REST API
sudo systemctl status sigmatcpserver    # Check DSP service
./setup_dsp_beocreate4.sh      # Load DSP profile
```

**Common Issues:**
- **REST API not responding**: Start with `sudo systemctl start sigmatcpserver`
- **No DSP profile**: Run `./setup_dsp_beocreate4.sh` after reboot
- **No volume control**: Check DSP profile loaded correctly
- **Boot config**: Ensure I2C/SPI enabled: `dtparam=i2c_arm=on,i2s=on,spi=on`

### Universal Bluetooth Issues
```bash
./pair_phone.sh                # Bluetooth pairing help
bluetoothctl devices Connected # Check connected devices
sudo systemctl status a2dp-agent     # Check A2DP service
```

**Common Issues:**
- **Phone can't find Pi**: Check `bluetoothctl discoverable on`
- **Connected but no audio**: Check audio routing to selected hardware
- **Pairing fails**: Restart Bluetooth: `sudo systemctl restart bluetooth`

## Project Structure
```
tazzariDash/
├── src/                       # C++ source files
│   ├── main.cpp              # Main dashboard (uses MultiAudioManager)
│   ├── SerialCommunication.cpp
│   └── MultiAudioManager.cpp # NEW: Unified audio interface
├── include/                  # Header files  
│   ├── MultiAudioManager.h   # NEW: Universal audio interface
│   └── SerialCommunication.h
├── ui/                      # LVGL UI files
├── build/                   # Build output (generated)
├── setup.sh                 # Multi-hardware setup script
├── build.sh                 # Multi-hardware build script  
├── test_audio_*.sh          # Hardware-specific test scripts
├── run_*_dev.sh            # Hardware-specific run scripts
└── pair_phone.sh           # Bluetooth pairing helper
```

## Advanced Configuration

### Custom Audio Hardware
To add support for additional audio hardware, modify:
1. **MultiAudioManager.h**: Add new `AudioHardware` enum value
2. **MultiAudioManager.cpp**: Add implementation class  
3. **CMakeLists.txt**: Add new build configuration
4. **setup.sh**: Add hardware-specific installation steps

### Boot Configuration Files
Audio hardware boot configurations are automatically added to:
- `/boot/firmware/config.txt` (newer Pi OS)
- `/boot/config.txt` (older Pi OS)

### ALSA Configuration
Hardware-specific ALSA configurations are created in:
- `/etc/asound.conf` (system-wide ALSA settings)
- Custom configurations for alsaeq routing

## Audio Quality Comparison

| Hardware | Sample Rate | Bit Depth | SNR | THD+N | Use Case |
|----------|-------------|-----------|-----|-------|----------|
| **Built-in** | 48kHz | 16-bit | ~90dB | ~0.05% | Basic audio, development |
| **DAC+** | 192kHz | 32-bit | ~112dB | ~0.0018% | High-quality line output |
| **AMP4** | 192kHz | 32-bit | ~112dB | ~0.0018% | Direct speaker drive |
| **BeoCreate 4** | 192kHz | 32-bit | ~112dB | ~0.0018% | Professional DSP processing |

## Roadmap

- **v1.0**: Multi-audio hardware support ✓
- **v1.1**: Room correction for BeoCreate 4
- **v1.2**: Custom DSP filters and crossovers  
- **v1.3**: Multi-zone audio support
- **v1.4**: Wireless speaker integration
- **v2.0**: Advanced vehicle integration (CAN bus)

## Contributing

When adding audio hardware support:
1. Follow the existing pattern in `MultiAudioManager`
2. Add hardware-specific implementation class
3. Update build system and setup scripts
4. Add comprehensive testing scripts
5. Update documentation

## License

Built with LVGL, SDL2, and love for electric vehicles! 🚗⚡

## Hardware Compatibility Matrix

| Pi Model | Built-in | DAC+ | AMP4 | BeoCreate 4 | Notes |
|----------|----------|------|------|-------------|--------|
| **Pi 4** | ✓ | ✓ | ✓ | ✓ | Full support all hardware |
| **Pi 5** | ✓ | ✓ | ✓ | ✓ | Use updated overlays |  
| **Pi 3B+** | ✓ | ✓ | ✓ | ✓ | BeoCreate 4 may be slower |

## Power Requirements

| Hardware | Pi Power | Additional Power | Total |
|----------|----------|------------------|--------|
| **Built-in** | 5V/3A | External amp | 15W + amp |
| **DAC+** | 5V/3A | External amp | 15W + amp |
| **AMP4** | 5V/3A | None (built-in amp) | 15W |
| **BeoCreate 4** | 5V/3A | None (built-in amps) | 15W |

**Vehicle Integration**: All configurations work with 12V→5V converters for automotive use.

## Support

- **Hardware Issues**: Check respective test scripts first
- **Build Issues**: Verify dependencies match selected audio hardware  
- **Audio Quality**: Ensure proper boot configuration for selected hardware
- **Bluetooth**: Common across all hardware - check A2DP agent status

Choose the audio hardware that best fits your project requirements, from simple built-in audio for development to professional BeoCreate 4 DSP for production vehicles!
