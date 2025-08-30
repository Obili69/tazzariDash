#ifndef BLUETOOTH_AUDIO_MANAGER_H
#define BLUETOOTH_AUDIO_MANAGER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

enum class PlaybackState {
    STOPPED,
    PLAYING,
    PAUSED
};

struct MediaInfo {
    std::string title = "Unknown Track";
    std::string artist = "Unknown Artist";
    std::string album = "Unknown Album";
    PlaybackState state = PlaybackState::STOPPED;
    int duration_seconds = 0;
    int position_seconds = 0;
    bool connected = false;
};

class BluetoothAudioManager {
public:
    BluetoothAudioManager();
    ~BluetoothAudioManager();
    
    // Initialize Bluetooth and audio systems
    bool initialize();
    void shutdown();
    
    // Media control functions
    bool play();
    bool pause();
    bool stop();
    bool next();
    bool previous();
    
    // Volume control (0-100)
    bool setVolume(int volume);
    int getVolume();
    
    // Connection management
    bool isConnected();
    void scanForDevices();
    bool connectToDevice(const std::string& address);
    void disconnectCurrent();
    
    // Media information
    MediaInfo getCurrentMediaInfo();
    
    // Set callback for media state changes
    void setMediaStateCallback(std::function<void(const MediaInfo&)> callback);
    
    // Update method to be called regularly
    void update();

private:
    struct BluetoothImpl;
    std::unique_ptr<BluetoothImpl> impl;
    
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    
    // Internal methods
    void initializePulseAudio();
    void initializeBlueZ();
    void startMonitorThread();
    void monitorLoop();
    
    // D-Bus communication helpers
    bool executeDBusCommand(const std::string& command);
    std::string getDBusProperty(const std::string& path, const std::string& interface, const std::string& property);
    bool setDBusProperty(const std::string& path, const std::string& interface, const std::string& property, const std::string& value);
    
    // PulseAudio helpers
    bool setPulseVolume(int volume);
    int getPulseVolume();
    void routeAudioToJack();
    
    // Media control via MPRIS
    bool sendMPRISCommand(const std::string& command);
    
    std::function<void(const MediaInfo&)> media_callback;
    MediaInfo current_media;
    int current_volume = 50;
    std::string connected_device_path;
};

#endif // BLUETOOTH_AUDIO_MANAGER_H