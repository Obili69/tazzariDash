#include <lvgl.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <cmath>
#include <fstream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>

// Include your UI files
extern "C" {
    #include "ui.h"
    #include "screens.h"
}

// Serial communication protocol - MUST MATCH SENDER
#define PACKET_START_BYTE   0xAA
#define PACKET_END_BYTE     0x55
#define BMS_PACKET_TYPE     0x01
#define AUTO_PACKET_TYPE    0x02

// Data structures - copied from ESP32 implementation
typedef struct {
  float current;         // Current in Amperes (+ charging, - discharging)
  float totalVoltage;    // Total pack voltage
  float soc;             // State of charge percentage (0-100)
  float minVoltage;      // Minimum cell voltage
  float maxVoltage;      // Maximum cell voltage
  float minTemp;         // Minimum temperature
  float maxTemp;         // Maximum temperature
  uint32_t timestamp;    // Timestamp
  bool dataValid;        // Data validity flag
} bms_data_t;

typedef struct {
  bool reverse;
  bool forward;
  bool abblendlicht;
  bool vollicht;
  bool nebelHinten;
  bool indicatorLeft;
  bool indicatorRight;
  bool bremsfluid;
  bool handbremse;
  bool lightOn;           // Running lights ON signal
  float speed_kmh;
  uint16_t rpm;
  uint32_t timestamp;
} automotive_data_t;

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
    lv_chart_series_t* voltage_series = nullptr;
    lv_chart_series_t* current_series = nullptr;
    
    // USB Serial configuration
    int serial_fd = -1;
    const char* serial_port = "/dev/ttyUSB0";  // Default USB serial port
    const int baud_rate = 115200;
    
    // Serial packet receiving variables
    uint8_t packetBuffer[256];
    int packetState = 0; // 0=waiting for start, 1=got start, 2=got type, 3=got length, 4=reading data, 5=reading checksum
    uint8_t packetType = 0;
    uint8_t packetLength = 0;
    int dataIndex = 0;
    uint8_t expectedChecksum = 0;
    
    // Received data variables
    automotive_data_t receivedAutomotiveData = {0};
    bms_data_t receivedBMSData = {0};
    bool newAutomotiveDataReceived = false;
    bool newBMSDataReceived = false;
    
    // Timing variables (converted from Arduino millis() to chrono)
    std::chrono::steady_clock::time_point lastAutomotiveDataTime;
    std::chrono::steady_clock::time_point lastBMSDataTime;
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point last_odo_save;
    std::chrono::steady_clock::time_point startup_time;
    
    const int dataTimeoutMs = 500;      // 500ms timeout for automotive data
    const int bmsTimeoutMs = 2000;      // 2 second timeout for BMS data
    const int UPDATE_INTERVAL = 100;    // Update display every 100ms
    const int ODO_SAVE_INTERVAL = 2000; // Save odo/trip every 2 seconds
    const int STARTUP_ICON_DURATION = 2000; // 2 seconds startup test
    
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
    float discharge_ah = 0.0;
    
    // Lighting states (extracted from ESP32)
    bool highbeam_on = false;
    bool lowbeam_on = false;
    bool drl_on = false;
    bool fog_rear_on = false;
    bool reverse_light_on = false;
    bool indicator_left_on = false;
    bool indicator_right_on = false;
    bool brake_on = false;
    bool handbrake_on = false;
    
    // Startup icon test
    bool startup_icons_active = true;
    
    // Storage
    float saved_odo = 0.0;
    float saved_trip = 0.0;
    
    // Get current time in milliseconds (Arduino millis() equivalent)
    uint32_t getCurrentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    // USB Serial functions
    bool initializeSerial() {
        std::cout << "Opening serial port: " << serial_port << std::endl;
        
        serial_fd = open(serial_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (serial_fd < 0) {
            std::cerr << "Error opening serial port " << serial_port << ": " << strerror(errno) << std::endl;
            std::cout << "Debug: Try changing serial_port to /dev/ttyACM0 or check 'ls /dev/tty*'" << std::endl;
            return false;
        }
        
        struct termios tty;
        if (tcgetattr(serial_fd, &tty) != 0) {
            std::cerr << "Error getting serial attributes: " << strerror(errno) << std::endl;
            close(serial_fd);
            return false;
        }
        
        // Configure serial port
        cfsetospeed(&tty, B115200);
        cfsetispeed(&tty, B115200);
        
        tty.c_cflag &= ~PARENB;        // No parity
        tty.c_cflag &= ~CSTOPB;        // 1 stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;            // 8 bits per byte
        tty.c_cflag &= ~CRTSCTS;       // No RTS/CTS flow control
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
        
        tty.c_lflag &= ~ICANON;        // Non-canonical mode
        tty.c_lflag &= ~ECHO;          // Disable echo
        tty.c_lflag &= ~ECHOE;         // Disable erasure
        tty.c_lflag &= ~ECHONL;        // Disable new-line echo
        tty.c_lflag &= ~ISIG;          // Disable interpretation of INTR, QUIT and SUSP
        
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
        tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
        
        tty.c_oflag &= ~OPOST;         // Prevent special interpretation of output bytes
        tty.c_oflag &= ~ONLCR;         // Prevent conversion of newline to carriage return/line feed
        
        tty.c_cc[VTIME] = 0;           // No blocking, return immediately
        tty.c_cc[VMIN] = 0;
        
        if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
            std::cerr << "Error setting serial attributes: " << strerror(errno) << std::endl;
            close(serial_fd);
            return false;
        }
        
        std::cout << "Serial port opened successfully at 115200 baud" << std::endl;
        return true;
    }
    
    void closeSerial() {
        if (serial_fd >= 0) {
            close(serial_fd);
            serial_fd = -1;
            std::cout << "Serial port closed" << std::endl;
        }
    }
    
    // Calculate checksum for packet verification (from ESP32)
    uint8_t calculateChecksum(uint8_t* data, size_t length) {
        uint8_t checksum = 0;
        for (size_t i = 0; i < length; i++) {
            checksum ^= data[i];
        }
        return checksum;
    }
    
    void processSerialData() {
        if (serial_fd < 0) return;
        
        uint8_t buffer[256];
        ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            static uint32_t lastSerialDebug = 0;
            uint32_t current_time = getCurrentTimeMs();
            if (current_time - lastSerialDebug > 5000) {
                std::cout << "Debug: Received " << bytes_read << " bytes from serial" << std::endl;
                lastSerialDebug = current_time;
            }
            
            for (ssize_t i = 0; i < bytes_read; i++) {
                uint8_t receivedByte = buffer[i];
                
                switch (packetState) {
                    case 0: // Waiting for start byte
                        if (receivedByte == PACKET_START_BYTE) {
                            packetState = 1;
                        }
                        break;
                        
                    case 1: // Got start byte, waiting for packet type
                        if (receivedByte == BMS_PACKET_TYPE || receivedByte == AUTO_PACKET_TYPE) {
                            packetType = receivedByte;
                            packetState = 2;
                        } else {
                            packetState = 0; // Reset on invalid type
                        }
                        break;
                        
                    case 2: // Got type, waiting for length
                        packetLength = receivedByte;
                        if (packetLength > 0 && packetLength <= sizeof(packetBuffer)) {
                            dataIndex = 0;
                            packetState = 3;
                        } else {
                            packetState = 0; // Reset on invalid length
                        }
                        break;
                        
                    case 3: // Reading data
                        packetBuffer[dataIndex++] = receivedByte;
                        if (dataIndex >= packetLength) {
                            packetState = 4; // Move to checksum reading
                        }
                        break;
                        
                    case 4: { // Reading checksum
                        // Verify checksum
                        uint8_t checksumData[packetLength + 2];
                        checksumData[0] = packetType;
                        checksumData[1] = packetLength;
                        memcpy(&checksumData[2], packetBuffer, packetLength);
                        uint8_t calculatedChecksum = calculateChecksum(checksumData, sizeof(checksumData));
                        
                        if (receivedByte == calculatedChecksum) {
                            packetState = 5; // Move to end byte checking
                        } else {
                            std::cout << "Debug: Checksum mismatch! Expected: " << (int)calculatedChecksum << ", Got: " << (int)receivedByte << std::endl;
                            packetState = 0; // Reset on checksum mismatch
                        }
                        break;
                    }
                        
                    case 5: // Checking end byte
                        if (receivedByte == PACKET_END_BYTE) {
                            // Valid packet received!
                            handleReceivedPacket();
                        } else {
                            std::cout << "Debug: Invalid end byte! Expected: " << PACKET_END_BYTE << ", Got: " << (int)receivedByte << std::endl;
                        }
                        packetState = 0; // Reset for next packet
                        break;
                }
            }
        }
    }
    
    void handleReceivedPacket() {
        if (packetType == BMS_PACKET_TYPE && packetLength == sizeof(bms_data_t)) {
            // BMS data packet
            memcpy(&receivedBMSData, packetBuffer, sizeof(bms_data_t));
            newBMSDataReceived = true;
            lastBMSDataTime = std::chrono::steady_clock::now();
            bms_connected = true;
            
            static uint32_t lastBMSDebug = 0;
            uint32_t current_time = getCurrentTimeMs();
            if (current_time - lastBMSDebug > 3000) {
                std::cout << "Debug: BMS Data - SOC: " << receivedBMSData.soc << "%, Current: " << receivedBMSData.current << "A, Voltage: " << receivedBMSData.totalVoltage << "V" << std::endl;
                lastBMSDebug = current_time;
            }
        } else if (packetType == AUTO_PACKET_TYPE && packetLength == sizeof(automotive_data_t)) {
            // Automotive data packet
            memcpy(&receivedAutomotiveData, packetBuffer, sizeof(automotive_data_t));
            newAutomotiveDataReceived = true;
            lastAutomotiveDataTime = std::chrono::steady_clock::now();
            
            static uint32_t lastAutoDebug = 0;
            uint32_t current_time = getCurrentTimeMs();
            if (current_time - lastAutoDebug > 3000) {
                std::cout << "Debug: Auto Data - Speed: " << receivedAutomotiveData.speed_kmh << " km/h, Gear: " << (receivedAutomotiveData.reverse ? "R" : (receivedAutomotiveData.forward ? "D" : "N")) << std::endl;
                lastAutoDebug = current_time;
            }
        } else {
            std::cout << "Debug: Invalid packet - Type: " << (int)packetType << ", Length: " << (int)packetLength << " (expected BMS: " << sizeof(bms_data_t) << ", AUTO: " << sizeof(automotive_data_t) << ")" << std::endl;
        }
    }
    
public:
    void init() {
        std::cout << "=== LVGL Dashboard Starting Up ===" << std::endl;
        std::cout << "Boot: Initializing LVGL..." << std::endl;
        
        lv_init();
        
        // Display initialization based on build configuration
#ifdef DEPLOYMENT_BUILD
        std::cout << "Boot: Initializing fullscreen display for deployment..." << std::endl;
        // Create fullscreen display
        lv_display_t* disp = lv_sdl_window_create(1024, 600);
        // Set fullscreen mode
        SDL_Window* sdl_window = (SDL_Window*)lv_sdl_window_get_from_indev(lv_indev_get_next(NULL));
        if (sdl_window) {
            SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            std::cout << "Boot: Fullscreen mode enabled" << std::endl;
        }
#else
        std::cout << "Boot: Initializing windowed display for development..." << std::endl;
        // Create windowed display for development
        lv_display_t* disp = lv_sdl_window_create(1024, 600);
        std::cout << "Boot: Development window created (1024x600)" << std::endl;
#endif
        
        lv_indev_t* indev = lv_sdl_mouse_create();
        
        std::cout << "Boot: Initializing UI..." << std::endl;
        ui_init();
        
        // Setup chart series
        std::cout << "Boot: Setting up chart series..." << std::endl;
        setupChartSeries();
        
        // Load saved data
        loadFromStorage();
        
        // Initialize USB Serial
        std::cout << "Boot: Initializing USB Serial..." << std::endl;
        if (!initializeSerial()) {
            std::cout << "Warning: Could not open serial port - running without real data" << std::endl;
        }
        
        // Initialize timing
        auto now = std::chrono::steady_clock::now();
        last_update = now;
        last_odo_save = now;
        startup_time = now;
        lastAutomotiveDataTime = now;
        lastBMSDataTime = now;
        
        // Show startup icons (extracted from ESP32)
        showAllIconsStartup();
        
        // Initialize gear to neutral
        setGear(GEAR_N);
        
        std::cout << "=== Dashboard Ready! Serial communication active ===" << std::endl;
    }
    
    void setupChartSeries() {
        voltage_series = lv_chart_add_series(objects.cht_pwusage, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
        current_series = lv_chart_add_series(objects.cht_pwusage, lv_color_hex(0x0000FF), LV_CHART_AXIS_PRIMARY_Y);
        
        // Initial test values
        for(int i = 0; i < 10; i++) {
            lv_chart_set_next_value(objects.cht_pwusage, voltage_series, 0);
            lv_chart_set_next_value(objects.cht_pwusage, current_series, 200);
        }
        
        std::cout << "Chart series created: Voltage (red), Current (blue)" << std::endl;
    }
    
    // Extracted from ESP32: Show all icons for startup test
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
    
    // Extracted from ESP32: Hide all icons after startup
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
    
    // Helper function for gear visualization
    void setGear(Gear gear) {
        lv_obj_set_style_text_opa(objects.lbl_gear_d, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(objects.lbl_gear_n, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(objects.lbl_gear_r, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
        
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
    
    // Extracted from ESP32: Complex lighting hierarchy logic
    void updateLightingStates() {
        if (startup_icons_active) return;
        
        // Battery warning logic - ThunderSky Winston specific
        bool battery_warning = false;
        if (bms_connected) {
            bool temp_high_warning = (max_temp > 80.0);
            bool temp_low_warning = (min_temp < -30.0);
            bool voltage_damage_high = (max_cell_voltage > 4.2);
            bool voltage_damage_low = (min_cell_voltage < 2.5);
            bool voltage_work_high = (max_cell_voltage > 4.0);
            bool voltage_work_low = (min_cell_voltage < 2.8);
            
            battery_warning = temp_high_warning || temp_low_warning ||
                             voltage_damage_high || voltage_damage_low ||
                             voltage_work_high || voltage_work_low;
        }
        
        // Update battery warning icon
        if(battery_warning) {
            lv_obj_clear_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_icon_bat, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Reverse light logic
        bool reverse_should_be_on = reverse_light_on || (gear == 2);
        if(reverse_should_be_on) {
            lv_obj_clear_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.img_reverselight, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Lighting hierarchy logic (extracted from ESP32)
        bool any_light_active = lowbeam_on || highbeam_on || receivedAutomotiveData.lightOn;
        
        // DRL mode: No light inputs active
        if (!any_light_active) {
            lv_obj_clear_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        }
        // High beam active
        else if (highbeam_on) {
            lv_obj_clear_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
        }
        // Low beam active
        else if (lowbeam_on) {
            lv_obj_clear_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        }
        // Light ON mode
        else if (receivedAutomotiveData.lightOn) {
            lv_obj_clear_flag(objects.img_icon_light, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(objects.img_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_drl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_lowbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_icon_highbeam, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(objects.img_highbeam, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Rear lights logic
        bool rear_lights_should_be_on = any_light_active;
        if(rear_lights_should_be_on) {
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
    
    // Extracted from ESP32: Display update logic
    void updateDisplay() {
        char buffer[32];
        
        // Update speed (no unit)
        snprintf(buffer, sizeof(buffer), "%.0f", speed_kmh);
        lv_label_set_text(objects.lbl_speed, buffer);
        
        // Update odometer with km unit
        snprintf(buffer, sizeof(buffer), "%.1f", odo_km);
        lv_label_set_text(objects.lbl_odo, buffer);
        
        // Update trip with km unit
        snprintf(buffer, sizeof(buffer), "%.1f", trip_km);
        lv_label_set_text(objects.lbl_trip, buffer);
        
        // Update SOC label - show percentage instead of Ah
        if (bms_connected) {
            snprintf(buffer, sizeof(buffer), "%.0f%%", receivedBMSData.soc);
            lv_label_set_text(objects.lbl_soc, buffer);
        } else {
            lv_label_set_text(objects.lbl_soc, "No BMS");
        }
        
        // Update SOC bar
        lv_bar_set_value(objects.bar_soc, soc_percent, LV_ANIM_ON);
        
        // Update voltage min/max
        if (bms_connected) {
            snprintf(buffer, sizeof(buffer), "%.2f-%.2fV", min_cell_voltage, max_cell_voltage);
            lv_label_set_text(objects.lbl_volt_min_max, buffer);
        } else {
            auto now = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBMSDataTime).count();
            if (timeDiff > bmsTimeoutMs || lastBMSDataTime == std::chrono::steady_clock::time_point{}) {
                lv_label_set_text(objects.lbl_volt_min_max, "No BMS");
            }
        }
        
        // Update temperature min/max
        if (bms_connected) {
            snprintf(buffer, sizeof(buffer), "%.0f-%.0f°C", min_temp, max_temp);
            lv_label_set_text(objects.lbl_temp_min_max, buffer);
        } else {
            auto now = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBMSDataTime).count();
            if (timeDiff > bmsTimeoutMs || lastBMSDataTime == std::chrono::steady_clock::time_point{}) {
                lv_label_set_text(objects.lbl_temp_min_max, "No BMS");
            }
        }
    }
    
    // Update chart with current/voltage (extracted from ESP32)
    void updateCurrentGraph() {
        // Add voltage reading to chart
        int32_t voltage_chart_value = (int32_t)(voltage_v * 10); // Scale voltage for chart
        lv_chart_set_next_value(objects.cht_pwusage, voltage_series, voltage_chart_value);
        
        // Add current reading to chart (convert negative discharge to positive for display)
        int32_t current_chart_value;
        if (current_a <= 0) {
            current_chart_value = (int32_t)(abs(current_a) / 10.0); // Scale down discharge current
        } else {
            current_chart_value = (int32_t)(current_a / 10.0); // Charging current
        }
        if (current_chart_value > 65) current_chart_value = 65;
        if (current_chart_value < 0) current_chart_value = 0;
        
        lv_chart_set_next_value(objects.cht_pwusage, current_series, current_chart_value);
    }
    
    // Process automotive data (extracted from ESP32)
    void processAutomotiveData() {
        speed_kmh = receivedAutomotiveData.speed_kmh;
        
        // Map lighting states
        lowbeam_on = receivedAutomotiveData.abblendlicht;
        highbeam_on = receivedAutomotiveData.vollicht;
        fog_rear_on = receivedAutomotiveData.nebelHinten;
        indicator_left_on = receivedAutomotiveData.indicatorLeft;
        indicator_right_on = receivedAutomotiveData.indicatorRight;
        brake_on = receivedAutomotiveData.bremsfluid;
        handbrake_on = receivedAutomotiveData.handbremse;
        reverse_light_on = receivedAutomotiveData.reverse;
        
        // Set gear based on forward/reverse
        if (receivedAutomotiveData.reverse) {
            gear = 2; // R
            setGear(GEAR_R);
        } else if (receivedAutomotiveData.forward) {
            gear = 0; // D
            setGear(GEAR_D);
        } else {
            gear = 1; // N
            setGear(GEAR_N);
        }
    }
    
    // Process BMS data (extracted from ESP32)
    void processBMSData() {
        if (!receivedBMSData.dataValid) return;
        
        current_a = receivedBMSData.current;
        voltage_v = receivedBMSData.totalVoltage;
        soc_percent = (int)receivedBMSData.soc;
        min_cell_voltage = receivedBMSData.minVoltage;
        max_cell_voltage = receivedBMSData.maxVoltage;
        min_temp = receivedBMSData.minTemp;
        max_temp = receivedBMSData.maxTemp;
        
        // Calculate discharge amp-hours from SOC (160Ah total capacity)
        discharge_ah = 160.0 * (100.0 - receivedBMSData.soc) / 100.0;
    }
    
    // Trip reset function
    void resetTrip() {
        trip_km = 0.0;
        saveToStorage();
        std::cout << "Trip counter reset!" << std::endl;
    }
    
    // Simple storage functions (replace Preferences)
    void loadFromStorage() {
        std::ifstream file("dashboard_data.txt");
        if (file.is_open()) {
            file >> odo_km >> trip_km;
            file.close();
            std::cout << "Loaded: ODO=" << odo_km << "km, TRIP=" << trip_km << "km" << std::endl;
        } else {
            odo_km = saved_odo;
            trip_km = saved_trip;
            std::cout << "Using default values: ODO=" << odo_km << "km, TRIP=" << trip_km << "km" << std::endl;
        }
    }
    
    void saveToStorage() {
        std::ofstream file("dashboard_data.txt");
        if (file.is_open()) {
            file << odo_km << " " << trip_km;
            file.close();
            std::cout << "Saved: ODO=" << odo_km << "km, TRIP=" << trip_km << "km" << std::endl;
        }
    }
    
    void run() {
        while (running) {
            auto current_time = std::chrono::steady_clock::now();
            uint32_t current_time_ms = getCurrentTimeMs();
            
            // Handle startup icon display
            auto startup_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - startup_time).count();
            if (startup_icons_active && startup_elapsed >= STARTUP_ICON_DURATION) {
                startup_icons_active = false;
                hideAllIcons();
                std::cout << "Startup sequence complete - icons hidden" << std::endl;
            }
            
            // Process USB serial data
            processSerialData();
            
            // Check for timeouts
            auto auto_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - lastAutomotiveDataTime).count();
            if (auto_timeout > dataTimeoutMs && lastAutomotiveDataTime != std::chrono::steady_clock::time_point{}) {
                // Reset speed on timeout
                speed_kmh = 0.0;
            }
            
            auto bms_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - lastBMSDataTime).count();
            if (bms_timeout > bmsTimeoutMs && lastBMSDataTime != std::chrono::steady_clock::time_point{}) {
                bms_connected = false;
            }
            
            // Process received data
            if (newAutomotiveDataReceived) {
                processAutomotiveData();
                newAutomotiveDataReceived = false;
            }
            
            if (newBMSDataReceived) {
                processBMSData();
                newBMSDataReceived = false;
            }
            
            // Update display at regular intervals
            auto update_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update).count();
            if(update_elapsed >= UPDATE_INTERVAL) {
                updateDisplay();
                updateCurrentGraph();
                
                if (!startup_icons_active) {
                    updateLightingStates();
                }
                
                // Update odometer based on speed (integration over time)
                float time_delta_hours = update_elapsed / 3600000.0f; // Convert ms to hours
                float distance_delta = speed_kmh * time_delta_hours;
                odo_km += distance_delta;
                trip_km += distance_delta;
                
                last_update = current_time;
            }
            
            // Save to storage periodically
            auto save_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_odo_save).count();
            if(save_elapsed >= ODO_SAVE_INTERVAL) {
                if(abs(odo_km - saved_odo) >= 1.0 || abs(trip_km - saved_trip) >= 1.0) {
                    saveToStorage();
                    saved_odo = odo_km;
                    saved_trip = trip_km;
                }
                last_odo_save = current_time;
            }
            
            // Handle LVGL tasks
            uint32_t sleep_time = lv_timer_handler();
            ui_tick();
            
            // Handle EEZ events
            if(g_eez_event_is_available) {
                lv_obj_t *obj = (lv_obj_t*)lv_event_get_target(&g_eez_event);
                lv_event_code_t event_code = lv_event_get_code(&g_eez_event);
                g_eez_event_is_available = false;
                
                std::cout << "Event: Object:" << obj << " Code:" << event_code << std::endl;
                
                // Handle trip reset (click on trip label)
                if(obj == objects.lbl_trip) {
                    std::cout << ">>> Trip label clicked - resetting trip!" << std::endl;
                    resetTrip();
                }
                // Handle audio controls (keep for bluetooth functionality)
                else if(obj == objects.btn_play) {
                    std::cout << ">>> Play button clicked!" << std::endl;
                }
                else if(obj == objects.btn_skip) {
                    std::cout << ">>> Skip button clicked!" << std::endl;
                }
                else if(obj == objects.btn_back) {
                    std::cout << ">>> Back button clicked!" << std::endl;
                }
                else if(obj == objects.arc_volume) {
                    int32_t volume = lv_arc_get_value(obj);
                    std::cout << ">>> Volume arc: " << volume << "%" << std::endl;
                }
                // Handle EQ sliders (keep for bluetooth functionality)
                else if(obj == objects.sld_base) {
                    int32_t value = lv_slider_get_value(obj);
                    std::cout << ">>> Bass slider: " << value << std::endl;
                }
                else if(obj == objects.sld_mid) {
                    int32_t value = lv_slider_get_value(obj);
                    std::cout << ">>> Mid slider: " << value << std::endl;
                }
                else if(obj == objects.sld_high) {
                    int32_t value = lv_slider_get_value(obj);
                    std::cout << ">>> High slider: " << value << std::endl;
                }
                else {
                    std::cout << ">>> Unknown object: " << obj << std::endl;
                }
            }
            
            // Sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time > 0 ? sleep_time : 5));
        }
    }
    
    void stop() {
        closeSerial();
        running = false;
    }
    
    ~Dashboard() {
        closeSerial();
    }
};

int main() {
    std::cout << "=== Boot: LVGL Dashboard with USB Serial Communication ===" << std::endl;
    
    Dashboard dashboard;
    
    try {
        dashboard.init();
        dashboard.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
