#include "SimplifiedAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>
#include <cstring>

SimplifiedAudioManager::SimplifiedAudioManager() {
    last_update = std::chrono::steady_clock::now();
}

SimplifiedAudioManager::~SimplifiedAudioManager() {
    shutdown();
}

bool SimplifiedAudioManager::initialize() {
    std::cout << "Audio: Initializing Simplified Audio Manager..." << std::endl;
    
    // Check for BeoCreate DSP tools (in priority order)
    beocreate_available = false;
    
    // Method 1: Check for beocreate-client (best - direct DSP control)
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        beocreate_available = true;
        std::cout << "Audio: BeoCreate DSP client tools detected" << std::endl;
        
        // Test if SigmaTCP server is running
        if (system("systemctl is-active beocreate-tcp > /dev/null 2>&1") == 0) {
            std::cout << "Audio: SigmaTCP server is running" << std::endl;
        } else {
            std::cout << "Audio: Starting SigmaTCP server..." << std::endl;
            system("sudo systemctl start beocreate-tcp > /dev/null 2>&1");
        }
    }
    // Method 2: Check for ALSA DSPVolume control
    else if (system("which amixer > /dev/null 2>&1") == 0 && 
             system("amixer sget DSPVolume > /dev/null 2>&1") == 0) {
        beocreate_available = true;
        std::cout << "Audio: BeoCreate ALSA DSPVolume control detected" << std::endl;
    }
    // Method 3: Basic ALSA fallback
    else if (system("which amixer > /dev/null 2>&1") == 0) {
        std::cout << "Audio: Using ALSA Master volume control (no DSP)" << std::endl;
        beocreate_available = false;
    }
    else {
        std::cout << "Audio: No audio control methods available" << std::endl;
        beocreate_available = false;
    }
    
    // Set initial volume
    setVolume(current_volume);
    
    // Check Bluetooth
    bluetooth_available = (system("which bluetoothctl > /dev/null 2>&1") == 0);
    if (bluetooth_available) {
        std::cout << "Audio: Bluetooth stack detected" << std::endl;
        
        // Start Bluetooth services if not running
        system("sudo systemctl start bluetooth > /dev/null 2>&1");
        system("sudo systemctl start simple-bluetooth > /dev/null 2>&1");
        
        std::cout << "Audio: Pi configured as 'TazzariAudio'" << std::endl;
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
    
    // Fallback to ALSA Master volume
    if (!success) {
        std::string cmd = "amixer sset Master " + std::to_string(volume) + "% > /dev/null 2>&1";
        success = executeCommand(cmd);
        if (success) {
            std::cout << "Audio: ALSA Master volume set to " << volume << "%" << std::endl;
        }
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
    // Method 1: Try beocreate-client (direct DSP register control)
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        // Convert volume percentage to DSP value (0.0 to 1.0)
        float dsp_value = volume / 100.0f;
        
        // Common BeoCreate volume register addresses
        // Note: This may need adjustment based on your specific DSP program
        std::vector<std::string> volume_registers = {"0x0076", "0x0077", "0x0078"};
        
        for (const auto& reg : volume_registers) {
            std::string dsp_cmd = "beocreate-client localhost write_val " + reg + " " + std::to_string(dsp_value) + " > /dev/null 2>&1";
            if (executeCommand(dsp_cmd)) {
                std::cout << "Audio: BeoCreate DSP volume set to " << volume << "% (register " << reg << ")" << std::endl;
                return true;
            }
        }
        
        std::cout << "Audio: BeoCreate DSP register write failed, trying ALSA..." << std::endl;
    }
    
    // Method 2: Try ALSA DSPVolume control
    std::string alsa_cmd = "amixer sset DSPVolume " + std::to_string(volume) + "% > /dev/null 2>&1";
    if (executeCommand(alsa_cmd)) {
        std::cout << "Audio: BeoCreate ALSA DSPVolume set to " << volume << "%" << std::endl;
        return true;
    }
    
    return false;
}

int SimplifiedAudioManager::getBeoCreateVolume() {
    // Try to read from BeoCreate DSP first
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        // Try reading from common volume registers
        std::vector<std::string> volume_registers = {"0x0076", "0x0077", "0x0078"};
        
        for (const auto& reg : volume_registers) {
            std::string read_cmd = "beocreate-client localhost read_dec " + reg + " 4 2>/dev/null";
            FILE* pipe = popen(read_cmd.c_str(), "r");
            if (pipe) {
                char buffer[64];
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    float dsp_value = std::atof(buffer);
                    pclose(pipe);
                    if (dsp_value >= 0.0 && dsp_value <= 1.0) {
                        int vol = (int)(dsp_value * 100);
                        current_volume = vol;
                        return vol;
                    }
                }
                pclose(pipe);
            }
        }
    }
    
    // Fallback: Try ALSA DSPVolume
    FILE* pipe = popen("amixer sget DSPVolume 2>/dev/null | grep -o '[0-9]*%' | head -1 | tr -d '%'", "r");
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
    // Example: EQ control via BeoCreate DSP (register addresses depend on your DSP program)
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        // Convert -10 to +10 range to DSP value
        float dsp_value = (level + 10) / 20.0f; // Convert to 0.0-1.0 range
        std::string cmd = "beocreate-client localhost write_val 0x0080 " + std::to_string(dsp_value) + " > /dev/null 2>&1";
        if (executeCommand(cmd)) {
            std::cout << "Audio: Bass EQ set to " << level << " via DSP" << std::endl;
            return true;
        }
    }
    
    std::cout << "Audio: Bass EQ set to " << level << " (DSP control not available)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::setMid(int level) {
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        float dsp_value = (level + 10) / 20.0f;
        std::string cmd = "beocreate-client localhost write_val 0x0081 " + std::to_string(dsp_value) + " > /dev/null 2>&1";
        if (executeCommand(cmd)) {
            std::cout << "Audio: Mid EQ set to " << level << " via DSP" << std::endl;
            return true;
        }
    }
    
    std::cout << "Audio: Mid EQ set to " << level << " (DSP control not available)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::setHigh(int level) {
    if (system("which beocreate-client > /dev/null 2>&1") == 0) {
        float dsp_value = (level + 10) / 20.0f;
        std::string cmd = "beocreate-client localhost write_val 0x0082 " + std::to_string(dsp_value) + " > /dev/null 2>&1";
        if (executeCommand(cmd)) {
            std::cout << "Audio: High EQ set to " << level << " via DSP" << std::endl;
            return true;
        }
    }
    
    std::cout << "Audio: High EQ set to " << level << " (DSP control not available)" << std::endl;
    return false;
}

bool SimplifiedAudioManager::isBluetoothConnected() {
    if (!bluetooth_available) return false;
    
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
            device.erase(device.find_last_not_of(" \n\r\t") + 1);
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
    return executeCommand("echo 'player.play' | bluetoothctl > /dev/null 2>&1");
}

bool SimplifiedAudioManager::nextTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Next track" << std::endl;
    return executeCommand("echo 'player.next' | bluetoothctl > /dev/null 2>&1");
}

bool SimplifiedAudioManager::previousTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Previous track" << std::endl;
    return executeCommand("echo 'player.previous' | bluetoothctl > /dev/null 2>&1");
}

bool SimplifiedAudioManager::checkBluetoothStatus() {
    return isBluetoothConnected();
}

void SimplifiedAudioManager::updateBluetoothInfo() {
    if (isBluetoothConnected()) {
        current_info.device_name = getConnectedDevice();
        current_info.connected = true;
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
    
    // Update every 10 seconds to reduce system load
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