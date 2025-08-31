#ifndef SIMPLIFIED_AUDIO_MANAGER_H
#define SIMPLIFIED_AUDIO_MANAGER_H

#include <string>
#include <functional>
#include <chrono>
#include <atomic>

enum class SimplePlaybackState {
    STOPPED,
    PLAYING, 
    PAUSED,
    UNKNOWN
};

struct SimpleMediaInfo {
    std::string device_name = "No Device";
    SimplePlaybackState state = SimplePlaybackState::STOPPED;
    bool connected = false;
    int volume = 50;
};

class SimplifiedAudioManager {
public:
    SimplifiedAudioManager();
    ~SimplifiedAudioManager();
    
    // Initialize audio systems
    bool initialize();
    void shutdown();
    
    // BeoCreate 4 DSP Volume Control (0-100%)
    bool setVolume(int volume);
    int getVolume();
    
    // EQ Control via DSP (if needed later)
    bool setBass(int level);    // -10 to +10
    bool setMid(int level);     // -10 to +10  
    bool setHigh(int level);    // -10 to +10
    
    // Simple Bluetooth Connection Status
    bool isBluetoothConnected();
    std::string getConnectedDevice();
    
    // Media control (basic)
    bool togglePlayPause();
    bool nextTrack();
    bool previousTrack();
    
    // Get current status
    SimpleMediaInfo getMediaInfo();
    
    // Callback for state changes
    void setStateCallback(std::function<void(const SimpleMediaInfo&)> callback);
    
    // Update method (call every few seconds)
    void update();

private:
    // DSP Control
    bool executeCommand(const std::string& command);
    bool setBeoCreateVolume(int volume);
    int getBeoCreateVolume();
    
    // Simple Bluetooth Control
    bool checkBluetoothStatus();
    void updateBluetoothInfo();
    
    // State
    SimpleMediaInfo current_info;
    std::function<void(const SimpleMediaInfo&)> state_callback;
    std::chrono::steady_clock::time_point last_update;
    
    // Configuration
    bool beocreate_available = false;
    bool bluetooth_available = false;
    int current_volume = 50;
};

#endif // SIMPLIFIED_AUDIO_MANAGER_H