#include "SerialCommunication.h"
#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

SerialCommunication::SerialCommunication(const char* port, int baud) 
    : serial_port(port), baud_rate(baud) {
    
    auto now = std::chrono::steady_clock::now();
    last_auto_time = now;
    last_bms_time = now;
    
    // Initialize buffer
    data_index = 0;
    memset(packet_buffer, 0, sizeof(packet_buffer));
}

SerialCommunication::~SerialCommunication() {
    shutdown();
}

bool SerialCommunication::initialize() {
    std::cout << "Serial: Initializing communication on " << serial_port << std::endl;
    return setupSerial();
}

void SerialCommunication::shutdown() {
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
        std::cout << "Serial: Connection closed" << std::endl;
    }
}

bool SerialCommunication::setupSerial() {
    std::cout << "Serial: Opening port " << serial_port << " at " << baud_rate << " baud" << std::endl;
    
    serial_fd = open(serial_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) {
        std::cerr << "Serial: Error opening " << serial_port << ": " << strerror(errno) << std::endl;
        std::cout << "Serial: Try changing to /dev/ttyUSB0 or check 'ls /dev/tty*'" << std::endl;
        return false;
    }
    
    struct termios tty;
    if (tcgetattr(serial_fd, &tty) != 0) {
        std::cerr << "Serial: Error getting attributes: " << strerror(errno) << std::endl;
        close(serial_fd);
        return false;
    }
    
    // Configure serial port for 115200 8N1
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
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling
    
    tty.c_oflag &= ~OPOST;         // Prevent special interpretation of output bytes
    tty.c_oflag &= ~ONLCR;         // Prevent conversion of newline to carriage return/line feed
    
    tty.c_cc[VTIME] = 0;           // No blocking, return immediately
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        std::cerr << "Serial: Error setting attributes: " << strerror(errno) << std::endl;
        close(serial_fd);
        return false;
    }
    
    std::cout << "Serial: ✓ Port opened successfully - EMV noise immune parser ready" << std::endl;
    return true;
}

uint32_t SerialCommunication::getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint8_t SerialCommunication::calculateChecksum(uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// MAIN PROCESSING FUNCTION - EMV Noise Immune Scanner
void SerialCommunication::processData() {
    if (serial_fd < 0) return;
    
    uint8_t buffer[256];
    ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        // Add new data to our rolling buffer
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (data_index < sizeof(packet_buffer) - 1) {
                packet_buffer[data_index++] = buffer[i];
            } else {
                // Buffer full, shift everything left and add new byte
                memmove(packet_buffer, packet_buffer + 1, sizeof(packet_buffer) - 1);
                packet_buffer[sizeof(packet_buffer) - 1] = buffer[i];
                data_index = sizeof(packet_buffer) - 1;
            }
        }
        
        // Look for complete packets in our buffer
        findAndProcessPackets();
        
        static uint32_t last_debug = 0;
        uint32_t current_time = getCurrentTimeMs();
        if (current_time - last_debug > 10000) {  // Every 10 seconds
            std::cout << "Serial: ✓ Buffer active (" << data_index << " bytes)" << std::endl;
            last_debug = current_time;
        }
    } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cout << "Serial: Read error: " << strerror(errno) << std::endl;
    }
}

// PACKET SCANNER - Never gets stuck, ignores EMV noise
void SerialCommunication::findAndProcessPackets() {
    // Scan through buffer looking for start bytes
    for (int i = 0; i <= (int)data_index - 6; i++) {  // Minimum packet is 6 bytes
        
        // Found potential start of packet?
        if (packet_buffer[i] == PACKET_START_BYTE) {
            
            // Check if we have enough bytes for header inspection
            if (i + 2 >= data_index) break;  // Not enough data yet
            
            uint8_t packet_type = packet_buffer[i + 1];
            uint8_t packet_length = packet_buffer[i + 2];
            
            // Validate packet type (quick filter for noise)
            if (packet_type != BMS_PACKET_TYPE && packet_type != AUTO_PACKET_TYPE) {
                continue;  // Invalid type, keep scanning
            }
            
            // Validate packet length (reasonable bounds check)
            if (packet_length == 0 || packet_length > 200) {
                continue;  // Invalid length, keep scanning
            }
            
            // Calculate total packet size: START + TYPE + LENGTH + DATA + CHECKSUM + END
            int total_packet_size = 1 + 1 + 1 + packet_length + 1 + 1;
            
            // Do we have the complete packet?
            if (i + total_packet_size > data_index) {
                break;  // Incomplete packet, wait for more data
            }
            
            // Check end byte (quick validation)
            if (packet_buffer[i + total_packet_size - 1] != PACKET_END_BYTE) {
                continue;  // Wrong end byte, keep scanning
            }
            
            // Validate checksum (final validation step)
            uint8_t* data_start = &packet_buffer[i + 3];  // Skip START, TYPE, LENGTH
            uint8_t received_checksum = packet_buffer[i + total_packet_size - 2];
            
            uint8_t checksum_data[packet_length + 2];
            checksum_data[0] = packet_type;
            checksum_data[1] = packet_length;
            memcpy(&checksum_data[2], data_start, packet_length);
            uint8_t calculated_checksum = calculateChecksum(checksum_data, packet_length + 2);
            
            if (received_checksum != calculated_checksum) {
                static uint32_t last_checksum_debug = 0;
                if (getCurrentTimeMs() - last_checksum_debug > 3000) {
                    std::cout << "Serial: Checksum mismatch (EMV noise?) - continuing scan" << std::endl;
                    last_checksum_debug = getCurrentTimeMs();
                }
                continue;  // Bad checksum, keep scanning
            }
            
            // 🎉 WE FOUND A VALID PACKET! 🎉
            processValidPacket(packet_type, data_start, packet_length);
            
            // Remove processed packet from buffer
            int remaining_bytes = data_index - (i + total_packet_size);
            if (remaining_bytes > 0) {
                memmove(packet_buffer, &packet_buffer[i + total_packet_size], remaining_bytes);
            }
            data_index = remaining_bytes;
            
            // Look for more packets in remaining buffer
            if (data_index > 6) {
                findAndProcessPackets();  // Recursive scan
            }
            return;
        }
    }
    
    // Automatic buffer cleanup to prevent memory issues
    if (data_index > sizeof(packet_buffer) - 100) {
        // Keep only the last 100 bytes (might contain partial packet)
        memmove(packet_buffer, packet_buffer + data_index - 100, 100);
        data_index = 100;
        std::cout << "Serial: Buffer cleanup - keeping recent data for partial packets" << std::endl;
    }
}

// PACKET PROCESSOR - Handle validated packets
void SerialCommunication::processValidPacket(uint8_t packet_type, uint8_t* data, uint8_t length) {
    auto now = std::chrono::steady_clock::now();
    
    if (packet_type == BMS_PACKET_TYPE) {
        // Validate BMS packet size
        if (length == sizeof(bms_data_t)) {
            memcpy(&received_bms_data, data, sizeof(bms_data_t));
            new_bms_data = true;
            last_bms_time = now;
            
            // Call callback if registered
            if (bms_callback) {
                try {
                    bms_callback(received_bms_data);
                } catch (...) {
                    std::cout << "Serial: Exception in BMS callback" << std::endl;
                }
            }
            
            // Debug output (reduced frequency)
            static uint32_t last_bms_debug = 0;
            uint32_t current_time = getCurrentTimeMs();
            if (current_time - last_bms_debug > 5000) {  // Every 5 seconds
                std::cout << "Serial: ✓ BMS - SOC:" << received_bms_data.soc << "%, "
                         << "Current:" << received_bms_data.current << "A, "
                         << "Voltage:" << received_bms_data.totalVoltage << "V, "
                         << "Temp:" << received_bms_data.minTemp << "-" << received_bms_data.maxTemp << "°C" << std::endl;
                last_bms_debug = current_time;
            }
        } else {
            std::cout << "Serial: BMS packet wrong size: got " << (int)length << ", expected " << sizeof(bms_data_t) << std::endl;
        }
        
    } else if (packet_type == AUTO_PACKET_TYPE) {
        // Validate Automotive packet size
        if (length == sizeof(automotive_data_t)) {
            memcpy(&received_auto_data, data, sizeof(automotive_data_t));
            new_auto_data = true;
            last_auto_time = now;
            
            // Call callback if registered
            if (auto_callback) {
                try {
                    auto_callback(received_auto_data);
                } catch (...) {
                    std::cout << "Serial: Exception in Auto callback" << std::endl;
                }
            }
            
            // Debug output (reduced frequency)
            static uint32_t last_auto_debug = 0;
            uint32_t current_time = getCurrentTimeMs();
            if (current_time - last_auto_debug > 5000) {  // Every 5 seconds
                std::cout << "Serial: ✓ Auto - Speed:" << received_auto_data.speed_kmh << "km/h, "
                         << "RPM:" << received_auto_data.rpm << ", "
                         << "Gear:" << (received_auto_data.reverse ? "R" : (received_auto_data.forward ? "D" : "N")) << ", "
                         << "Lights:" << (received_auto_data.lightOn ? "ON" : "OFF") << std::endl;
                last_auto_debug = current_time;
            }
        } else {
            std::cout << "Serial: Auto packet wrong size: got " << (int)length << ", expected " << sizeof(automotive_data_t) << std::endl;
        }
    } else {
        // This shouldn't happen since we validate packet_type above, but just in case
        std::cout << "Serial: Unknown packet type in processor: " << (int)packet_type << std::endl;
    }
}

// DATA ACCESS METHODS
bool SerialCommunication::hasNewAutomotiveData() {
    return new_auto_data.exchange(false);
}

bool SerialCommunication::hasNewBMSData() {
    return new_bms_data.exchange(false);
}

bool SerialCommunication::isAutomotiveDataValid(int timeout_ms) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_auto_time).count();
    return elapsed <= timeout_ms && last_auto_time != std::chrono::steady_clock::time_point{};
}

bool SerialCommunication::isBMSDataValid(int timeout_ms) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_bms_time).count();
    return elapsed <= timeout_ms && last_bms_time != std::chrono::steady_clock::time_point{};
}

// CALLBACK REGISTRATION
void SerialCommunication::setAutomotiveDataCallback(std::function<void(const automotive_data_t&)> callback) {
    auto_callback = callback;
}

void SerialCommunication::setBMSDataCallback(std::function<void(const bms_data_t&)> callback) {
    bms_callback = callback;
}