// NUTS FAST main.cpp - Ultra-fast deployment startup system
#include <lvgl.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <cmath>
#include <fstream>
#include <future>

// Project headers
#include "MultiAudioManager.h"
#include "SerialCommunication.h"

// Include UI files
extern "C" {
    #include "ui.h"
    #include "screens.h"
}

// Objects struct from EEZ Studio
extern objects_t objects;

// EEZ Studio event handler infrastructure
lv_event_t g_eez_event;
bool g_eez_event_is_available = false;

extern "C" void action_set_global_eez_event(lv_event_t* event) {
    g_eez_event = *event;
    g_eez_event_is_available = true;
}

// Gear enumeration
enum Gear {
    GEAR_D = 0,
    GEAR_N = 1,
    GEAR_R = 2
};

class Dashboard {
private:
    std::atomic<bool> running{true};
    
    // Component managers
    std::unique_ptr<MultiAudioManager> audio_manager;
    std::unique_ptr<SerialCommunication> serial_comm;
    
    // Background initialization futures
    std::future<bool> audio_init_future;
    std::future<bool> serial_init_future;
    
    // LVGL chart series
    lv_chart_series_t* voltage_series = nullptr;
    lv_chart_series_t* current_series = nullptr;
    
    // Timing variables
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point last_odo_save;
    std::chrono::steady_clock::time_point startup_time;
    
#ifdef DEPLOYMENT_BUILD
    const int UPDATE_INTERVAL = 200;    // Slower updates in deployment for performance
    const int ODO_SAVE_INTERVAL = 5000; // Less frequent saves in deployment
    const int STARTUP_ICON_DURATION = 1000; // Faster startup test (1 second)
#else
    const int UPDATE_INTERVAL = 100;    // Update display every 100ms
    const int ODO_SAVE_INTERVAL = 2000; // Save odo/trip every 2 seconds
    const int STARTUP_ICON_DURATION = 2000; // 2 seconds startup test
#endif
    
    // Vehicle data variables
    float speed_kmh = 0.0;
    float odo_km = 0.0;
    float trip_km = 0.0;
    float voltage_v = 12.4;
    float current_a = 0.0;
    int soc_percent = 85;
    int gear = 0;  // 0=D, 1=N, 2=R
    
    // BMS specific variables
    float min_cell_voltage = 0.0;
    float max_cell_voltage = 0.0;
    float min_temp = 0.0;
    float max_temp = 0.0;
    bool bms_connected = false;
    
    // Lighting states
    bool highbeam_on = false;
    bool lowbeam_on = false;
    bool fog_rear_on = false;
    bool reverse_light_on = false;
    bool indicator_left_on = false;
    bool indicator_right_on = false;
    bool brake_on = false;
    bool handbrake_on = false;
    bool light_on = false;
    
    // Startup state
    bool startup_icons_active = true;
    
    // Storage
    float saved_odo = 0.0;
    float saved_trip = 0.0;
    
    // Audio state
    std::atomic<bool> audio_initialized{false};
    std::atomic<bool> serial_initialized{false};
    
public:
    void init() {
#ifdef DEPLOYMENT_BUILD
        std::cout << "=== NUTS FAST STARTUP ===" << std::endl;
        std::cout << "Audio: " << MultiAudioManager::getHardwareName() << std::endl;
#else
        std::cout << "=== LVGL Dashboard Starting Up ===" << std::endl;
        std::cout << "Audio Hardware: " << MultiAudioManager::getHardwareName() << std::endl;
#endif
        
        // Initialize LVGL first (critical path)
#ifdef DEPLOYMENT_BUILD
        std::cout << "Boot: LVGL init..." << std::endl;
#else
        std::cout << "Boot: Initializing LVGL..." << std::endl;
#endif
        lv_init();
        
        // Initialize display based on build configuration
#ifdef DEPLOYMENT_BUILD
        lv_display_t* disp = lv_sdl_window_create(1024, 600);
#else
        std::cout << "Boot: Creating windowed display (1024x600)..." << std::endl;
        lv_display_t* disp = lv_sdl_window_create(1024, 600);
#endif
        
        lv_indev_t* indev = lv_sdl_mouse_create();
        
        // Initialize UI IMMEDIATELY (critical path)
#ifdef DEPLOYMENT_BUILD
        std::cout << "Boot: UI ready" << std::endl;
#else
        std::cout << "Boot: Initializing UI..." << std::endl;
#endif
        ui_init();
        setupChartSeries();
        
        // NUTS FAST: Load saved data immediately (synchronous, fast)
        loadFromStorage();
        
        // NUTS FAST: Show UI immediately with default values
        auto now = std::chrono::steady_clock::now();
        last_update = now;
        last_odo_save = now;
        startup_time = now;
        
        // Show startup icons immediately
        showAllIconsStartup();
        setGear(GEAR_N);
        
        // Update display with loaded data immediately
        updateDisplay();
        
#ifdef DEPLOYMENT_BUILD
        // NUTS FAST: Initialize components in BACKGROUND
        std::cout << "Boot: Background init..." << std::endl;
        initializeComponentsAsync();
        
        std::cout << "=== NUTS FAST READY! ===" << std::endl;
#else
        // Development: Initialize components normally
        initializeComponents();
        std::cout << "=== Dashboard Ready! ===" << std::endl;
#endif
    }
    
#ifdef DEPLOYMENT_BUILD
    // NUTS FAST: Asynchronous component initialization
    void initializeComponentsAsync() {
        // Start audio initialization in background thread
        audio_init_future = std::async(std::launch::async, [this]() -> bool {
            audio_manager = std::make_unique<MultiAudioManager>();
            bool success = audio_manager->initialize();
            
            if (success) {
                // Set up audio state callback
                audio_manager->setStateCallback([this](const SimpleMediaInfo& info) {
                    updateAudioDisplay(info);
                });
                audio_initialized = true;
            }
            
            return success;
        });
        
        // Start serial initialization in background thread
        serial_init_future = std::async(std::launch::async, [this]() -> bool {
            serial_comm = std::make_unique<SerialCommunication>("/dev/ttyACM0", 115200);
            bool success = serial_comm->initialize();
            
            if (success) {
                // Set up serial callbacks
                serial_comm->setAutomotiveDataCallback([this](const automotive_data_t& data) {
                    processAutomotiveData(data);
                });
                
                serial_comm->setBMSDataCallback([this](const bms_data_t& data) {
                    processBMSData(data);
                });
                serial_initialized = true;
            }
            
            return success;
        });
    }
    
    // Check if background initialization is complete
    void checkBackgroundInit() {
        // Check audio initialization (non-blocking)
        if (!audio_initialized && audio_init_future.valid()) {
            if (audio_init_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                bool success = audio_init_future.get();
                if (!success) {
                    // Audio failed, but continue
                }
            }
        }
        
        // Check serial initialization (non-blocking)
        if (!serial_initialized && serial_init_future.valid()) {
            if (serial_init_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                bool success = serial_init_future.get();
                if (!success) {
                    // Serial failed, but continue
                }
            }
        }
    }
#endif
    
    void initializeComponents() {
#ifndef DEPLOYMENT_BUILD
        std::cout << "Boot: Initializing components..." << std::endl;
#endif
        
        // Initialize Serial Communication
        serial_comm = std::make_unique<SerialCommunication>("/dev/ttyACM0", 115200);
        if (!serial_comm->initialize()) {
#ifdef DEPLOYMENT_BUILD
            // Silent in deployment
#else
            std::cout << "Warning: Serial communication failed - running without vehicle data" << std::endl;
#endif
        } else {
            serial_initialized = true;
        }
        
        // Set up serial callbacks
        if (serial_comm) {
            serial_comm->setAutomotiveDataCallback([this](const automotive_data_t& data) {
                processAutomotiveData(data);
            });
            
            serial_comm->setBMSDataCallback([this](const bms_data_t& data) {
                processBMSData(data);
            });
        }
        
        // Initialize Multi Audio Manager
#ifndef DEPLOYMENT_BUILD
        std::cout << "Boot: Initializing " << MultiAudioManager::getHardwareName() << "..." << std::endl;
#endif
        audio_manager = std::make_unique<MultiAudioManager>();
        if (audio_manager->initialize()) {
            audio_initialized = true;
            
            // Set up audio state callback
            audio_manager->setStateCallback([this](const SimpleMediaInfo& info) {
                updateAudioDisplay(info);
            });
            
#ifndef DEPLOYMENT_BUILD
            // Show hardware capabilities
            std::cout << "Boot: " << MultiAudioManager::getHardwareName() << " ready" << std::endl;
            std::cout << "  Hardware Volume: " << (MultiAudioManager::hasHardwareVolume() ? "Yes" : "No") << std::endl;
            std::cout << "  Hardware EQ: " << (MultiAudioManager::hasHardwareEQ() ? "Yes" : "No") << std::endl;
            std::cout << "  Pi appears as 'TazzariAudio' for Bluetooth" << std::endl;
#endif
        } else {
#ifndef DEPLOYMENT_BUILD
            std::cout << "Warning: Audio initialization failed" << std::endl;
#endif
        }
    }
    
    void setupChartSeries() {
        voltage_series = lv_chart_add_series(objects.cht_pwusage, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
        current_series = lv_chart_add_series(objects.cht_pwusage, lv_color_hex(0x0000FF), LV_CHART_AXIS_PRIMARY_Y);
        
        // Initialize with default values
        for(int i = 0; i < 10; i++) {
            lv_chart_set_next_value(objects.cht_pwusage, voltage_series, 0);
            lv_chart_set_next_value(objects.cht_pwusage, current_series, 200);
        }
        
#ifndef DEPLOYMENT_BUILD
        std::cout << "Charts: Series created - Voltage (red), Current (blue)" << std::endl;
#endif
    }
    
    // Startup icon display
    void showAllIconsStartup() {
        lv_obj_clear_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_fog_rear, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_fogrear, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_park, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_rearlight, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_ind_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_ind_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_break, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
    }
    
    void hideAllIcons() {
        lv_obj_add_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_fog_rear, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_fogrear, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_park, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_rearlight, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_ind_left, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_ind_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_break, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
    }
    
    void setGear(Gear gear) {
        // Reset all gear opacity
        lv_obj_set_style_text_opa(objects.lbl_gear_d, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(objects.lbl_gear_n, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(objects.lbl_gear_r, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        
        // Highlight active gear
        switch(gear) {
            case GEAR_D:
                lv_obj_set_style_text_opa(objects.lbl_gear_d, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case GEAR_N:
                lv_obj_set_style_text_opa(objects.lbl_gear_n, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case GEAR_R:
                lv_obj_set_style_text_opa(objects.lbl_gear_r, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
        }
    }
    
    void processAutomotiveData(const automotive_data_t& data) {
        speed_kmh = data.speed_kmh;
        
        // Map lighting states
        lowbeam_on = data.abblendlicht;
        highbeam_on = data.vollicht;
        fog_rear_on = data.nebelHinten;
        indicator_left_on = data.indicatorLeft;
        indicator_right_on = data.indicatorRight;
        brake_on = data.bremsfluid;
        handbrake_on = data.handbremse;
        reverse_light_on = data.reverse;
        light_on = data.lightOn;
        
        // Set gear
        if (data.reverse) {
            gear = 2;
            setGear(GEAR_R);
        } else if (data.forward) {
            gear = 0;
            setGear(GEAR_D);
        } else {
            gear = 1;
            setGear(GEAR_N);
        }
    }
    
    void processBMSData(const bms_data_t& data) {
        if (!data.dataValid) return;
        
        current_a = data.current;
        voltage_v = data.totalVoltage;
        soc_percent = (int)data.soc;
        min_cell_voltage = data.minVoltage;
        max_cell_voltage = data.maxVoltage;
        min_temp = data.minTemp;
        max_temp = data.maxTemp;
        bms_connected = true;
    }
    
    // Universal audio display update - optimized for deployment
    void updateAudioDisplay(const SimpleMediaInfo& info) {
        static SimpleMediaInfo last_info;
        
        // Only log when something changes
        bool changed = (last_info.connected != info.connected || 
                    last_info.state != info.state ||
                    last_info.track_title != info.track_title);
        
#ifdef DEPLOYMENT_BUILD
        // Minimal logging in deployment
        if (changed && info.connected) {
            std::cout << "Audio: " << info.device_name << " connected" << std::endl;
        }
#else
        if (changed) {
            std::cout << "Audio: " << (info.connected ? "Connected" : "Disconnected") 
                    << " - " << info.device_name;
            
            // Show playback state
            switch(info.state) {
                case SimplePlaybackState::PLAYING:
                    std::cout << " [Playing]";
                    break;
                case SimplePlaybackState::PAUSED:
                    std::cout << " [Paused]";
                    break;
                case SimplePlaybackState::STOPPED:
                    std::cout << " [Stopped]";
                    break;
                default:
                    break;
            }
            
            // Show current track if available
            if (!info.track_title.empty()) {
                std::cout << " - " << info.artist << " - " << info.track_title;
            }
            
            std::cout << " Vol:" << info.volume << "%" << std::endl;
        }
#endif
        
        last_info = info;
        
        // TODO: Update UI labels with track info when UI elements are available
    }
    
    void updateDisplay() {
        char buffer[32];
        
        // Update speed
        snprintf(buffer, sizeof(buffer), "%.0f", speed_kmh);
        lv_label_set_text(objects.lbl_speed, buffer);
        
        // Update odometer
        snprintf(buffer, sizeof(buffer), "%.1f", odo_km);
        lv_label_set_text(objects.lbl_odo, buffer);
        
        // Update trip
        snprintf(buffer, sizeof(buffer), "%.1f", trip_km);
        lv_label_set_text(objects.lbl_trip, buffer);
        
        // Update SOC
        if (bms_connected && serial_comm && serial_comm->isBMSDataValid()) {
            snprintf(buffer, sizeof(buffer), "%.0f%%", (float)soc_percent);
            lv_label_set_text(objects.lbl_soc, buffer);
            lv_bar_set_value(objects.bar_soc, soc_percent, LV_ANIM_ON);
        } else {
            lv_label_set_text(objects.lbl_soc, "No BMS");
        }
        
        // Update voltage range
        if (bms_connected && serial_comm && serial_comm->isBMSDataValid()) {
            snprintf(buffer, sizeof(buffer), "%.2f-%.2fV", min_cell_voltage, max_cell_voltage);
            lv_label_set_text(objects.lbl_volt_min_max, buffer);
        } else {
            lv_label_set_text(objects.lbl_volt_min_max, "No BMS");
        }
        
        // Update temperature range
        if (bms_connected && serial_comm && serial_comm->isBMSDataValid()) {
            snprintf(buffer, sizeof(buffer), "%.0f-%.0f°C", min_temp, max_temp);
            lv_label_set_text(objects.lbl_temp_min_max, buffer);
        } else {
            lv_label_set_text(objects.lbl_temp_min_max, "No BMS");
        }
    }
    
    void updateCurrentGraph() {
        // Add voltage to chart (scaled)
        int32_t voltage_chart_value = (int32_t)(voltage_v * 10);
        lv_chart_set_next_value(objects.cht_pwusage, voltage_series, voltage_chart_value);
        
        // Add current to chart (absolute value, scaled)
        int32_t current_chart_value = (int32_t)(abs(current_a) / 10.0);
        if (current_chart_value > 65) current_chart_value = 65;
        if (current_chart_value < 0) current_chart_value = 0;
        
        lv_chart_set_next_value(objects.cht_pwusage, current_series, current_chart_value);
    }
    
    void updateLightingStates() {
        if (startup_icons_active) return;
        
        // Battery warning logic - ThunderSky Winston specific
        bool battery_warning = false;
        if (bms_connected && serial_comm && serial_comm->isBMSDataValid()) {
            bool temp_high = (max_temp > 80.0);
            bool temp_low = (min_temp < -30.0);
            bool volt_high = (max_cell_voltage > 4.2 || max_cell_voltage > 4.0);
            bool volt_low = (min_cell_voltage < 2.5 || min_cell_voltage < 2.8);
            
            battery_warning = temp_high || temp_low || volt_high || volt_low;
        }
        
        // Update battery warning
        if(battery_warning) {
            lv_obj_clear_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Reverse light
        bool reverse_should_be_on = reverse_light_on || (gear == 2);
        if(reverse_should_be_on) {
            lv_obj_clear_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Lighting hierarchy logic
        bool any_light_active = lowbeam_on || highbeam_on || light_on;
        
        if (!any_light_active) {
            // DRL mode
            lv_obj_clear_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        } else if (highbeam_on) {
            // High beam active
            lv_obj_clear_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
        } else if (lowbeam_on) {
            // Low beam active
            lv_obj_clear_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        } else if (light_on) {
            // Light ON mode
            lv_obj_clear_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Rear lights
        if(any_light_active) {
            lv_obj_clear_flag(objects.img_rearlight, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_rearlight, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Other lighting states
        if(fog_rear_on) {
            lv_obj_clear_flag(objects.img_icon_fog_rear, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_fogrear, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_fog_rear, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_fogrear, LV_OBJ_FLAG_HIDDEN);
        }
        
        if(handbrake_on) {
            lv_obj_clear_flag(objects.img_icon_park, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_park, LV_OBJ_FLAG_HIDDEN);
        }
        
        if(indicator_left_on) {
            lv_obj_clear_flag(objects.img_icon_ind_left, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_ind_left, LV_OBJ_FLAG_HIDDEN);
        }
        
        if(indicator_right_on) {
            lv_obj_clear_flag(objects.img_icon_ind_right, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_ind_right, LV_OBJ_FLAG_HIDDEN);
        }
        
        if(brake_on) {
            lv_obj_clear_flag(objects.img_icon_break, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_break, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    void handleUIEvents() {
        if(!g_eez_event_is_available) return;
        
        lv_obj_t *obj = (lv_obj_t*)lv_event_get_target(&g_eez_event);
        lv_event_code_t event_code = lv_event_get_code(&g_eez_event);
        g_eez_event_is_available = false;
        
        // Trip reset
        if(obj == objects.lbl_trip) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Trip reset requested" << std::endl;
#endif
            resetTrip();
        }
        // Universal audio controls (only if audio is initialized)
        else if(obj == objects.btn_play && audio_initialized) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Play/Pause button pressed" << std::endl;
#endif
            if (audio_manager) audio_manager->togglePlayPause();
        }
        else if(obj == objects.btn_skip && audio_initialized) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Skip button pressed" << std::endl;
#endif
            if (audio_manager) audio_manager->nextTrack();
        }
        else if(obj == objects.btn_back && audio_initialized) {
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Back button pressed" << std::endl;
#endif
            if (audio_manager) audio_manager->previousTrack();
        }
        else if(obj == objects.arc_volume && audio_initialized) {
            int32_t volume = lv_arc_get_value(obj);
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Volume changed to " << volume << "%" << std::endl;
#endif
            if (audio_manager) audio_manager->setVolume(volume);
        }
        // EQ sliders (universal)
        else if(obj == objects.sld_base && audio_initialized) {
            int32_t value = lv_slider_get_value(obj);
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Bass EQ: " << value << std::endl;
#endif
            if (audio_manager) audio_manager->setBass(value - 50);
        }
        else if(obj == objects.sld_mid && audio_initialized) {
            int32_t value = lv_slider_get_value(obj);
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: Mid EQ: " << value << std::endl;
#endif
            if (audio_manager) audio_manager->setMid(value - 50);
        }
        else if(obj == objects.sld_high && audio_initialized) {
            int32_t value = lv_slider_get_value(obj);
#ifndef DEPLOYMENT_BUILD
            std::cout << "UI: High EQ: " << value << std::endl;
#endif
            if (audio_manager) audio_manager->setHigh(value - 50);
        }
    }
    
    void resetTrip() {
        trip_km = 0.0;
        saveToStorage();
#ifndef DEPLOYMENT_BUILD
        std::cout << "Trip: Counter reset to 0.0 km" << std::endl;
#endif
    }
    
    void loadFromStorage() {
        std::ifstream file("dashboard_data.txt");
        if (file.is_open()) {
            file >> odo_km >> trip_km;
            file.close();
#ifndef DEPLOYMENT_BUILD
            std::cout << "Storage: Loaded ODO=" << odo_km << "km, TRIP=" << trip_km << "km" << std::endl;
#endif
        } else {
            odo_km = saved_odo;
            trip_km = saved_trip;
#ifndef DEPLOYMENT_BUILD
            std::cout << "Storage: Using defaults ODO=" << odo_km << "km, TRIP=" << trip_km << "km" << std::endl;
#endif
        }
    }
    
    void saveToStorage() {
        std::ofstream file("dashboard_data.txt");
        if (file.is_open()) {
            file << odo_km << " " << trip_km;
            file.close();
        }
    }
    
    void run() {
        while (running) {
            auto current_time = std::chrono::steady_clock::now();
            
#ifdef DEPLOYMENT_BUILD
            // Check background initialization progress
            checkBackgroundInit();
#endif
            
            // Handle startup sequence
            auto startup_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - startup_time).count();
            if (startup_icons_active && startup_elapsed >= STARTUP_ICON_DURATION) {
                startup_icons_active = false;
                hideAllIcons();
#ifndef DEPLOYMENT_BUILD
                std::cout << "Startup: Icon test complete" << std::endl;
#endif
            }
            
            // Process vehicle data (only if serial is initialized)
            if (serial_comm && serial_initialized) {
                serial_comm->processData();
                
                // Reset speed if automotive data times out
                if (!serial_comm->isAutomotiveDataValid()) {
                    speed_kmh = 0.0;
                }
                
                // Update BMS connection status
                bms_connected = serial_comm->isBMSDataValid();
            }
            
            // Update audio manager (only if initialized)
            if (audio_manager && audio_initialized) {
                audio_manager->update();
            }
            
            // Update display at regular intervals
            auto update_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update).count();
            if(update_elapsed >= UPDATE_INTERVAL) {
                updateDisplay();
                updateCurrentGraph();
                
                if (!startup_icons_active) {
                    updateLightingStates();
                }
                
                // Update odometer (integration over time)
                float time_delta_hours = update_elapsed / 3600000.0f;
                float distance_delta = speed_kmh * time_delta_hours;
                odo_km += distance_delta;
                trip_km += distance_delta;
                
                last_update = current_time;
            }
            
            // Save data periodically
            auto save_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_odo_save).count();
            if(save_elapsed >= ODO_SAVE_INTERVAL) {
                if(abs(odo_km - saved_odo) >= 1.0 || abs(trip_km - saved_trip) >= 1.0) {
                    saveToStorage();
                    saved_odo = odo_km;
                    saved_trip = trip_km;
                }
                last_odo_save = current_time;
            }
            
            // Handle LVGL and UI events
            uint32_t sleep_time = lv_timer_handler();
            ui_tick();
            
            handleUIEvents();
            
            // Sleep - optimized for deployment
#ifdef DEPLOYMENT_BUILD
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time > 0 ? sleep_time : 10));
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time > 0 ? sleep_time : 5));
#endif
        }
    }
    
    void stop() {
        running = false;
        
        if (audio_manager) {
            audio_manager->shutdown();
        }
        
        if (serial_comm) {
            serial_comm->shutdown();
        }
    }
};

int main() {
#ifdef DEPLOYMENT_BUILD
    std::cout << "=== NUTS FAST TAZZARI DASHBOARD ===" << std::endl;
    std::cout << "Audio: " << MultiAudioManager::getHardwareName() << std::endl;
#else
    std::cout << "=== LVGL Dashboard with Multi-Audio Hardware Support ===" << std::endl;
    std::cout << "Audio Hardware: " << MultiAudioManager::getHardwareName() << std::endl;
#endif
    
    Dashboard dashboard;
    
    try {
        dashboard.init();
        dashboard.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    dashboard.stop();
    return 0;
}