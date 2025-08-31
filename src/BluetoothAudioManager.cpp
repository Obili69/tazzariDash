#include "BluetoothAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>  // For strcspn, strlen
#ifdef __linux__
#include <pthread.h>  // For CPU affinity
#include <sched.h>
#endif

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
    
    // ALWAYS route audio to analog jack (never Bluetooth output)
    routeAudioToJack();
    
    // Set initial volume
    setPulseVolume(current_volume);
    
    // Force audio routing every few seconds to maintain analog output
    std::thread([this]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (running) {
                routeAudioToJack();  // Ensure we stay on analog jack
            }
        }
    }).detach();
    
    impl->pulse_initialized = true;
    std::cout << "BT Audio: PulseAudio initialized with forced analog output" << std::endl;
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
    // Set thread to run on different CPU core (if available)
    #ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);  // Use core 1 (LVGL uses core 0)
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #endif
    
    while (running) {
        update();
        // Longer sleep to reduce interference with LVGL
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

bool BluetoothAudioManager::executeDBusCommand(const std::string& command) {
    std::cout << "BT Audio: Executing: " << command << std::endl;
    int result = system(command.c_str());
    bool success = (result == 0);
    
    if (!success) {
        std::cout << "BT Audio: Command failed with exit code: " << result << std::endl;
    }
    
    return success;
}

void BluetoothAudioManager::routeAudioToJack() {
    // ALWAYS route audio to analog jack (never Bluetooth output)
    std::cout << "BT Audio: Forcing audio output to analog jack..." << std::endl;
    
    // Pi 3B+ / Pi 4 analog output
    system("pactl set-default-sink alsa_output.platform-bcm2835_audio.analog-stereo 2>/dev/null || "
           // Pi 5 analog output  
           "pactl set-default-sink alsa_output.platform-bcm2835_headphones.analog-stereo 2>/dev/null || "
           // Generic analog fallback
           "pactl set-default-sink 0 2>/dev/null");
    
    // Move any existing streams to analog jack
    system("pactl list short sink-inputs | cut -f1 | xargs -I{} pactl move-sink-input {} @DEFAULT_SINK@ 2>/dev/null");
    
    // Disable auto-switching to Bluetooth sinks
    system("pactl unload-module module-switch-on-connect 2>/dev/null || true");
    
    std::cout << "BT Audio: All audio routed to analog jack (no Bluetooth output)" << std::endl;
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
    
    std::cout << "BT Audio: Play command failed" << std::endl;
    return false;
}

bool BluetoothAudioManager::pause() {
    std::cout << "BT Audio: Pause command" << std::endl;
    
    if (sendMPRISCommand("Pause")) {
        current_media.state = PlaybackState::PAUSED;
        return true;
    }
    
    std::cout << "BT Audio: Pause command failed" << std::endl;
    return false;
}

bool BluetoothAudioManager::stop() {
    std::cout << "BT Audio: Stop command" << std::endl;
    
    if (sendMPRISCommand("Stop")) {
        current_media.state = PlaybackState::STOPPED;
        return true;
    }
    
    std::cout << "BT Audio: Stop command failed" << std::endl;
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
    std::cout << "BT Audio: Attempting MPRIS command: " << command << std::endl;
    
    // Get list of available MPRIS players with better command
    FILE* pipe = popen("busctl --user list | grep 'org.mpris.MediaPlayer2' | head -1 | awk '{print $1}'", "r");
    
    if (!pipe) {
        std::cout << "BT Audio: Failed to open pipe for MPRIS detection" << std::endl;
        return false;
    }
    
    char player_name[256];
    if (fgets(player_name, sizeof(player_name), pipe)) {
        // Remove newline
        player_name[strcspn(player_name, "\n")] = 0;
        
        if (strlen(player_name) > 0) {
            std::cout << "BT Audio: Found MPRIS player: " << player_name << std::endl;
            
            // Use busctl instead of dbus-send for more reliability
            std::string busctl_command = "busctl --user call " + std::string(player_name) + 
                                       " /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player " + command;
            
            std::cout << "BT Audio: Executing: " << busctl_command << std::endl;
            pclose(pipe);
            return executeDBusCommand(busctl_command);
        }
    }
    
    pclose(pipe);
    std::cout << "BT Audio: No MPRIS players found" << std::endl;
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
    // Simple check: look for any connected Bluetooth audio device
    FILE* pipe = popen("bluetoothctl devices Connected | grep -v '^$' | wc -l", "r");
    if (pipe) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            int connected_count = std::atoi(buffer);
            pclose(pipe);
            current_media.connected = (connected_count > 0);
            return current_media.connected;
        }
        pclose(pipe);
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
    
    // Update every 5 seconds to reduce interference with LVGL
    if (elapsed < 5000) return;
    
    impl->last_update = now;
    
    // Update connection status (lightweight check)
    current_media.connected = isConnected();
    
    // Only do heavy MPRIS calls if connected
    if (current_media.connected) {
        updateMediaInfo();
    } else {
        // Reset media info when disconnected
        current_media.title = "Unknown Track";
        current_media.artist = "Unknown Artist";
        current_media.state = PlaybackState::STOPPED;
    }
    
    // Call callback if set
    if (media_callback) {
        media_callback(current_media);
    }
}

void BluetoothAudioManager::updateMediaInfo() {
    // Get media info using busctl (more reliable than dbus-send)
    FILE* pipe = popen("busctl --user list | grep 'org.mpris.MediaPlayer2' | head -1 | awk '{print $1}'", "r");
    
    if (!pipe) {
        std::cout << "BT Audio: Cannot query MPRIS players" << std::endl;
        return;
    }
    
    char player_name[256];
    if (fgets(player_name, sizeof(player_name), pipe)) {
        player_name[strcspn(player_name, "\n")] = 0;
        
        if (strlen(player_name) > 0) {
            std::cout << "BT Audio: Updating media info from: " << player_name << std::endl;
            
            // Get playback status
            std::string status_cmd = "busctl --user get-property " + std::string(player_name) + 
                                   " /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player PlaybackStatus 2>/dev/null | awk '{print $2}' | tr -d '\"'";
            
            FILE* status_pipe = popen(status_cmd.c_str(), "r");
            if (status_pipe) {
                char status[32];
                if (fgets(status, sizeof(status), status_pipe)) {
                    status[strcspn(status, "\n")] = 0;
                    
                    std::cout << "BT Audio: Playback status: " << status << std::endl;
                    
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
            
            // Get track title (simplified approach)
            std::string title_cmd = "busctl --user get-property " + std::string(player_name) + 
                                  " /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player Metadata 2>/dev/null | grep -A1 'xesam:title' | tail -1 | cut -d'\"' -f2";
            
            FILE* title_pipe = popen(title_cmd.c_str(), "r");
            if (title_pipe) {
                char title[256];
                if (fgets(title, sizeof(title), title_pipe)) {
                    title[strcspn(title, "\n")] = 0;
                    if (strlen(title) > 0 && strcmp(title, "xesam:title") != 0) {
                        current_media.title = title;
                        std::cout << "BT Audio: Track title: " << title << std::endl;
                    }
                }
                pclose(title_pipe);
            }
        } else {
            std::cout << "BT Audio: No MPRIS players active" << std::endl;
        }
    }
    
    pclose(pipe);
}