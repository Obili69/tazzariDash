#include "SimplifiedAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>

SimplifiedAudioManager::SimplifiedAudioManager() {
    last_update = std::chrono::steady_clock::now();
}

SimplifiedAudioManager::~SimplifiedAudioManager() {
    shutdown();
}

bool SimplifiedAudioManager::initialize() {
    std::cout << "Audio: Initializing Simplified Audio Manager..." << std::endl;
    
    // Check if BeoCreate DSP tools are available
    beocreate_available = (system("which amixer > /dev/null 2>&1") == 0);
    if (beocreate_available) {
        // Test if DSPVolume control exists
        beocreate_available = (system("amixer sget DSPVolume > /dev/null 2>&1") == 0);
        if (beocreate_available) {
            std::cout << "Audio: BeoCreate DSP volume control detected" << std::endl;
            // Set initial volume
            setBeoCreateVolume(current_volume);
        } else {
            std::cout << "Audio: DSPVolume control not found, checking for beocreate-client..." << std::endl;
            // Alternative: check for beocreate-client tool
            beocreate_available = (system("which beocreate-client > /dev/null 2>&1") == 0);
            if (beocreate_available) {
                std::cout << "Audio: beocreate-client found" << std::endl;
            }
        }
    }
    
    if (!beocreate_available) {
        std::cout << "Audio: BeoCreate DSP not available - using system volume" << std::endl;
    }
    
    // Check if Bluetooth is available (minimal check)
    bluetooth_available = (system("which bluetoothctl > /dev/null 2>&1") == 0);
    if (bluetooth_available) {
        std::cout << "Audio: Bluetooth stack detected" << std::endl;
        
        // Simple Bluetooth setup for Pi 3B+ BCM43438
        std::cout << "Audio: Configuring Bluetooth for BCM43438 (Pi 3B+)..." << std::endl;
        
        // Enable Bluetooth
        executeCommand("sudo systemctl enable bluetooth");
        executeCommand("sudo systemctl start bluetooth");
        
        // Basic configuration to avoid WiFi conflicts
        executeCommand("sudo hciconfig hci0 up");
        executeCommand("sudo hciconfig hci0 name TazzariAudio");
        
        // Make discoverable (simple approach)
        executeCommand("bluetoothctl power on");
        executeCommand("bluetoothctl discoverable on"); 
        executeCommand("bluetoothctl pairable on");
        
        std::cout << "Audio: Bluetooth configured as 'TazzariAudio'" << std::endl;
    } else {
        std::cout << "Audio: Bluetooth not available" << std::endl;
    }
    
    current_info.volume = current_volume;
    
    std::cout << "Audio: Initialization complete" << std::endl;
    return true;
}

void SimplifiedAudioManager::shutdown() {
    std::cout << "Audio: Shutting down..." << std::endl;
}

bool SimplifiedAudioManager::executeCommand(const std::string& command) {
    int result = system(command.c_str());
    return (result == 0);
}

bool SimplifiedAudioManager::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    current_volume = volume;
    current_info.volume = volume;
    
    bool success = false;
    
    if (beocreate_available) {
        success = setBeoCreateVolume(volume);
    }
    
    // Fallback to system volume
    if (!success) {
        std::string cmd = "amixer set Master " + std::to_string(volume) + "%";
        success = executeCommand(cmd);
    }
    
    if (success) {
        std::cout << "Audio: Volume set to " << volume << "%" << std::endl;
    }
    
    return success;
}

int SimplifiedAudioManager::getVolume() {
    if (beocreate_available) {
        return getBeoCreateVolume();
    }
    return current_volume;
}

bool SimplifiedAudioManager::setBeoCreateVolume(int volume) {
    // Method 1: Try ALSA DSPVolume control
    std::string alsa_cmd = "amixer sset DSPVolume " + std::to_string(volume) + "%";
    if (executeCommand(alsa_cmd)) {
        std::cout << "Audio: BeoCreate ALSA volume set to " << volume << "%" << std::endl;
        return true;
    }
    
    // Method 2: Try beocreate-client (direct DSP register control)
    // This requires finding the correct register address for your DSP program
    // Common volume register addresses for BeoCreate: 0x0076, 0x0077
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        // Convert volume percentage to DSP value (0.0 to 1.0)
        float dsp_value = volume / 100.0f;
        std::string dsp_cmd = "beocreate-client localhost write_val 0x0076 " + std::to_string(dsp_value);
        
        if (executeCommand(dsp_cmd)) {
            std::cout << "Audio: BeoCreate DSP register volume set to " << volume << "%" << std::endl;
            return true;
        }
    }
    
    return false;
}

int SimplifiedAudioManager::getBeoCreateVolume() {
    // Try to read current DSP volume
    FILE* pipe = popen("amixer sget DSPVolume | grep -o '[0-9]*%' | head -1 | tr -d '%'", "r");
    if (pipe) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            int vol = std::atoi(buffer);
            pclose(pipe);
            if (vol >= 0 && vol <= 100) {
                current_volume = vol;
                return vol;
            }
        }
        pclose(pipe);
    }
    
    return current_volume;
}

bool SimplifiedAudioManager::setBass(int level) {
    // Placeholder for EQ control via BeoCreate DSP
    // Requires specific DSP program with EQ controls
    std::cout << "Audio: Bass EQ set to " << level << " (not implemented)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::setMid(int level) {
    std::cout << "Audio: Mid EQ set to " << level << " (not implemented)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::setHigh(int level) {
    std::cout << "Audio: High EQ set to " << level << " (not implemented)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::isBluetoothConnected() {
    if (!bluetooth_available) return false;
    
    // Simple check: count connected devices
    FILE* pipe = popen("bluetoothctl devices Connected 2>/dev/null | wc -l", "r");
    if (pipe) {
        char buffer[16];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            int count = std::atoi(buffer);
            pclose(pipe);
            current_info.connected = (count > 0);
            return current_info.connected;
        }
        pclose(pipe);
    }
    
    return false;
}

std::string SimplifiedAudioManager::getConnectedDevice() {
    if (!bluetooth_available) return "No Bluetooth";
    
    FILE* pipe = popen("bluetoothctl devices Connected 2>/dev/null | head -1 | cut -d' ' -f3-", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string device(buffer);
            device.erase(device.find_last_not_of(" \n\r\t") + 1); // trim
            pclose(pipe);
            if (!device.empty()) {
                current_info.device_name = device;
                return device;
            }
        }
        pclose(pipe);
    }
    
    return "No Device";
}

bool SimplifiedAudioManager::togglePlayPause() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Toggle play/pause" << std::endl;
    
    // Try simple bluetoothctl player control
    bool success = executeCommand("bluetoothctl << EOF\nplayer.play\nEOF");
    if (!success) {
        // Alternative: try pause
        success = executeCommand("bluetoothctl << EOF\nplayer.pause\nEOF");
    }
    
    return success;
}

bool SimplifiedAudioManager::nextTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Next track" << std::endl;
    return executeCommand("bluetoothctl << EOF\nplayer.next\nEOF");
}

bool SimplifiedAudioManager::previousTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Previous track" << std::endl;
    return executeCommand("bluetoothctl << EOF\nplayer.previous\nEOF");
}

bool SimplifiedAudioManager::checkBluetoothStatus() {
    return isBluetoothConnected();
}

void SimplifiedAudioManager::updateBluetoothInfo() {
    if (isBluetoothConnected()) {
        current_info.device_name = getConnectedDevice();
        current_info.connected = true;
        
        // Simple state detection (not very reliable, but minimal)
        current_info.state = SimplePlaybackState::UNKNOWN;
    } else {
        current_info.device_name = "No Device";
        current_info.connected = false;
        current_info.state = SimplePlaybackState::STOPPED;
    }
}

SimpleMediaInfo SimplifiedAudioManager::getMediaInfo() {
    return current_info;
}

void SimplifiedAudioManager::setStateCallback(std::function<void(const SimpleMediaInfo&)> callback) {
    state_callback = callback;
}

void SimplifiedAudioManager::update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();
    
    // Update every 10 seconds to reduce interference
    if (elapsed < 10) return;
    
    last_update = now;
    
    // Update Bluetooth info
    updateBluetoothInfo();
    
    // Update volume from DSP
    if (beocreate_available) {
        current_info.volume = getBeoCreateVolume();
    }
    
    // Call callback if registered
    if (state_callback) {
        state_callback(current_info);
    }
}