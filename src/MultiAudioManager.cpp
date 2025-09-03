// src/MultiAudioManager.cpp - Unified audio manager implementation

#include "MultiAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>
#include <cstring>
#include <alsa/asoundlib.h>
#include <pulse/pulseaudio.h>
#include <curl/curl.h>

// Callback for libcurl
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

// Base implementation class
class MultiAudioManager::BaseAudioImpl {
public:
    virtual ~BaseAudioImpl() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool setVolume(int volume) = 0;
    virtual int getVolume() = 0;
    virtual bool setBass(int level) = 0;
    virtual bool setMid(int level) = 0;
    virtual bool setHigh(int level) = 0;
    
protected:
    int current_volume = 50;
    int current_bass = 0;
    int current_mid = 0; 
    int current_high = 0;
};

// AUX/Built-in audio implementation using PulseAudio
class MultiAudioManager::AuxAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::cout << "Audio: Initializing built-in 3.5mm jack with PulseAudio..." << std::endl;
        
        // Check PulseAudio availability
        if (system("pulseaudio --check 2>/dev/null") != 0) {
            std::cout << "Audio: Starting PulseAudio..." << std::endl;
            system("pulseaudio --start 2>/dev/null");
        }
        
        // Install alsaeq for EQ if not present
        if (system("which alsaeq >/dev/null 2>&1") != 0) {
            std::cout << "Audio: Installing alsaeq for equalizer..." << std::endl;
            system("sudo apt-get install -y libasound2-plugin-equal 2>/dev/null");
        }
        
        setupAlsaEQ();
        
        std::cout << "Audio: ✓ Built-in audio ready with software EQ" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "Audio: Shutting down PulseAudio interface..." << std::endl;
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        std::string cmd = "pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(volume) + "%";
        bool success = (system(cmd.c_str()) == 0);
        
        if (success) {
            current_volume = volume;
            std::cout << "Audio: ✓ PulseAudio volume set to " << volume << "%" << std::endl;
        }
        
        return success;
    }
    
    int getVolume() override {
        FILE* pipe = popen("pactl get-sink-volume @DEFAULT_SINK@ | grep -o '[0-9]*%' | head -1 | tr -d '%'", "r");
        if (pipe) {
            char buffer[16];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                current_volume = std::atoi(buffer);
            }
            pclose(pipe);
        }
        return current_volume;
    }
    
    bool setBass(int level) override {
        current_bass = level;
        return setAlsaEQBand(0, level); // Band 0 = Bass (31Hz-125Hz)
    }
    
    bool setMid(int level) override {
        current_mid = level;
        return setAlsaEQBand(5, level); // Band 5 = Mid (1kHz)
    }
    
    bool setHigh(int level) override {
        current_high = level;
        return setAlsaEQBand(9, level); // Band 9 = High (8kHz-16kHz)
    }

private:
    void setupAlsaEQ() {
        // Create /etc/asound.conf for alsaeq
        std::ofstream asound("/tmp/asound.conf");
        asound << "pcm.!default {\n"
               << "  type plug\n"
               << "  slave.pcm plugequal;\n"
               << "}\n"
               << "ctl.!default {\n"
               << "  type hw\n"
               << "  card 0\n"
               << "}\n"
               << "ctl.equal {\n"
               << "  type equal;\n"
               << "}\n"
               << "pcm.plugequal {\n"
               << "  type equal;\n"
               << "  slave.pcm \"plughw:0,0\";\n"
               << "}\n"
               << "pcm.equal {\n"
               << "  type plug;\n"
               << "  slave.pcm plugequal;\n"
               << "}\n";
        asound.close();
        
        system("sudo cp /tmp/asound.conf /etc/asound.conf");
        system("rm /tmp/asound.conf");
    }
    
    bool setAlsaEQBand(int band, int level) {
        // alsaeq uses 10 bands with range -12dB to +12dB
        // Map our -10 to +10 range to alsaeq range
        int alsaeq_value = level; // Direct mapping for now
        
        std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + " " + std::to_string(alsaeq_value);
        bool success = (system(cmd.c_str()) == 0);
        
        if (success) {
            std::cout << "Audio: ✓ EQ band " << band << " set to " << level << "dB" << std::endl;
        }
        
        return success;
    }
};

// HiFiBerry DAC+/AMP4 implementation using ALSA
class MultiAudioManager::HiFiBerryAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::string hw_name = (AUDIO_HW == AudioHardware::HIFIBERRY_AMP4) ? "AMP4" : "DAC+";
        std::cout << "Audio: Initializing HiFiBerry " << hw_name << " with ALSA..." << std::endl;
        
        // Open ALSA mixer
        int result = snd_mixer_open(&mixer_handle, 0);
        if (result < 0) {
            std::cerr << "Audio: Failed to open ALSA mixer: " << snd_strerror(result) << std::endl;
            return false;
        }
        
        result = snd_mixer_attach(mixer_handle, "default");
        if (result < 0) {
            std::cerr << "Audio: Failed to attach mixer: " << snd_strerror(result) << std::endl;
            return false;
        }
        
        result = snd_mixer_selem_register(mixer_handle, NULL, NULL);
        if (result < 0) {
            std::cerr << "Audio: Failed to register mixer elements: " << snd_strerror(result) << std::endl;
            return false;
        }
        
        result = snd_mixer_load(mixer_handle);
        if (result < 0) {
            std::cerr << "Audio: Failed to load mixer elements: " << snd_strerror(result) << std::endl;
            return false;
        }
        
        // Find the "Digital" volume control
        snd_mixer_elem_t* elem = snd_mixer_first_elem(mixer_handle);
        while (elem) {
            const char* name = snd_mixer_selem_get_name(elem);
            if (name && strcmp(name, "Digital") == 0) {
                digital_elem = elem;
                break;
            }
            elem = snd_mixer_elem_next(elem);
        }
        
        if (!digital_elem) {
            std::cerr << "Audio: 'Digital' volume control not found" << std::endl;
            return false;
        }
        
        // Install alsaeq for EQ
        if (system("which alsaeq >/dev/null 2>&1") != 0) {
            std::cout << "Audio: Installing alsaeq for equalizer..." << std::endl;
            system("sudo apt-get install -y libasound2-plugin-equal 2>/dev/null");
        }
        
        setupHiFiBerryAlsaEQ();
        
        std::cout << "Audio: ✓ HiFiBerry " << hw_name << " ready with hardware volume + alsaeq" << std::endl;
        return true;
    }
    
    void shutdown() override {
        if (mixer_handle) {
            snd_mixer_close(mixer_handle);
            mixer_handle = nullptr;
        }
        std::cout << "Audio: HiFiBerry interface closed" << std::endl;
    }
    
    bool setVolume(int volume) override {
        if (!digital_elem) return false;
        
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        long min, max;
        snd_mixer_selem_get_playback_volume_range(digital_elem, &min, &max);
        
        long alsa_volume = min + (long)((max - min) * volume / 100.0);
        
        int result = snd_mixer_selem_set_playback_volume_all(digital_elem, alsa_volume);
        if (result < 0) {
            std::cerr << "Audio: Failed to set volume: " << snd_strerror(result) << std::endl;
            return false;
        }
        
        current_volume = volume;
        std::cout << "Audio: ✓ HiFiBerry hardware volume set to " << volume << "%" << std::endl;
        return true;
    }
    
    int getVolume() override {
        if (!digital_elem) return current_volume;
        
        long vol, min, max;
        snd_mixer_selem_get_playback_volume_range(digital_elem, &min, &max);
        snd_mixer_selem_get_playback_volume(digital_elem, SND_MIXER_SCHN_MONO, &vol);
        
        current_volume = (int)((vol - min) * 100 / (max - min));
        return current_volume;
    }
    
    bool setBass(int level) override {
        current_bass = level;
        return setAlsaEQBand(0, level);
    }
    
    bool setMid(int level) override {
        current_mid = level;
        return setAlsaEQBand(5, level);
    }
    
    bool setHigh(int level) override {
        current_high = level;
        return setAlsaEQBand(9, level);
    }

private:
    snd_mixer_t* mixer_handle = nullptr;
    snd_mixer_elem_t* digital_elem = nullptr;
    
    void setupHiFiBerryAlsaEQ() {
        // Create /etc/asound.conf for HiFiBerry with alsaeq
        std::ofstream asound("/tmp/asound.conf");
        asound << "pcm.!default {\n"
               << "  type plug\n"
               << "  slave.pcm plugequal;\n"
               << "}\n"
               << "ctl.!default {\n"
               << "  type hw\n"
               << "  card 0\n"
               << "}\n"
               << "ctl.equal {\n"
               << "  type equal;\n"
               << "}\n"
               << "pcm.plugequal {\n"
               << "  type equal;\n"
               << "  slave.pcm \"plughw:0,0\"; # HiFiBerry device\n"
               << "}\n"
               << "pcm.equal {\n"
               << "  type plug;\n"
               << "  slave.pcm plugequal;\n"
               << "}\n";
        asound.close();
        
        system("sudo cp /tmp/asound.conf /etc/asound.conf");
        system("rm /tmp/asound.conf");
    }
    
    bool setAlsaEQBand(int band, int level) {
        std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + " " + std::to_string(level);
        bool success = (system(cmd.c_str()) == 0);
        
        if (success) {
            std::cout << "Audio: ✓ HiFiBerry EQ band " << band << " set to " << level << "dB" << std::endl;
        }
        
        return success;
    }
};

// BeoCreate 4 implementation (existing SimplifiedAudioManager code)
class MultiAudioManager::BeoCreateAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::cout << "Audio: Initializing BeoCreate 4 DSP + REST API..." << std::endl;
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // Test DSP REST API connection with retry
        for (int i = 0; i < 5; i++) {
            if (testRestApiConnection()) {
                dsp_rest_api_available = true;
                std::cout << "Audio: ✓ BeoCreate 4 DSP REST API connected" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        if (dsp_rest_api_available) {
            // Get volume register from profile metadata
            std::string metadata_response;
            if (makeRestApiCall("GET", "/metadata", "", &metadata_response)) {
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
            setVolume(current_volume);
        } else {
            std::cout << "Audio: ✗ BeoCreate 4 DSP REST API not available" << std::endl;
        }
        
        return true;
    }
    
    void shutdown() override {
        curl_global_cleanup();
        std::cout << "Audio: BeoCreate 4 interface shut down" << std::endl;
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        current_volume = volume;
        
        if (dsp_rest_api_available) {
            return setDSPVolume(volume);
        }
        
        std::cout << "Audio: Volume set to " << volume << "% (DSP not available)" << std::endl;
        return false;
    }
    
    int getVolume() override {
        if (dsp_rest_api_available) {
            return getDSPVolume();
        }
        return current_volume;
    }
    
    bool setBass(int level) override {
        current_bass = level;
        return setDSPEQ("bass", level);
    }
    
    bool setMid(int level) override {
        current_mid = level;
        return setDSPEQ("mid", level);
    }
    
    bool setHigh(int level) override {
        current_high = level;
        return setDSPEQ("high", level);
    }

private:
    bool dsp_rest_api_available = false;
    std::string rest_api_base_url = "http://localhost:13141";
    std::string volume_register = "volumeControlRegister";
    
    bool makeRestApiCall(const std::string& method, const std::string& endpoint, 
                        const std::string& data, std::string* response) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        std::string url = rest_api_base_url + endpoint;
        std::string response_buffer;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
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
    
    bool testRestApiConnection() {
        std::string response;
        return makeRestApiCall("GET", "/checksum", "", &response);
    }
    
    bool setDSPVolume(int volume) {
        float dsp_value = volume / 100.0f;
        
        std::ostringstream json;
        json << "{"
             << "\"address\":\"" << volume_register << "\","
             << "\"value\":" << dsp_value
             << "}";
        
        std::string response;
        bool success = makeRestApiCall("POST", "/memory", json.str(), &response);
        
        if (success) {
            std::cout << "Audio: ✓ BeoCreate 4 DSP volume set to " << volume << "%" << std::endl;
        }
        
        return success;
    }
    
    int getDSPVolume() {
        std::string endpoint = "/memory/" + volume_register + "?format=float";
        std::string response;
        
        if (makeRestApiCall("GET", endpoint, "", &response)) {
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
    
    bool setDSPEQ(const std::string& band, int level) {
        std::string eq_address;
        if (band == "bass") {
            eq_address = "eq1_band1";
        } else if (band == "mid") {
            eq_address = "eq1_band3";
        } else if (band == "high") {
            eq_address = "eq1_band5";
        } else {
            return false;
        }
        
        float db_value = level;
        
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
            std::cout << "Audio: ✓ BeoCreate 4 " << band << " EQ set to " << level << "dB" << std::endl;
        }
        
        return success;
    }
};

// MultiAudioManager implementation
MultiAudioManager::MultiAudioManager() {
    last_update = std::chrono::steady_clock::now();
}

MultiAudioManager::~MultiAudioManager() {
    shutdown();
}

bool MultiAudioManager::initialize() {
    std::cout << "Audio: Initializing " << getHardwareName() << "..." << std::endl;
    
    // Create appropriate implementation based on compile-time configuration
    if constexpr (AUDIO_HW == AudioHardware::AUX_BUILTIN) {
        impl = std::make_unique<AuxAudioImpl>();
    } else if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_DAC || 
                        AUDIO_HW == AudioHardware::HIFIBERRY_AMP4) {
        impl = std::make_unique<HiFiBerryAudioImpl>();
    } else if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_BEOCREATE4) {
        impl = std::make_unique<BeoCreateAudioImpl>();
    }
    
    bool success = impl->initialize();
    
    if (success) {
        current_info.volume = impl->getVolume();
        std::cout << "Audio: ✓ " << getHardwareName() << " initialized" << std::endl;
    }
    
    return success;
}

void MultiAudioManager::shutdown() {
    if (impl) {
        impl->shutdown();
    }
}

bool MultiAudioManager::setVolume(int volume) {
    if (!impl) return false;
    
    bool success = impl->setVolume(volume);
    if (success) {
        current_info.volume = volume;
    }
    return success;
}

int MultiAudioManager::getVolume() {
    if (!impl) return current_info.volume;
    
    int volume = impl->getVolume();
    current_info.volume = volume;
    return volume;
}

bool MultiAudioManager::setBass(int level) {
    return impl ? impl->setBass(level) : false;
}

bool MultiAudioManager::setMid(int level) {
    return impl ? impl->setMid(level) : false;
}

bool MultiAudioManager::setHigh(int level) {
    return impl ? impl->setHigh(level) : false;
}

// Bluetooth methods (common to all implementations)
bool MultiAudioManager::isBluetoothConnected() {
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

std::string MultiAudioManager::getConnectedDevice() {
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

bool MultiAudioManager::togglePlayPause() {
    std::string command;
    bool success = false;
    
    if (internal_playing_state) {
        command = "timeout 3s bluetoothctl << EOF >/dev/null 2>&1\nplayer.pause\nEOF";
        success = (system(command.c_str()) == 0);
        
        if (success) {
            internal_playing_state = false;
            current_info.state = SimplePlaybackState::PAUSED;
        }
    } else {
        command = "timeout 3s bluetoothctl << EOF >/dev/null 2>&1\nplayer.play\nEOF";
        success = (system(command.c_str()) == 0);
        
        if (success) {
            internal_playing_state = true;
            current_info.state = SimplePlaybackState::PLAYING;
        }
    }
    
    return success;
}

bool MultiAudioManager::nextTrack() {
    return (system("echo 'player.next' | bluetoothctl > /dev/null 2>&1") == 0);
}

bool MultiAudioManager::previousTrack() {
    return (system("echo 'player.previous' | bluetoothctl > /dev/null 2>&1") == 0);
}

SimpleMediaInfo MultiAudioManager::getMediaInfo() {
    return current_info;
}

void MultiAudioManager::setStateCallback(std::function<void(const SimpleMediaInfo&)> callback) {
    state_callback = callback;
}

void MultiAudioManager::update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();
    
    if (elapsed < 10) return; // Update every 10 seconds
    
    last_update = now;
    
    // Update Bluetooth info
    updateBluetoothInfo();
    
    // Update volume from hardware
    if (impl) {
        current_info.volume = impl->getVolume();
    }
    
    // Call callback if registered
    if (state_callback) {
        state_callback(current_info);
    }
}

bool MultiAudioManager::checkBluetoothStatus() {
    return isBluetoothConnected();
}

void MultiAudioManager::updateBluetoothInfo() {
    bool was_connected = current_info.connected;
    
    if (isBluetoothConnected()) {
        current_info.device_name = getConnectedDevice();
        current_info.connected = true;
        
        // Reset internal state when device connects for the first time
        if (!was_connected) {
            internal_playing_state = false;
            current_info.state = SimplePlaybackState::STOPPED;
            std::cout << "Audio: Bluetooth connected - reset playback state" << std::endl;
        }
        
        // Get media metadata (may not work on iOS, but try anyway)
        updateMediaMetadata();
        
    } else {
        current_info.device_name = "No Device";
        current_info.connected = false;
        current_info.state = SimplePlaybackState::STOPPED;
        current_info.track_title = "";
        current_info.artist = "";
        current_info.album = "";
        
        // Reset internal state when disconnected
        internal_playing_state = false;
        
        if (was_connected) {
            std::cout << "Audio: Bluetooth disconnected - reset playback state" << std::endl;
        }
    }
}

void MultiAudioManager::updateMediaMetadata() {
    // Get current track info from bluetoothctl
    FILE* pipe = popen("echo 'info' | bluetoothctl 2>/dev/null | grep -E '(Title|Artist|Album):'", "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            
            if (line.find("Title:") != std::string::npos) {
                size_t pos = line.find("Title:") + 6;
                current_info.track_title = line.substr(pos);
                current_info.track_title.erase(current_info.track_title.find_last_not_of(" \n\r\t") + 1);
            } else if (line.find("Artist:") != std::string::npos) {
                size_t pos = line.find("Artist:") + 7;
                current_info.artist = line.substr(pos);
                current_info.artist.erase(current_info.artist.find_last_not_of(" \n\r\t") + 1);
            } else if (line.find("Album:") != std::string::npos) {
                size_t pos = line.find("Album:") + 6;
                current_info.album = line.substr(pos);
                current_info.album.erase(current_info.album.find_last_not_of(" \n\r\t") + 1);
            }
        }
        pclose(pipe);
    }
    
    // Debug output for media info
    if (!current_info.track_title.empty()) {
        static std::string last_track = "";
        if (last_track != current_info.track_title) {
            std::cout << "Audio: Now playing - " << current_info.artist << " - " << current_info.track_title << std::endl;
            last_track = current_info.track_title;
        }
    }
}

// Static methods for hardware information
std::string MultiAudioManager::getHardwareName() {
    if constexpr (AUDIO_HW == AudioHardware::AUX_BUILTIN) {
        return "Built-in 3.5mm Jack";
    } else if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_DAC) {
        return "HiFiBerry DAC+";
    } else if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_AMP4) {
        return "HiFiBerry AMP4";
    } else if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_BEOCREATE4) {
        return "HiFiBerry BeoCreate 4";
    }
    return "Unknown Audio Hardware";
}

bool MultiAudioManager::hasHardwareVolume() {
    if constexpr (AUDIO_HW == AudioHardware::AUX_BUILTIN) {
        return false; // PulseAudio software volume
    } else {
        return true; // All HiFiBerry cards have hardware volume
    }
}

bool MultiAudioManager::hasHardwareEQ() {
    if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_BEOCREATE4) {
        return true; // BeoCreate 4 has DSP EQ
    } else {
        return false; // Others use alsaeq software EQ
    }
}