// include/MultiAudioManager.h - Unified audio interface for all hardware options

#ifndef MULTI_AUDIO_MANAGER_H
#define MULTI_AUDIO_MANAGER_H

#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <memory>

// Audio hardware types (compile-time configuration)
enum class AudioHardware {
    AUX_BUILTIN,      // Built-in 3.5mm jack with PulseAudio
    HIFIBERRY_DAC,    // HiFiBerry DAC+ (no amplifier)  
    HIFIBERRY_AMP4,   // HiFiBerry AMP4 (amplifier, no DSP) - same as DAC+
    HIFIBERRY_BEOCREATE4  // HiFiBerry BeoCreate 4 (DSP + amplifier)
};

// Build-time configuration - set by CMake
#ifdef AUDIO_HARDWARE_AUX
    constexpr AudioHardware AUDIO_HW = AudioHardware::AUX_BUILTIN;
#elif defined(AUDIO_HARDWARE_DAC)
    constexpr AudioHardware AUDIO_HW = AudioHardware::HIFIBERRY_DAC;
#elif defined(AUDIO_HARDWARE_AMP4)  
    constexpr AudioHardware AUDIO_HW = AudioHardware::HIFIBERRY_AMP4;
#elif defined(AUDIO_HARDWARE_BEOCREATE4)
    constexpr AudioHardware AUDIO_HW = AudioHardware::HIFIBERRY_BEOCREATE4;
#else
    constexpr AudioHardware AUDIO_HW = AudioHardware::HIFIBERRY_BEOCREATE4; // Default
#endif

enum class SimplePlaybackState {
    STOPPED, PLAYING, PAUSED, UNKNOWN
};

struct SimpleMediaInfo {
    std::string device_name = "No Device";
    std::string track_title = "";
    std::string artist = "";
    std::string album = "";
    SimplePlaybackState state = SimplePlaybackState::STOPPED;
    bool connected = false;
    int volume = 50;
};

class MultiAudioManager {
public:
    MultiAudioManager();
    ~MultiAudioManager();
    
    // Initialize audio system based on compile-time hardware selection
    bool initialize();
    void shutdown();
    
    // Universal volume control (0-100%)
    bool setVolume(int volume);
    int getVolume();
    
    // Universal EQ control (-10 to +10 dB)
    bool setBass(int level);
    bool setMid(int level); 
    bool setHigh(int level);
    
    // Bluetooth control (common across all hardware)
    bool isBluetoothConnected();
    std::string getConnectedDevice();
    bool togglePlayPause();
    bool nextTrack();
    bool previousTrack();
    
    // Status and callbacks
    SimpleMediaInfo getMediaInfo();
    void setStateCallback(std::function<void(const SimpleMediaInfo&)> callback);
    void update();
    
    // Hardware information
    static std::string getHardwareName();
    static bool hasHardwareVolume();
    static bool hasHardwareEQ();

private:
    // Forward declaration of implementation classes
    class BaseAudioImpl;
    class AuxAudioImpl;
    class HiFiBerryAudioImpl;
    class BeoCreateAudioImpl;
    
    std::unique_ptr<BaseAudioImpl> impl;
    
    // Common state
    SimpleMediaInfo current_info;
    std::function<void(const SimpleMediaInfo&)> state_callback;
    std::chrono::steady_clock::time_point last_update;
    
    // Bluetooth helpers (common to all implementations)
    bool checkBluetoothStatus();
    void updateBluetoothInfo();
    void updateMediaMetadata();
    bool internal_playing_state = false;
};

#endif // MULTI_AUDIO_MANAGER_H