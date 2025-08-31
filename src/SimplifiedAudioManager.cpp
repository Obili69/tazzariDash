#include "BluetoothAudioManager.h"
#include <iostream>
#include <dbus/dbus.h>
#include <pulse/pulseaudio.h>
#include <pulse/gccmacro.h>

// D-Bus helper class for cleaner code
class DBusHelper {
private:
    DBusConnection* connection = nullptr;
    
public:
    DBusHelper() {
        DBusError error;
        dbus_error_init(&error);
        
        connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
        if (dbus_error_is_set(&error)) {
            std::cerr << "D-Bus connection error: " << error.message << std::endl;
            dbus_error_free(&error);
        }
    }
    
    ~DBusHelper() {
        if (connection) {
            dbus_connection_unref(connection);
        }
    }
    
    bool isConnected() { return connection != nullptr; }
    
    // Get Bluetooth adapter power state
    bool isAdapterPowered() {
        if (!connection) return false;
        
        DBusMessage* msg = dbus_message_new_method_call(
            "org.bluez", "/org/bluez/hci0", 
            "org.freedesktop.DBus.Properties", "Get");
            
        const char* interface = "org.bluez.Adapter1";
        const char* property = "Powered";
        dbus_message_append_args(msg, 
            DBUS_TYPE_STRING, &interface,
            DBUS_TYPE_STRING, &property,
            DBUS_TYPE_INVALID);
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            connection, msg, 1000, nullptr);
        dbus_message_unref(msg);
        
        if (!reply) return false;
        
        DBusMessageIter iter, variant;
        dbus_message_iter_init(reply, &iter);
        dbus_message_iter_recurse(&iter, &variant);
        
        dbus_bool_t powered = FALSE;
        dbus_message_iter_get_basic(&variant, &powered);
        
        dbus_message_unref(reply);
        return powered == TRUE;
    }
    
    // Set adapter discoverable
    bool setDiscoverable(bool discoverable) {
        if (!connection) return false;
        
        DBusMessage* msg = dbus_message_new_method_call(
            "org.bluez", "/org/bluez/hci0",
            "org.freedesktop.DBus.Properties", "Set");
            
        const char* interface = "org.bluez.Adapter1";
        const char* property = "Discoverable";
        dbus_bool_t value = discoverable ? TRUE : FALSE;
        
        DBusMessageIter iter, variant;
        dbus_message_iter_init_append(msg, &iter);
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, 
            DBUS_TYPE_BOOLEAN_AS_STRING, &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
        dbus_message_iter_close_container(&iter, &variant);
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            connection, msg, 1000, nullptr);
        dbus_message_unref(msg);
        
        bool success = (reply != nullptr);
        if (reply) dbus_message_unref(reply);
        
        return success;
    }
    
    // Get connected A2DP devices
    std::vector<std::string> getConnectedA2DPDevices() {
        std::vector<std::string> devices;
        if (!connection) return devices;
        
        // Query all Bluetooth devices
        DBusMessage* msg = dbus_message_new_method_call(
            "org.bluez", "/",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            connection, msg, 2000, nullptr);
        dbus_message_unref(msg);
        
        if (!reply) return devices;
        
        // Parse the complex reply structure to find connected A2DP devices
        // (Simplified - full implementation would parse the entire object tree)
        
        dbus_message_unref(reply);
        return devices;
    }
};

// PulseAudio context for reliable audio control
class PulseAudioHelper {
private:
    pa_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
    bool connected = false;
    int volume = 50;
    
    static void context_state_callback(pa_context* c, void* userdata) {
        PulseAudioHelper* helper = static_cast<PulseAudioHelper*>(userdata);
        pa_context_state_t state = pa_context_get_state(c);
        
        if (state == PA_CONTEXT_READY) {
            helper->connected = true;
            std::cout << "PulseAudio: Connected" << std::endl;
        } else if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
            helper->connected = false;
            std::cout << "PulseAudio: Disconnected" << std::endl;
        }
    }
    
    static void sink_info_callback(pa_context* c, const pa_sink_info* info, 
                                  int eol, void* userdata) {
        if (eol) return;
        
        PulseAudioHelper* helper = static_cast<PulseAudioHelper*>(userdata);
        
        // Calculate average volume percentage
        pa_volume_t avg_vol = pa_cvolume_avg(&info->volume);
        helper->volume = (pa_volume_to_user(avg_vol) * 100);
    }
    
public:
    PulseAudioHelper() {
        mainloop = pa_mainloop_new();
        if (!mainloop) return;
        
        pa_mainloop_api* api = pa_mainloop_get_api(mainloop);
        context = pa_context_new(api, "TazzariDashboard");
        
        if (context) {
            pa_context_set_state_callback(context, context_state_callback, this);
            pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
        }
    }
    
    ~PulseAudioHelper() {
        if (context) {
            pa_context_disconnect(context);
            pa_context_unref(context);
        }
        if (mainloop) {
            pa_mainloop_free(mainloop);
        }
    }
    
    bool isConnected() { return connected; }
    
    void iterate() {
        if (mainloop) {
            pa_mainloop_iterate(mainloop, 0, nullptr);
        }
    }
    
    bool setVolume(int vol) {
        if (!connected || vol < 0 || vol > 100) return false;
        
        // Convert percentage to PulseAudio volume
        pa_volume_t pa_vol = pa_volume_from_user(vol / 100.0);
        
        // Set volume on default sink
        pa_operation* op = pa_context_set_sink_volume_by_name(
            context, "@DEFAULT_SINK@", 
            pa_cvolume_set(PA_CHANNELS_MAX, pa_vol),
            nullptr, nullptr);
            
        if (op) {
            pa_operation_unref(op);
            volume = vol;
            return true;
        }
        
        return false;
    }
    
    int getVolume() {
        if (!connected) return volume;
        
        // Request current volume
        pa_operation* op = pa_context_get_sink_info_by_name(
            context, "@DEFAULT_SINK@", sink_info_callback, this);
            
        if (op) {
            pa_operation_unref(op);
        }
        
        return volume;
    }
    
    // Force audio to analog output (Pi 3B+ and Pi 5 compatible)
    bool routeToAnalogOutput() {
        if (!connected) return false;
        
        // Pi 5 uses different sink names than Pi 3B+
        const char* pi5_sink = "alsa_output.platform-bcm2835_headphones.analog-stereo";
        const char* pi3_sink = "alsa_output.platform-bcm2835_audio.analog-stereo";
        
        // Try Pi 5 first, then Pi 3B+
        pa_operation* op = pa_context_set_default_sink(context, pi5_sink, nullptr, nullptr);
        if (!op) {
            op = pa_context_set_default_sink(context, pi3_sink, nullptr, nullptr);
        }
        
        if (op) {
            pa_operation_unref(op);
            std::cout << "PulseAudio: Routed to analog output" << std::endl;
            return true;
        }
        
        return false;
    }
};

// Updated BluetoothAudioManager implementation
struct BluetoothAudioManager::BluetoothImpl {
    std::unique_ptr<DBusHelper> dbus_helper;
    std::unique_ptr<PulseAudioHelper> pulse_helper;
    bool initialized = false;
    std::chrono::steady_clock::time_point last_update;
};

BluetoothAudioManager::BluetoothAudioManager() 
    : impl(std::make_unique<BluetoothImpl>()) {
}

BluetoothAudioManager::~BluetoothAudioManager() {
    shutdown();
}

bool BluetoothAudioManager::initialize() {
    std::cout << "BT Audio: Initializing improved Bluetooth manager..." << std::endl;
    
    // Initialize D-Bus helper
    impl->dbus_helper = std::make_unique<DBusHelper>();
    if (!impl->dbus_helper->isConnected()) {
        std::cout << "Warning: D-Bus connection failed - limited functionality" << std::endl;
    }
    
    // Initialize PulseAudio helper  
    impl->pulse_helper = std::make_unique<PulseAudioHelper>();
    
    // Give PulseAudio time to connect
    for (int i = 0; i < 50; i++) {
        impl->pulse_helper->iterate();
        if (impl->pulse_helper->isConnected()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!impl->pulse_helper->isConnected()) {
        std::cout << "Warning: PulseAudio connection failed" << std::endl;
        return false;
    }
    
    // Configure Bluetooth adapter
    if (impl->dbus_helper->isConnected()) {
        if (!impl->dbus_helper->isAdapterPowered()) {
            std::cout << "BT Audio: Bluetooth adapter is not powered" << std::endl;
            // Try to power it on via simple command as fallback
            system("bluetoothctl power on");
        }
        
        // Make discoverable as TazzariAudio
        impl->dbus_helper->setDiscoverable(true);
        system("bluetoothctl discoverable on");  // Fallback
        system("hciconfig hci0 name 'TazzariAudio'");
    }
    
    // Force audio to analog output initially
    impl->pulse_helper->routeToAnalogOutput();
    
    // Start monitoring thread with lower frequency
    startMonitorThread();
    
    impl->initialized = true;
    impl->last_update = std::chrono::steady_clock::now();
    
    std::cout << "BT Audio: Improved manager initialized successfully" << std::endl;
    return true;
}

void BluetoothAudioManager::shutdown() {
    running = false;
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    impl->dbus_helper.reset();
    impl->pulse_helper.reset();
}

void BluetoothAudioManager::startMonitorThread() {
    running = true;
    monitor_thread = std::thread([this]() {
        while (running) {
            // Process PulseAudio events
            if (impl->pulse_helper) {
                impl->pulse_helper->iterate();
            }
            
            // Update connection status less frequently
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - impl->last_update).count();
            
            if (elapsed >= 5) {  // Update every 5 seconds
                updateConnectionStatus();
                impl->last_update = now;
            }
            
            // Sleep longer to reduce CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
}

void BluetoothAudioManager::updateConnectionStatus() {
    if (!impl->dbus_helper || !impl->dbus_helper->isConnected()) {
        // Fallback to simple shell command
        FILE* pipe = popen("bluetoothctl devices Connected | wc -l", "r");
        if (pipe) {
            char buffer[16];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                int count = std::atoi(buffer);
                current_media.connected = (count > 0);
            }
            pclose(pipe);
        }
        return;
    }
    
    // Use D-Bus to check for connected A2DP devices
    auto devices = impl->dbus_helper->getConnectedA2DPDevices();
    current_media.connected = !devices.empty();
    
    if (current_media.connected) {
        // Ensure audio stays routed to analog output
        impl->pulse_helper->routeToAnalogOutput();
        
        std::cout << "BT Audio: Device connected, audio routed to speakers" << std::endl;
    }
}

bool BluetoothAudioManager::setVolume(int volume) {
    if (!impl->pulse_helper || !impl->pulse_helper->isConnected()) {
        return false;
    }
    
    bool success = impl->pulse_helper->setVolume(volume);
    if (success) {
        current_volume = volume;
        std::cout << "BT Audio: Volume set to " << volume << "%" << std::endl;
    }
    
    return success;
}

int BluetoothAudioManager::getVolume() {
    if (impl->pulse_helper && impl->pulse_helper->isConnected()) {
        current_volume = impl->pulse_helper->getVolume();
    }
    return current_volume;
}

// Media controls using simplified MPRIS
bool BluetoothAudioManager::play() {
    std::cout << "BT Audio: Play command" << std::endl;
    
    // Simple MPRIS command with timeout
    int result = system("timeout 2s dbus-send --session --print-reply "
                       "--dest=org.mpris.MediaPlayer2.* /org/mpris/MediaPlayer2 "
                       "org.mpris.MediaPlayer2.Player.Play 2>/dev/null");
    
    if (result == 0) {
        current_media.state = PlaybackState::PLAYING;
        return true;
    }
    
    return false;
}

bool BluetoothAudioManager::pause() {
    std::cout << "BT Audio: Pause command" << std::endl;
    
    int result = system("timeout 2s dbus-send --session --print-reply "
                       "--dest=org.mpris.MediaPlayer2.* /org/mpris/MediaPlayer2 "
                       "org.mpris.MediaPlayer2.Player.Pause 2>/dev/null");
    
    if (result == 0) {
        current_media.state = PlaybackState::PAUSED;
        return true;
    }
    
    return false;
}

bool BluetoothAudioManager::next() {
    std::cout << "BT Audio: Next track" << std::endl;
    
    return (system("timeout 2s dbus-send --session --print-reply "
                  "--dest=org.mpris.MediaPlayer2.* /org/mpris/MediaPlayer2 "
                  "org.mpris.MediaPlayer2.Player.Next 2>/dev/null") == 0);
}

bool BluetoothAudioManager::previous() {
    std::cout << "BT Audio: Previous track" << std::endl;
    
    return (system("timeout 2s dbus-send --session --print-reply "
                  "--dest=org.mpris.MediaPlayer2.* /org/mpris/MediaPlayer2 "
                  "org.mpris.MediaPlayer2.Player.Previous 2>/dev/null") == 0);
}

bool BluetoothAudioManager::isConnected() {
    return current_media.connected;
}

void BluetoothAudioManager::update() {
    // Called from main thread - just iterate PulseAudio
    if (impl->pulse_helper) {
        impl->pulse_helper->iterate();
    }
    
    // Call media callback if set
    if (media_callback) {
        media_callback(current_media);
    }
}

// Remaining methods stay the same...
MediaInfo BluetoothAudioManager::getCurrentMediaInfo() {
    return current_media;
}

void BluetoothAudioManager::setMediaStateCallback(std::function<void(const MediaInfo&)> callback) {
    media_callback = callback;
}

bool BluetoothAudioManager::stop() {
    return pause();  // Most media players treat stop as pause
}

void BluetoothAudioManager::scanForDevices() {
    // Simple background scan
    system("bluetoothctl scan on &");
    
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        system("bluetoothctl scan off");
    }).detach();
}

bool BluetoothAudioManager::connectToDevice(const std::string& address) {
    std::string command = "bluetoothctl connect " + address;
    return (system(command.c_str()) == 0);
}

void BluetoothAudioManager::disconnectCurrent() {
    system("bluetoothctl disconnect");
}