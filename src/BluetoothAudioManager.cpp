#include "BluetoothAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <algorithm>

struct BluetoothAudioManager::BluetoothImpl {
    bool bluez_initialized = false;
    bool pulse_initialized = false;
    std::string current_device_address;
    std::chrono::steady_clock::time_point last_update;
};

BluetoothAudioManager::BluetoothAudioManager() 
    : impl(std::make_unique<BluetoothImpl>()) {
}

BluetoothAudioManager::~BluetoothAudioManager() {
    shutdown();
}

bool BluetoothAudioManager::initialize() {
    std::cout << "BT Audio: Initializing Bluetooth Audio Manager..." << std::endl;
    
    impl->last_update = std::chrono::steady_clock::now();
    
    // Initialize PulseAudio first
    initializePulseAudio();
    
    // Initialize BlueZ
    initializeBlueZ();
    
    // Start monitoring thread
    startMonitorThread();
    
    std::cout << "BT Audio: Initialization complete" << std::endl;
    return true;
}

void BluetoothAudioManager::shutdown() {
    std::cout << "BT Audio: Shutting down..." << std::endl;
    running = false;
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    disconnectCurrent();
}

void BluetoothAudioManager::initializePulseAudio() {
    std::cout << "BT Audio: Initializing PulseAudio..." << std::endl;
    
    // Ensure PulseAudio is running
    system("pulseaudio --check || pulseaudio --start");
    
    // Route audio to analog jack by default
    routeAudioToJack();
    
    // Set initial volume
    setPulseVolume(current_volume);
    
    impl->pulse_initialized = true;
    std::cout << "BT Audio: PulseAudio initialized" << std::endl;
}

void BluetoothAudioManager::initializeBlueZ() {
    std::cout << "BT Audio: Initializing BlueZ..." << std::endl;
    
    // Start Bluetooth service if not running
    system("sudo systemctl start bluetooth");
    
    // Enable Bluetooth controller
    executeDBusCommand("hciconfig hci0 up");
    
    // Set discoverable and pairable
    executeDBusCommand("bluetoothctl power on");
    executeDBusCommand("bluetoothctl discoverable on");
    executeDBusCommand("bluetoothctl pairable on");
    
    // Set agent for auto-pairing
    executeDBusCommand("bluetoothctl agent on");
    executeDBusCommand("bluetoothctl default-agent");
    
    impl->bluez_initialized = true;
    std::cout << "BT Audio: BlueZ initialized - device is discoverable" << std::endl;
}

void BluetoothAudioManager::startMonitorThread() {
    running = true;
    monitor_thread = std::thread([this]() {
        monitorLoop();
    });
}

void BluetoothAudioManager::monitorLoop() {
    while (running) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool BluetoothAudioManager::executeDBusCommand(const std::string& command) {
    std::cout << "BT Audio: Executing: " << command << std::endl;
    int result = system(command.c_str());
    return result == 0;
}

void BluetoothAudioManager::routeAudioToJack() {
    // Set default sink to analog output (3.5mm jack)
    system("pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo 2>/dev/null || "
           "pactl set-default-sink 0 2>/dev/null || "
           "echo 'Could not set audio output'");
    
    std::cout << "BT Audio: Audio routed to analog jack" << std::endl;
}

bool BluetoothAudioManager::setPulseVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    std::string command = "pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(volume) + "%";
    bool success = executeDBusCommand(command);
    
    if (success) {
        current_volume = volume;
        std::cout << "BT Audio: Volume set to " << volume << "%" << std::endl;
    }
    
    return success;
}

int BluetoothAudioManager::getPulseVolume() {
    // Use pactl to get current volume
    FILE* pipe = popen("pactl get-sink-volume @DEFAULT_SINK@ | grep -oP '\\d+%' | head -1 | tr -d '%'", "r");
    if (pipe) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            current_volume = std::atoi(buffer);
        }
        pclose(pipe);
    }
    return current_volume;
}

bool BluetoothAudioManager::setVolume(int volume) {
    return setPulseVolume(volume);
}

int BluetoothAudioManager::getVolume() {
    return getPulseVolume();
}

bool BluetoothAudioManager::play() {
    std::cout << "BT Audio: Play command" << std::endl;
    
    // Try MPRIS first (for connected Bluetooth devices)
    if (sendMPRISCommand("Play")) {
        current_media.state = PlaybackState::PLAYING;
        return true;
    }
    
    // Fallback: try generic Bluetooth commands
    return executeDBusCommand("bluetoothctl -- << 'EOF'\nconnect\nEOF");
}

bool BluetoothAudioManager::pause() {
    std::cout << "BT Audio: Pause command" << std::endl;
    
    if (sendMPRISCommand("Pause")) {
        current_media.state = PlaybackState::PAUSED;
        return true;
    }
    
    return false;
}

bool BluetoothAudioManager::stop() {
    std::cout << "BT Audio: Stop command" << std::endl;
    
    if (sendMPRISCommand("Stop")) {
        current_media.state = PlaybackState::STOPPED;
        return true;
    }
    
    return false;
}

bool BluetoothAudioManager::next() {
    std::cout << "BT Audio: Next track command" << std::endl;
    return sendMPRISCommand("Next");
}

bool BluetoothAudioManager::previous() {
    std::cout << "BT Audio: Previous track command" << std::endl;
    return sendMPRISCommand("Previous");
}

bool BluetoothAudioManager::sendMPRISCommand(const std::string& command) {
    // Get list of available MPRIS players
    FILE* pipe = popen("dbus-send --print-reply --session --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep -o '\"org.mpris.MediaPlayer2.[^\"]*\"' | head -1 | tr -d '\"'", "r");
    
    if (!pipe) return false;
    
    char player_name[256];
    if (fgets(player_name, sizeof(player_name), pipe)) {
        // Remove newline
        player_name[strcspn(player_name, "\n")] = 0;
        
        if (strlen(player_name) > 0) {
            std::string dbus_command = "dbus-send --print-reply --session --dest=" + std::string(player_name) + 
                                     " /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player." + command;
            
            pclose(pipe);
            return executeDBusCommand(dbus_command);
        }
    }
    
    pclose(pipe);
    return false;
}

void BluetoothAudioManager::scanForDevices() {
    std::cout << "BT Audio: Scanning for devices..." << std::endl;
    executeDBusCommand("bluetoothctl scan on &");
    
    // Auto-scan in background for 10 seconds
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        executeDBusCommand("bluetoothctl scan off");
        std::cout << "BT Audio: Device scan complete" << std::endl;
    }).detach();
}

bool BluetoothAudioManager::connectToDevice(const std::string& address) {
    std::cout << "BT Audio: Connecting to device: " << address << std::endl;
    
    std::string command = "bluetoothctl connect " + address;
    bool success = executeDBusCommand(command);
    
    if (success) {
        impl->current_device_address = address;
        connected_device_path = "/org/bluez/hci0/dev_" + address;
        std::replace(connected_device_path.begin(), connected_device_path.end(), ':', '_');
        
        // Trust the device
        executeDBusCommand("bluetoothctl trust " + address);
        
        current_media.connected = true;
        std::cout << "BT Audio: Connected to " << address << std::endl;
    }
    
    return success;
}

void BluetoothAudioManager::disconnectCurrent() {
    if (!impl->current_device_address.empty()) {
        std::cout << "BT Audio: Disconnecting from " << impl->current_device_address << std::endl;
        executeDBusCommand("bluetoothctl disconnect " + impl->current_device_address);
        impl->current_device_address.clear();
        connected_device_path.clear();
        current_media.connected = false;
    }
}

bool BluetoothAudioManager::isConnected() {
    if (impl->current_device_address.empty()) return false;
    
    // Check if device is still connected via D-Bus
    std::string command = "bluetoothctl info " + impl->current_device_address + " | grep 'Connected: yes'";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        bool connected = fgets(buffer, sizeof(buffer), pipe) != nullptr;
        pclose(pipe);
        current_media.connected = connected;
        return connected;
    }
    
    return false;
}

MediaInfo BluetoothAudioManager::getCurrentMediaInfo() {
    return current_media;
}

void BluetoothAudioManager::setMediaStateCallback(std::function<void(const MediaInfo&)> callback) {
    media_callback = callback;
}

void BluetoothAudioManager::update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - impl->last_update).count();
    
    // Update every 2 seconds to avoid excessive system calls
    if (elapsed < 2000) return;
    
    impl->last_update = now;
    
    // Update connection status
    current_media.connected = isConnected();
    
    // If connected, try to get media info via MPRIS
    if (current_media.connected) {
        updateMediaInfo();
    }
    
    // Call callback if set
    if (media_callback) {
        media_callback(current_media);
    }
}

void BluetoothAudioManager::updateMediaInfo() {
    // Get media info from MPRIS if available
    FILE* pipe = popen("dbus-send --print-reply --session --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep -o '\"org.mpris.MediaPlayer2.[^\"]*\"' | head -1 | tr -d '\"'", "r");
    
    if (!pipe) return;
    
    char player_name[256];
    if (fgets(player_name, sizeof(player_name), pipe)) {
        player_name[strcspn(player_name, "\n")] = 0;
        
        if (strlen(player_name) > 0) {
            // Get playback status
            std::string status_cmd = "dbus-send --print-reply --session --dest=" + std::string(player_name) + 
                                   " /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:PlaybackStatus 2>/dev/null | grep -o '\"[^\"]*\"' | tail -1 | tr -d '\"'";
            
            FILE* status_pipe = popen(status_cmd.c_str(), "r");
            if (status_pipe) {
                char status[32];
                if (fgets(status, sizeof(status), status_pipe)) {
                    status[strcspn(status, "\n")] = 0;
                    
                    if (strcmp(status, "Playing") == 0) {
                        current_media.state = PlaybackState::PLAYING;
                    } else if (strcmp(status, "Paused") == 0) {
                        current_media.state = PlaybackState::PAUSED;
                    } else {
                        current_media.state = PlaybackState::STOPPED;
                    }
                }
                pclose(status_pipe);
            }
            
            // Get track title (simplified for now)
            std::string title_cmd = "dbus-send --print-reply --session --dest=" + std::string(player_name) + 
                                  " /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get string:org.mpris.MediaPlayer2.Player string:Metadata 2>/dev/null | grep -A1 'xesam:title' | tail -1 | grep -o '\"[^\"]*\"' | tr -d '\"'";
            
            FILE* title_pipe = popen(title_cmd.c_str(), "r");
            if (title_pipe) {
                char title[256];
                if (fgets(title, sizeof(title), title_pipe)) {
                    title[strcspn(title, "\n")] = 0;
                    if (strlen(title) > 0) {
                        current_media.title = title;
                    }
                }
                pclose(title_pipe);
            }
        }
    }
    
    pclose(pipe);
}