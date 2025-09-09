#ifndef SERIAL_COMMUNICATION_H
#define SERIAL_COMMUNICATION_H

#include <atomic>
#include <functional>
#include <chrono>
#include <cstdint>

// Serial communication protocol - MUST MATCH ESP32 SENDER
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

class SerialCommunication {
public:
    SerialCommunication(const char* port = "/dev/ttyACM0", int baud = 115200);
    ~SerialCommunication();
    
    // Initialize/shutdown
    bool initialize();
    void shutdown();
    
    // Data processing - EMV noise immune!
    void processData();
    
    // Check connection status
    bool isConnected() const { return serial_fd >= 0; }
    
    // Data access
    const automotive_data_t& getAutomotiveData() const { return received_auto_data; }
    const bms_data_t& getBMSData() const { return received_bms_data; }
    
    // Check for new data
    bool hasNewAutomotiveData();
    bool hasNewBMSData();
    
    // Timeout checking
    bool isAutomotiveDataValid(int timeout_ms = 500);
    bool isBMSDataValid(int timeout_ms = 2000);
    
    // Set data callbacks
    void setAutomotiveDataCallback(std::function<void(const automotive_data_t&)> callback);
    void setBMSDataCallback(std::function<void(const bms_data_t&)> callback);

private:
    // Serial configuration
    const char* serial_port;
    int baud_rate;
    int serial_fd = -1;
    
    // Rolling buffer for packet scanning (EMV noise immune)
    uint8_t packet_buffer[1024];  // Larger buffer for noisy environments
    size_t data_index = 0;        // Current buffer position
    
    // Received data
    automotive_data_t received_auto_data = {0};
    bms_data_t received_bms_data = {0};
    
    // Data flags
    std::atomic<bool> new_auto_data{false};
    std::atomic<bool> new_bms_data{false};
    
    // Timing
    std::chrono::steady_clock::time_point last_auto_time;
    std::chrono::steady_clock::time_point last_bms_time;
    
    // Callbacks
    std::function<void(const automotive_data_t&)> auto_callback;
    std::function<void(const bms_data_t&)> bms_callback;
    
    // Internal methods - EMV noise immune packet processing
    bool setupSerial();
    uint8_t calculateChecksum(uint8_t* data, size_t length);
    void findAndProcessPackets();           // Scanner - never gets stuck!
    void processValidPacket(uint8_t packet_type, uint8_t* data, uint8_t length);
    uint32_t getCurrentTimeMs();
};

#endif // SERIAL_COMMUNICATION_H