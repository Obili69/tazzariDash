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
    
    std::cout << "Serial: Port opened successfully" << std::endl;
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

void SerialCommunication::processData() {
    if (serial_fd < 0) return;
    
    uint8_t buffer[256];
    ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        static uint32_t last_debug = 0;
        uint32_t current_time = getCurrentTimeMs();
        if (current_time - last_debug > 5000) {
            std::cout << "Serial: Received " << bytes_read << " bytes" << std::endl;
            last_debug = current_time;
        }
        
        for (ssize_t i = 0; i < bytes_read; i++) {
            uint8_t byte = buffer[i];
            
            try {
                switch (packet_state) {
                    case 0: // Waiting for start byte
                        if (byte == PACKET_START_BYTE) {
                            packet_state = 1;
                            // Reset packet data
                            packet_type = 0;
                            packet_length = 0;
                            data_index = 0;
                        }
                        break;
                        
                    case 1: // Got start byte, waiting for packet type
                        if (byte == BMS_PACKET_TYPE || byte == AUTO_PACKET_TYPE) {
                            packet_type = byte;
                            packet_state = 2;
                        } else {
                            std::cout << "Serial: Invalid packet type: " << (int)byte << std::endl;
                            packet_state = 0;
                        }
                        break;
                        
                    case 2: // Got type, waiting for length
                        packet_length = byte;
                        if (packet_length > 0 && packet_length <= sizeof(packet_buffer)) {
                            data_index = 0;
                            packet_state = 3;
                        } else {
                            std::cout << "Serial: Invalid packet length: " << (int)packet_length << std::endl;
                            packet_state = 0;
                        }
                        break;
                        
                    case 3: // Reading data
                        if (data_index < packet_length && data_index < sizeof(packet_buffer)) {
                            packet_buffer[data_index++] = byte;
                            if (data_index >= packet_length) {
                                packet_state = 4;
                            }
                        } else {
                            std::cout << "Serial: Buffer overflow protection triggered" << std::endl;
                            packet_state = 0;
                        }
                        break;
                        
                    case 4: { // Reading checksum
                        if (packet_length + 2 <= sizeof(packet_buffer)) {
                            uint8_t checksum_data[packet_length + 2];
                            checksum_data[0] = packet_type;
                            checksum_data[1] = packet_length;
                            memcpy(&checksum_data[2], packet_buffer, packet_length);
                            uint8_t calculated = calculateChecksum(checksum_data, packet_length + 2);
                            
                            if (byte == calculated) {
                                packet_state = 5;
                            } else {
                                std::cout << "Serial: Checksum mismatch! Expected: " << (int)calculated 
                                         << ", Got: " << (int)byte << std::endl;
                                packet_state = 0; // Reset and continue
                            }
                        } else {
                            std::cout << "Serial: Checksum buffer too small" << std::endl;
                            packet_state = 0;
                        }
                        break;
                    }
                        
                    case 5: // Checking end byte
                        if (byte == PACKET_END_BYTE) {
                            // Only call handleReceivedPacket if we got here safely
                            handleReceivedPacket();
                        } else {
                            std::cout << "Serial: Invalid end byte: " << (int)byte << std::endl;
                        }
                        packet_state = 0;
                        break;
                        
                    default:
                        std::cout << "Serial: Invalid packet state: " << packet_state << std::endl;
                        packet_state = 0;
                        break;
                }
            } catch (...) {
                std::cout << "Serial: Exception in packet processing, resetting state" << std::endl;
                packet_state = 0;
                data_index = 0;
                packet_length = 0;
                packet_type = 0;
            }
        }
    } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cout << "Serial: Read error: " << strerror(errno) << std::endl;
    }
}

void SerialCommunication::handleReceivedPacket() {
    auto now = std::chrono::steady_clock::now();
    
    // Add bounds checking to prevent segfaults
    if (packet_length == 0 || packet_length > sizeof(packet_buffer)) {
        std::cout << "Serial: Invalid packet length: " << packet_length << std::endl;
        return;
    }
    
    if (packet_type == BMS_PACKET_TYPE) {
        // Check if packet size matches expected structure
        if (packet_length != sizeof(bms_data_t)) {
            std::cout << "Serial: BMS packet size mismatch. Expected: " << sizeof(bms_data_t) 
                     << ", Got: " << packet_length << std::endl;
            return;
        }
        
        // Safe copy with bounds checking
        memset(&received_bms_data, 0, sizeof(bms_data_t)); // Clear first
        memcpy(&received_bms_data, packet_buffer, std::min((size_t)packet_length, sizeof(bms_data_t)));
        
        new_bms_data = true;
        last_bms_time = now;
        
        if (bms_callback) {
            try {
                bms_callback(received_bms_data);
            } catch (...) {
                std::cout << "Serial: Exception in BMS callback" << std::endl;
            }
        }
        
        static uint32_t last_debug = 0;
        uint32_t current_time = getCurrentTimeMs();
        if (current_time - last_debug > 3000) {
            std::cout << "Serial: BMS - SOC:" << received_bms_data.soc << "%, "
                     << "Current:" << received_bms_data.current << "A, "
                     << "Voltage:" << received_bms_data.totalVoltage << "V" << std::endl;
            last_debug = current_time;
        }
        
    } else if (packet_type == AUTO_PACKET_TYPE) {
        // Check if packet size matches expected structure
        if (packet_length != sizeof(automotive_data_t)) {
            std::cout << "Serial: Auto packet size mismatch. Expected: " << sizeof(automotive_data_t) 
                     << ", Got: " << packet_length << std::endl;
            return;
        }
        
        // Safe copy with bounds checking
        memset(&received_auto_data, 0, sizeof(automotive_data_t)); // Clear first
        memcpy(&received_auto_data, packet_buffer, std::min((size_t)packet_length, sizeof(automotive_data_t)));
        
        new_auto_data = true;
        last_auto_time = now;
        
        if (auto_callback) {
            try {
                auto_callback(received_auto_data);
            } catch (...) {
                std::cout << "Serial: Exception in Auto callback" << std::endl;
            }
        }
        
        static uint32_t last_debug = 0;
        uint32_t current_time = getCurrentTimeMs();
        if (current_time - last_debug > 3000) {
            std::cout << "Serial: Auto - Speed:" << received_auto_data.speed_kmh << "km/h, "
                     << "Gear:" << (received_auto_data.reverse ? "R" : (received_auto_data.forward ? "D" : "N")) << std::endl;
            last_debug = current_time;
        }
    } else {
        std::cout << "Serial: Unknown packet type: " << (int)packet_type << std::endl;
    }
}


bool SerialCommunication::hasNewAutomotiveData() {
    bool result = new_auto_data.exchange(false);
    return result;
}

bool SerialCommunication::hasNewBMSData() {
    bool result = new_bms_data.exchange(false);
    return result;
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

void SerialCommunication::setAutomotiveDataCallback(std::function<void(const automotive_data_t&)> callback) {
    auto_callback = callback;
}

void SerialCommunication::setBMSDataCallback(std::function<void(const bms_data_t&)> callback) {
    bms_callback = callback;
}