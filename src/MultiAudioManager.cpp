// src/MultiAudioManager.cpp - FIXED: No test tones + deployment optimizations

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
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: Fast init - Built-in audio" << std::endl;
#else
        std::cout << "Audio: Initializing built-in 3.5mm jack..." << std::endl;
#endif
        
        try {
            // Check if PulseAudio is available (silent in deployment)
            if (system("pactl info >/dev/null 2>&1") != 0) {
#ifndef DEPLOYMENT_BUILD
                std::cout << "Audio: Starting PulseAudio..." << std::endl;
#endif
                system("pulseaudio --start >/dev/null 2>&1");
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Reduced wait time
            }
            
            // Setup EQ only if not deployment build
#ifndef DEPLOYMENT_BUILD
            setupAlsaEQSafe();
#endif
            
#ifdef DEPLOYMENT_BUILD
            std::cout << "Audio: ✓ AUX ready" << std::endl;
#else
            std::cout << "Audio: ✓ Built-in audio ready" << std::endl;
#endif
            return true;
            
        } catch (...) {
            std::cout << "Audio: Exception during AUX initialization" << std::endl;
            return false;
        }
    }
    
    void shutdown() override {
#ifndef DEPLOYMENT_BUILD
        std::cout << "Audio: AUX interface shut down" << std::endl;
#endif
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        try {
            std::string cmd = "pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(volume) + "% 2>/dev/null";
            bool success = (system(cmd.c_str()) == 0);
            
            if (success) {
                current_volume = volume;
#ifndef DEPLOYMENT_BUILD
                std::cout << "Audio: ✓ Volume set to " << volume << "%" << std::endl;
#endif
            } else {
#ifndef DEPLOYMENT_BUILD
                std::cout << "Audio: Warning - volume command failed, setting internal value" << std::endl;
#endif
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
            }
        } catch (...) {
            eq_available = false;
        }
    }
    
    bool setEQBandSafe(int band, int level) {
        if (!eq_available) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "Audio: EQ not available, setting ignored" << std::endl;
#endif
            return true; // Non-critical
        }
        
        try {
            std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + 
                            " " + std::to_string(level) + " 2>/dev/null";
            bool success = (system(cmd.c_str()) == 0);
            
#ifndef DEPLOYMENT_BUILD
            if (success) {
                std::cout << "Audio: ✓ EQ band " << band << " set to " << level << "dB" << std::endl;
            }
#endif
            
            return success;
        } catch (...) {
            return false;
        }
    }
};
#endif // AUDIO_HARDWARE_AUX

// HiFiBerry DAC+/AMP4 implementation - FIXED: No test tones
#if defined(AUDIO_HARDWARE_DAC) || defined(AUDIO_HARDWARE_AMP4)
class MultiAudioManager::HiFiBerryAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
        std::string hw_name = (AUDIO_HW == AudioHardware::HIFIBERRY_AMP4) ? "AMP4" : "DAC+";
        
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: Fast init - " << hw_name << std::endl;
#else
        std::cout << "Audio: Initializing HiFiBerry " << hw_name << " with ALSA..." << std::endl;
#endif
        
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
        
        // FIXED: Try multiple control names instead of just "Digital"
        const char* control_names[] = {"Digital", "Master", "PCM", "Speaker", "Headphone", NULL};
        
#ifndef DEPLOYMENT_BUILD
        std::cout << "Audio: Available mixer controls:" << std::endl;
        snd_mixer_elem_t* elem = snd_mixer_first_elem(mixer_handle);
        while (elem) {
            const char* name = snd_mixer_selem_get_name(elem);
            if (name) {
                std::cout << "  - " << name << std::endl;
            }
            elem = snd_mixer_elem_next(elem);
        }
#endif
        
        // Find a working volume control
        for (int i = 0; control_names[i] != NULL; i++) {
            snd_mixer_elem_t* elem = snd_mixer_first_elem(mixer_handle);
            while (elem) {
                const char* name = snd_mixer_selem_get_name(elem);
                if (name && strcmp(name, control_names[i]) == 0) {
                    // Check if this control has playback volume capability
                    if (snd_mixer_selem_has_playback_volume(elem)) {
                        volume_elem = elem;
                        volume_control_name = control_names[i];
#ifdef DEPLOYMENT_BUILD
                        std::cout << "Audio: ✓ Volume: " << volume_control_name << std::endl;
#else
                        std::cout << "Audio: ✓ Using '" << volume_control_name << "' for volume control" << std::endl;
#endif
                        break;
                    }
                }
                elem = snd_mixer_elem_next(elem);
            }
            if (volume_elem) break;
        }
        
        if (!volume_elem) {
            std::cerr << "Audio: No suitable volume control found" << std::endl;
#ifndef DEPLOYMENT_BUILD
            std::cout << "Audio: Continuing without hardware volume control" << std::endl;
#endif
        }
        
        // Setup EQ only in development mode or if explicitly requested
#ifndef DEPLOYMENT_BUILD
        setupHiFiBerryAlsaEQ();
#endif
        
        // REMOVED: No test audio output in any mode
        
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: ✓ " << hw_name << " ready" << std::endl;
#else
        std::cout << "Audio: ✓ HiFiBerry " << hw_name << " ready" << std::endl;
#endif
        return true;
    }
    
    void shutdown() override {
        if (mixer_handle) {
            snd_mixer_close(mixer_handle);
            mixer_handle = nullptr;
        }
#ifndef DEPLOYMENT_BUILD
        std::cout << "Audio: HiFiBerry interface closed" << std::endl;
#endif
    }
    
    bool setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        
        current_volume = volume;
        
        if (!volume_elem) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "Audio: No hardware volume control, using software volume" << std::endl;
#endif
            // Fallback to software volume control
            std::string cmd = "amixer set Master " + std::to_string(volume) + "% 2>/dev/null";
            system(cmd.c_str());
            return true;
        }
        
        long min, max;
        snd_mixer_selem_get_playback_volume_range(volume_elem, &min, &max);
        
        long alsa_volume = min + (long)((max - min) * volume / 100.0);
        
        int result = snd_mixer_selem_set_playback_volume_all(volume_elem, alsa_volume);
        if (result < 0) {
            std::cerr << "Audio: Failed to set volume: " << snd_strerror(result) << std::endl;
            return false;
        }
        
#ifdef DEPLOYMENT_BUILD
        // Silent in deployment
#else
        std::cout << "Audio: ✓ Hardware volume (" << volume_control_name << ") set to " << volume << "%" << std::endl;
#endif
        return true;
    }
    
    int getVolume() override {
        if (!volume_elem) return current_volume;
        
        long vol, min, max;
        snd_mixer_selem_get_playback_volume_range(volume_elem, &min, &max);
        snd_mixer_selem_get_playback_volume(volume_elem, SND_MIXER_SCHN_MONO, &vol);
        
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
    snd_mixer_elem_t* volume_elem = nullptr;
    std::string volume_control_name = "";
    
    // REMOVED: testAudioOutput() function completely
    
    void setupHiFiBerryAlsaEQ() {
        std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
        std::string asound_path = home + "/.asoundrc";
        
        std::cout << "Audio: Setting up ALSA EQ for HiFiBerry..." << std::endl;
        
        std::ofstream asound(asound_path);
        asound << "# HiFiBerry AMP4/DAC+ with alsaeq - FIXED\n"
               << "# Force all audio through EQ\n"
               << "pcm.!default {\n"
               << "  type plug\n"
               << "  slave.pcm \"plugequal\"\n"
               << "}\n"
               << "\n"
               << "# Route EQ to HiFiBerry hardware\n"
               << "pcm.plugequal {\n"
               << "  type equal\n"
               << "  slave.pcm \"plughw:1,0\"\n"
               << "}\n"
               << "\n"
               << "# Control interface\n"
               << "ctl.!default {\n"
               << "  type hw\n"
               << "  card 1\n"
               << "}\n"
               << "\n"
               << "# EQ control interface\n"
               << "ctl.equal {\n"
               << "  type equal\n"
               << "}\n"
               << "\n"
               << "# Alternative names\n"
               << "pcm.equal {\n"
               << "  type plug\n"
               << "  slave.pcm \"plugequal\"\n"
               << "}\n";
        asound.close();
        
        // Restart ALSA to apply new config
        system("sudo /sbin/alsa force-reload >/dev/null 2>&1 || true");
        
        std::cout << "Audio: ✓ ALSA EQ configuration updated" << std::endl;
    }
    
    bool setAlsaEQBand(int band, int level) {
        std::string cmd = "amixer -D equal cset numid=" + std::to_string(band + 1) + " " + std::to_string(level) + " 2>/dev/null";
        bool success = (system(cmd.c_str()) == 0);
        
#ifndef DEPLOYMENT_BUILD
        if (success) {
            std::cout << "Audio: ✓ EQ band " << band << " set to " << level << "dB" << std::endl;
        }
#endif
        
        return success;
    }
};
#endif // AUDIO_HARDWARE_DAC || AUDIO_HARDWARE_AMP4

// BeoCreate 4 implementation - Fast init for deployment
#ifdef AUDIO_HARDWARE_BEOCREATE4
class MultiAudioManager::BeoCreateAudioImpl : public BaseAudioImpl {
public:
    bool initialize() override {
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: Fast init - BeoCreate 4" << std::endl;
#else
        std::cout << "Audio: Initializing BeoCreate 4 DSP..." << std::endl;
#endif
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // Test DSP REST API connection (reduced retries in deployment)
#ifdef DEPLOYMENT_BUILD
        int max_retries = 3;
        int sleep_time = 1;
#else
        int max_retries = 5;
        int sleep_time = 2;
#endif
        
        for (int i = 0; i < max_retries; i++) {
            if (testRestApiConnection()) {
                dsp_rest_api_available = true;
#ifdef DEPLOYMENT_BUILD
                std::cout << "Audio: ✓ DSP ready" << std::endl;
#else
                std::cout << "Audio: ✓ BeoCreate 4 DSP connected" << std::endl;
#endif
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L); // Reduced timeout for deployment
        
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

// MultiAudioManager implementation - NUTS FAST startup optimizations
MultiAudioManager::MultiAudioManager() {
    last_update = std::chrono::steady_clock::now();
}

MultiAudioManager::~MultiAudioManager() {
    shutdown();
}

bool MultiAudioManager::initialize() {
#ifdef DEPLOYMENT_BUILD
    // NUTS FAST initialization
    std::cout << "Audio: NUTS FAST init - " << getHardwareName() << std::endl;
#else
    std::cout << "Audio: Initializing " << getHardwareName() << "..." << std::endl;
#endif
    
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
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: ✓ Ready" << std::endl;
#else
        std::cout << "Audio: ✓ " << getHardwareName() << " initialized" << std::endl;
#endif
    } else {
#ifdef DEPLOYMENT_BUILD
        std::cout << "Audio: ✗ Init failed" << std::endl;
#else
        std::cout << "Audio: ✗ " << getHardwareName() << " initialization failed" << std::endl;
#endif
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

// Bluetooth methods (common to all implementations) - optimized for deployment
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
    
#ifdef DEPLOYMENT_BUILD
    // Longer update interval in deployment for performance
    if (elapsed < 15) return;
#else
    if (elapsed < 10) return;
#endif
    
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