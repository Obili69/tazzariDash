#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <string>
#include <cstdint>

// Data structure for all persistent dashboard settings
struct DashboardData {
    // Odometer data
    float odo_km = 0.0f;
    float trip_km = 0.0f;
    
    // Audio settings
    int audio_volume = 50;      // 0-100%
    int audio_bass = 0;         // -50 to +50 (slider shows 0-100, we convert)
    int audio_mid = 0;          // -50 to +50
    int audio_high = 0;         // -50 to +50
    
    // Metadata
    uint32_t timestamp = 0;     // When this data was saved
    uint32_t write_count = 0;   // Number of times data has been written (for rotation)
    
    // Validation
    bool isValid() const {
        return (odo_km >= 0.0f && odo_km < 999999.0f &&
                trip_km >= 0.0f && trip_km < 999999.0f &&
                audio_volume >= 0 && audio_volume <= 100 &&
                audio_bass >= -50 && audio_bass <= 50 &&
                audio_mid >= -50 && audio_mid <= 50 &&
                audio_high >= -50 && audio_high <= 50 &&
                timestamp > 0);
    }
};

class PersistentStorage {
public:
    PersistentStorage(const std::string& base_filename = "dashboard_data");
    ~PersistentStorage() = default;
    
    // Load data from the most recent valid file
    bool loadData(DashboardData& data);
    
    // Save data using 3-file rotation for power-cut protection
    bool saveData(const DashboardData& data);
    
    // Get statistics about storage files
    void printStorageInfo() const;

private:
    std::string base_filename_;
    static const int NUM_FILES = 3;
    
    // Get filename for rotation index (0, 1, 2)
    std::string getFilename(int index) const;
    
    // Load data from specific file
    bool loadFromFile(const std::string& filename, DashboardData& data) const;
    
    // Save data to specific file
    bool saveToFile(const std::string& filename, const DashboardData& data) const;
    
    // Find the file with the most recent valid data
    int findNewestValidFile() const;
    
    // Get current timestamp in milliseconds
    uint32_t getCurrentTimestamp() const;
};

#endif // PERSISTENT_STORAGE_H