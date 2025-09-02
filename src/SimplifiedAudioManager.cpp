#include "SimplifiedAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>
#include <cstring>
#include <curl/curl.h>

// Callback for libcurl to write response data
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

SimplifiedAudioManager::SimplifiedAudioManager() {
    last_update = std::chrono::steady_clock::now();
}

SimplifiedAudioManager::~SimplifiedAudioManager() {
    shutdown();
}

bool SimplifiedAudioManager::initialize() {
    std::cout << "Audio: Initializing BeoCreate 4 DSP + Bluetooth Audio Manager..." << std::endl;
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Test DSP REST API connection
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for sigmatcpserver
    
    if (testRestApiConnection()) {
        dsp_rest_api_available = true;
        std::cout << "Audio: ✓ BeoCreate 4 DSP REST API connected" << std::endl;
        
        // Get volume register from profile metadata
        std::string metadata_response;
        if (makeRestApiCall("GET", "/metadata", "", &metadata_response)) {
            // Parse JSON to find volumeControlRegister
            // Simple string search for now (could use proper JSON parser)
            size_t pos = metadata_response.find("\"volumeControlRegister\":");
            if (pos != std::string::npos) {
                size_t start = metadata_response.find("\"", pos + 24);
                size_t end = metadata_response.find("\"", start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    volume_register = metadata_response.substr(start + 1, end - start - 1);
                    std::cout << "Audio: Volume register: " << volume_register << std::endl;
                }
            }
        }
        
        // Set initial volume
        setVolume(current_volume);
        
    } else {
        std::cout << "Audio: ✗ BeoCreate 4 DSP REST API not available" << std::endl;
        dsp_rest_api_available = false;
    }
    
    // Check Bluetooth
    bluetooth_available = (system("which bluetoothctl > /dev/null 2>&1") == 0);
    if (bluetooth_available) {
        std::cout << "Audio: ✓ Bluetooth stack detected" << std::endl;
        std::cout << "Audio: Pi configured as 'TazzariAudio' A2DP sink" << std::endl;
    } else {
        std::cout << "Audio: ✗ Bluetooth not available" << std::endl;
    }
    
    current_info.volume = current_volume;
    
    std::cout << "Audio: Initialization complete" << std::endl;
    return dsp_rest_api_available || bluetooth_available;
}

void SimplifiedAudioManager::shutdown() {
    std::cout << "Audio: Shutting down..." << std::endl;
    curl_global_cleanup();
}

bool SimplifiedAudioManager::makeRestApiCall(const std::string& method, const std::string& endpoint, 
                                              const std::string& data, std::string* response) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string url = rest_api_base_url + endpoint;
    std::string response_buffer;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 second timeout
    
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
    }
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK && response) {
        *response = response_buffer;
    }
    
    return (res == CURLE_OK);
}

bool SimplifiedAudioManager::testRestApiConnection() {
    std::string response;
    return makeRestApiCall("GET", "/checksum", "", &response);
}

bool SimplifiedAudioManager::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    current_volume = volume;
    current_info.volume = volume;
    
    if (dsp_rest_api_available) {
        return setDSPVolume(volume);
    }
    
    std::cout << "Audio: Volume set to " << volume << "% (DSP not available)" << std::endl;
    return false;
}

int SimplifiedAudioManager::getVolume() {
    if (dsp_rest_api_available) {
        return getDSPVolume();
    }
    return current_volume;
}

bool SimplifiedAudioManager::setDSPVolume(int volume) {
    if (!dsp_rest_api_available) return false;
    
    // Convert percentage to DSP value (0.0 to 1.0)
    float dsp_value = volume / 100.0f;
    
    // Create JSON payload for memory write
    std::ostringstream json;
    json << "{"
         << "\"address\":\"" << volume_register << "\","
         << "\"value\":" << dsp_value
         << "}";
    
    std::string response;
    bool success = makeRestApiCall("POST", "/memory", json.str(), &response);
    
    if (success) {
        std::cout << "Audio: ✓ BeoCreate 4 DSP volume set to " << volume << "%" << std::endl;
    } else {
        std::cout << "Audio: ✗ Failed to set DSP volume" << std::endl;
    }
    
    return success;
}

int SimplifiedAudioManager::getDSPVolume() {
    if (!dsp_rest_api_available) return current_volume;
    
    // Read current volume from DSP
    std::string endpoint = "/memory/" + volume_register + "?format=float";
    std::string response;
    
    if (makeRestApiCall("GET", endpoint, "", &response)) {
        // Parse JSON response to extract volume value
        // Simple parsing - look for "values":[x.x]
        size_t pos = response.find("\"values\":[");
        if (pos != std::string::npos) {
            size_t start = pos + 10;
            size_t end = response.find("]", start);
            if (end != std::string::npos) {
                std::string value_str = response.substr(start, end - start);
                float dsp_value = std::stof(value_str);
                int volume = (int)(dsp_value * 100);
                if (volume >= 0 && volume <= 100) {
                    current_volume = volume;
                    return volume;
                }
            }
        }
    }
    
    return current_volume;
}

bool SimplifiedAudioManager::setBass(int level) {
    if (level < -10) level = -10;
    if (level > 10) level = 10;
    
    return setDSPEQ("bass", level);
}

bool SimplifiedAudioManager::setMid(int level) {
    if (level < -10) level = -10;
    if (level > 10) level = 10;
    
    return setDSPEQ("mid", level);
}

bool SimplifiedAudioManager::setHigh(int level) {
    if (level < -10) level = -10;
    if (level > 10) level = 10;
    
    return setDSPEQ("high", level);
}

bool SimplifiedAudioManager::setDSPEQ(const std::string& band, int level) {
    if (!dsp_rest_api_available) {
        std::cout << "Audio: " << band << " EQ set to " << level << " (DSP not available)" << std::endl;
        return false;
    }
    
    // For BeoCreate Universal profile, we need to find the appropriate EQ registers
    // This is a simplified approach - you may need to adjust based on your specific DSP program
    
    std::string eq_address;
    if (band == "bass") {
        eq_address = "eq1_band1"; // Low frequency band
    } else if (band == "mid") {
        eq_address = "eq1_band3"; // Mid frequency band  
    } else if (band == "high") {
        eq_address = "eq1_band5"; // High frequency band
    } else {
        return false;
    }
    
    // Convert EQ level (-10 to +10) to dB value
    float db_value = level; // Direct dB value
    
    // Create biquad filter for peaking EQ
    std::ostringstream json;
    json << "{"
         << "\"address\":\"" << eq_address << "\","
         << "\"offset\":0,"
         << "\"filter\":{"
         << "\"type\":\"PeakingEq\","
         << "\"f\":" << (band == "bass" ? 100 : (band == "mid" ? 1000 : 10000)) << ","
         << "\"db\":" << db_value << ","
         << "\"q\":1.0"
         << "}"
         << "}";
    
    std::string response;
    bool success = makeRestApiCall("POST", "/biquad", json.str(), &response);
    
    if (success) {
        std::cout << "Audio: ✓ " << band << " EQ set to " << level << "dB via DSP" << std::endl;
    } else {
        std::cout << "Audio: ✗ Failed to set " << band << " EQ" << std::endl;
    }
    
    return success;
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
    return (system("echo 'player.play' | bluetoothctl > /dev/null 2>&1") == 0);
}

bool SimplifiedAudioManager::nextTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Next track" << std::endl;
    return (system("echo 'player.next' | bluetoothctl > /dev/null 2>&1") == 0);
}

bool SimplifiedAudioManager::previousTrack() {
    if (!bluetooth_available) return false;
    
    std::cout << "Audio: Previous track" << std::endl;
    return (system("echo 'player.previous' | bluetoothctl > /dev/null 2>&1") == 0);
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
    if (dsp_rest_api_available) {
        current_info.volume = getDSPVolume();
    }
    
    // Call callback if registered
    if (state_callback) {
        state_callback(current_info);
    }
}