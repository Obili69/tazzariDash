// src/MultiAudioManager.cpp - Unified audio manager implementation

#include "MultiAudioManager.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <thread>
#include <cstring>

// Conditional includes based on audio hardware
#ifdef AUDIO_HARDWARE_AUX
    // No additional includes needed for AUX
#endif

#if defined(AUDIO_HARDWARE_DAC) || defined(AUDIO_HARDWARE_AMP4)
    #include <alsa/asoundlib.h>
#endif

#ifdef AUDIO_HARDWARE_BEOCREATE4
    #include <curl/curl.h>
    
    // Callback for libcurl (only needed for BeoCreate 4)
    size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response) {
        size_t total_size = size * nmemb;
        response->append((char*)contents, total_size);
        return total_size;
    }
#endif

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

// AUX/Built-in audio implementation using system commands
#ifdef AUDIO_HARDWARE_AUX
class MultiAudioManager::AuxAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::cout << "Audio: Initializing built-in 3.5mm jack..." << std::endl;
        
        try {
            // Check if PulseAudio is available
            if (system("pactl info >/dev/null 2>&1") != 0) {
                std::cout << "Audio: Starting PulseAudio..." << std::endl;
                system("pulseaudio --start >/dev/null 2>&1");
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            
            // Try to set up alsaeq (non-critical)
            setupAlsaEQSafe();
            
            std::cout << "Audio: ✓ Built-in audio ready" << std::endl;
            return true;
            
        } catch (...) {
            std::cout << "Audio: Exception during AUX initialization" << std::endl;
            return false;
        }
    }
    
    void shutdown() override {
        std::cout << "Audio: AUX interface shut down" << std::endl;
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        try {
            std::string cmd = "pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(volume) + "% 2>/dev/null";
            bool success = (system(cmd.c_str()) == 0);
            
            if (success) {
                current_volume = volume;
                std::cout << "Audio: ✓ Volume set to " << volume << "%" << std::endl;
            } else {
                std::cout << "Audio: Warning - volume command failed, setting internal value" << std::endl;
                current_volume = volume;
            }
            
            return true; // Always return true for AUX (non-critical)
            
        } catch (...) {
            current_volume = volume;
            return true;
        }
    }
    
    int getVolume() override {
        try {
            FILE* pipe = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | grep -o '[0-9]*%' | head -1 | tr -d '%'", "r");
            if (pipe) {
                char buffer[16];
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    int vol = std::atoi(buffer);
                    if (vol >= 0 && vol <= 100) {
                        current_volume = vol;
                    }
                }
                pclose(pipe);
            }
        } catch (...) {
            // Return current stored value on error
        }
        
        return current_volume;
    }
    
    bool setBass(int level) override {
        current_bass = level;
        return setEQBandSafe(0, level);
    }
    
    bool setMid(int level) override {
        current_mid = level;
        return setEQBandSafe(5, level);
    }
    
    bool setHigh(int level) override {
        current_high = level;
        return setEQBandSafe(9, level);
    }

private:
    bool eq_available = false;
    
    void setupAlsaEQSafe() {
        try {
            // Check if alsaeq is available
            if (system("dpkg -l | grep libasound2-plugin-equal >/dev/null 2>&1") == 0) {
                // Create user-level ALSA config
                std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
                std::string asound_path = home + "/.asoundrc";
                
                std::ofstream asound(asound_path);
                if (asound.is_open()) {
                    asound << "# TazzariAudio EQ config\n"
                           << "pcm.!default {\n"
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
                    
                    eq_available = true;
                    std::cout << "Audio: ALSA EQ configured" << std::endl;
                }
            } else {
                std::cout << "Audio: alsaeq not available, install with: sudo apt install libasound2-plugin-equal" << std::endl;
            }
        } catch (...) {
            eq_available = false;
        }
    }
    
    bool setEQBandSafe(int band, int level) {
        if (!eq_available) {
            std::cout << "Audio: EQ not available, setting ignored" << std::endl;
            return true; // Non-critical
        }
        
        try {
            std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + 
                            " " + std::to_string(level) + " 2>/dev/null";
            bool success = (system(cmd.c_str()) == 0);
            
            if (success) {
                std::cout << "Audio: ✓ EQ band " << band << " set to " << level << "dB" << std::endl;
            }
            
            return success;
        } catch (...) {
            return false;
        }
    }
};
#endif // AUDIO_HARDWARE_AUX

// HiFiBerry DAC+/AMP4 implementation using ALSA
#if defined(AUDIO_HARDWARE_DAC) || defined(AUDIO_HARDWARE_AMP4)
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
        
        setupHiFiBerryAlsaEQ();
        
        std::cout << "Audio: ✓ HiFiBerry " << hw_name << " ready" << std::endl;
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
        std::cout << "Audio: ✓ Hardware volume set to " << volume << "%" << std::endl;
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
        std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
        std::string asound_path = home + "/.asoundrc";
        
        std::ofstream asound(asound_path);
        asound << "# HiFiBerry with alsaeq\n"
               << "pcm.!default {\n"
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
    }
    
    bool setAlsaEQBand(int band, int level) {
        std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + " " + std::to_string(level) + " 2>/dev/null";
        bool success = (system(cmd.c_str()) == 0);
        
        if (success) {
            std::cout << "Audio: ✓ EQ band " << band << " set to " << level << "dB" << std::endl;
        }
        
        return success;
    }
};
#endif // AUDIO_HARDWARE_DAC || AUDIO_HARDWARE_AMP4

// BeoCreate 4 implementation (existing code)
#ifdef AUDIO_HARDWARE_BEOCREATE4
class MultiAudioManager::BeoCreateAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::cout << "Audio: Initializing BeoCreate 4 DSP..." << std::endl;
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // Test DSP REST API connection
        for (int i = 0; i < 5; i++) {
            if (testRestApiConnection()) {
                dsp_rest_api_available = true;
                std::cout << "Audio: ✓ BeoCreate 4 DSP connected" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        if (dsp_rest_api_available) {
            setVolume(current_volume);
        }
        
        return true;
    }
    
    void shutdown() override {
        curl_global_cleanup();
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        current_volume = volume;
        
        if (dsp_rest_api_available) {
            return setDSPVolume(volume);
        }
        
        return false;
    }
    
    int getVolume() override {
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
        json << "{\"address\":\"volumeControlRegister\",\"value\":" << dsp_value << "}";
        
        std::string response;
        return makeRestApiCall("POST", "/memory", json.str(), &response);
    }
    
    bool setDSPEQ(const std::string& band, int level) {
        // Simplified DSP EQ implementation
        return true;
    }
};
#endif // AUDIO_HARDWARE_BEOCREATE4

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
#ifdef AUDIO_HARDWARE_AUX
    if constexpr (AUDIO_HW == AudioHardware::AUX_BUILTIN) {
        impl = std::make_unique<AuxAudioImpl>();
    }
#endif

#if defined(AUDIO_HARDWARE_DAC) || defined(AUDIO_HARDWARE_AMP4)
    if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_DAC || 
                  AUDIO_HW == AudioHardware::HIFIBERRY_AMP4) {
        impl = std::make_unique<HiFiBerryAudioImpl>();
    }
#endif

#ifdef AUDIO_HARDWARE_BEOCREATE4
    if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_BEOCREATE4) {
        impl = std::make_unique<BeoCreateAudioImpl>();
    }
#endif
    
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
    if (internal_playing_state) {
        system("timeout 3s bluetoothctl << EOF >/dev/null 2>&1\nplayer.pause\nEOF");
        internal_playing_state = false;
        current_info.state = SimplePlaybackState::PAUSED;
    } else {
        system("timeout 3s bluetoothctl << EOF >/dev/null 2>&1\nplayer.play\nEOF");
        internal_playing_state = true;
        current_info.state = SimplePlaybackState::PLAYING;
    }
    return true;
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
    
    if (elapsed < 10) return;
    
    last_update = now;
    
    // Update Bluetooth info
    current_info.connected = isBluetoothConnected();
    if (current_info.connected) {
        current_info.device_name = getConnectedDevice();
    } else {
        current_info.device_name = "No Device";
        current_info.state = SimplePlaybackState::STOPPED;
        internal_playing_state = false;
    }
    
    // Update volume from hardware
    if (impl) {
        current_info.volume = impl->getVolume();
    }
    
    // Call callback if registered
    if (state_callback) {
        state_callback(current_info);
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
        return false;
    } else {
        return true;
    }
}

bool MultiAudioManager::hasHardwareEQ() {
    if constexpr (AUDIO_HW == AudioHardware::HIFIBERRY_BEOCREATE4) {
        return true;
    } else {
        return false;
    }
}