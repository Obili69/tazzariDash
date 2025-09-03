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
    std::string track_title = "";
    std::string artist = "";
    std::string album = "";
    SimplePlaybackState state = SimplePlaybackState::STOPPED;
    bool connected = false;
    int volume = 50;
    std::string album_art_path = ""; // Future: path to downloaded album art
};

class SimplifiedAudioManager {
public:
    SimplifiedAudioManager();
    ~SimplifiedAudioManager();
    
    // Initialize audio systems
    bool initialize();
    void shutdown();
    
    // HiFiBerry BeoCreate 4 DSP Volume Control via REST API (0-100%)
    bool setVolume(int volume);
    int getVolume();
    
    // EQ Control via DSP REST API
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
    // REST API Communication
    bool makeRestApiCall(const std::string& method, const std::string& endpoint, 
                         const std::string& data = "", std::string* response = nullptr);
    bool testRestApiConnection();
    
    // DSP Volume Control via REST API
    bool setDSPVolume(int volume);
    int getDSPVolume();
    
    // DSP EQ Control via REST API  
    bool setDSPEQ(const std::string& band, int level);
    
    // Bluetooth Control
    bool checkBluetoothStatus();
    void updateBluetoothInfo();
    void updateMediaMetadata();
    void updatePlaybackState();
    
    // State
    SimpleMediaInfo current_info;
    std::function<void(const SimpleMediaInfo&)> state_callback;
    std::chrono::steady_clock::time_point last_update;
    
    // Internal state tracking (since iOS doesn't expose status reliably)
    bool internal_playing_state = false;
    std::chrono::steady_clock::time_point last_command_time;
    
    // Configuration
    bool dsp_rest_api_available = false;
    bool bluetooth_available = false;
    int current_volume = 50;
    
    // REST API settings
    std::string rest_api_base_url = "http://localhost:13141";
    
    // Volume register addresses for BeoCreate Universal profile
    std::string volume_register = "volumeControlRegister"; // From profile metadata
    
    // EQ register addresses (will be determined from profile metadata)
    std::string bass_register = "";
    std::string mid_register = "";  
    std::string high_register = "";
};

#endif // SIMPLIFIED_AUDIO_MANAGER_H