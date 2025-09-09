#include "PersistentStorage.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>  // for rename()
#include <chrono>

PersistentStorage::PersistentStorage(const std::string& base_filename) 
    : base_filename_(base_filename) {
    
#ifndef DEPLOYMENT_BUILD
    std::cout << "Storage: Initialized with 3-file rotation system" << std::endl;
    printStorageInfo();
#endif
}

bool PersistentStorage::loadData(DashboardData& data) {
    // Find the newest valid data file
    int newest_file = findNewestValidFile();
    
    if (newest_file == -1) {
        // No valid files found, use defaults
        data = DashboardData();  // Default values
        data.timestamp = getCurrentTimestamp();
#ifndef DEPLOYMENT_BUILD
        std::cout << "Storage: No valid data found, using defaults" << std::endl;
#endif
        return false;
    }
    
    // Load from the newest valid file
    std::string filename = getFilename(newest_file);
    bool success = loadFromFile(filename, data);
    
    if (success) {
#ifdef DEPLOYMENT_BUILD
        std::cout << "Storage: ✓ Data loaded" << std::endl;
#else
        std::cout << "Storage: ✓ Loaded from " << filename << std::endl;
        std::cout << "  ODO: " << data.odo_km << "km, TRIP: " << data.trip_km << "km" << std::endl;
        std::cout << "  Audio: Vol=" << data.audio_volume << "%, Bass=" << data.audio_bass 
                  << ", Mid=" << data.audio_mid << ", High=" << data.audio_high << std::endl;
#endif
    } else {
        std::cout << "Storage: Failed to load from " << filename << std::endl;
    }
    
    return success;
}

bool PersistentStorage::saveData(const DashboardData& data) {
    // Validate data before saving
    if (!data.isValid()) {
        std::cout << "Storage: Invalid data, refusing to save" << std::endl;
        return false;
    }
    
    // Find next file to use in rotation
    int newest_file = findNewestValidFile();
    int next_file = (newest_file + 1) % NUM_FILES;
    
    // Create data copy with updated metadata
    DashboardData save_data = data;
    save_data.timestamp = getCurrentTimestamp();
    save_data.write_count = (newest_file >= 0) ? data.write_count + 1 : 1;
    
    // Save to temp file first, then atomic rename
    std::string temp_filename = getFilename(next_file) + ".tmp";
    std::string final_filename = getFilename(next_file);
    
    if (saveToFile(temp_filename, save_data)) {
        // Atomic rename (power-cut safe on most filesystems)
        if (rename(temp_filename.c_str(), final_filename.c_str()) == 0) {
#ifdef DEPLOYMENT_BUILD
            // Silent in deployment
#else
            std::cout << "Storage: ✓ Saved to " << final_filename 
                     << " (write #" << save_data.write_count << ")" << std::endl;
#endif
            return true;
        } else {
            std::cout << "Storage: Failed to rename " << temp_filename 
                     << " to " << final_filename << std::endl;
            remove(temp_filename.c_str());  // Clean up temp file
            return false;
        }
    }
    
    return false;
}

void PersistentStorage::printStorageInfo() const {
    std::cout << "Storage: File rotation status:" << std::endl;
    
    for (int i = 0; i < NUM_FILES; i++) {
        std::string filename = getFilename(i);
        std::ifstream file(filename);
        
        if (file.good()) {
            DashboardData data;
            if (loadFromFile(filename, data)) {
                std::cout << "  " << filename << ": Valid (age: " 
                         << (getCurrentTimestamp() - data.timestamp) / 1000 << "s, "
                         << "writes: " << data.write_count << ")" << std::endl;
            } else {
                std::cout << "  " << filename << ": Corrupted" << std::endl;
            }
        } else {
            std::cout << "  " << filename << ": Missing" << std::endl;
        }
        file.close();
    }
}

std::string PersistentStorage::getFilename(int index) const {
    return base_filename_ + "_" + std::to_string(index) + ".txt";
}

bool PersistentStorage::loadFromFile(const std::string& filename, DashboardData& data) const {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Read data in format: odo_km trip_km volume bass mid high timestamp write_count
    if (!(file >> data.odo_km >> data.trip_km >> data.audio_volume >> data.audio_bass 
              >> data.audio_mid >> data.audio_high >> data.timestamp >> data.write_count)) {
        file.close();
        return false;
    }
    
    file.close();
    
    // Validate the loaded data
    return data.isValid();
}

bool PersistentStorage::saveToFile(const std::string& filename, const DashboardData& data) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Write data: odo_km trip_km volume bass mid high timestamp write_count
    file << std::fixed;
    file.precision(2);
    file << data.odo_km << " " << data.trip_km << " "
         << data.audio_volume << " " << data.audio_bass << " " 
         << data.audio_mid << " " << data.audio_high << " "
         << data.timestamp << " " << data.write_count;
    
    file.close();
    
    // Verify the write was successful
    return file.good();
}

int PersistentStorage::findNewestValidFile() const {
    int newest_file = -1;
    uint32_t newest_timestamp = 0;
    uint32_t highest_write_count = 0;
    
    for (int i = 0; i < NUM_FILES; i++) {
        DashboardData data;
        if (loadFromFile(getFilename(i), data)) {
            // Prefer higher write count, then newer timestamp
            if (data.write_count > highest_write_count || 
                (data.write_count == highest_write_count && data.timestamp > newest_timestamp)) {
                newest_file = i;
                newest_timestamp = data.timestamp;
                highest_write_count = data.write_count;
            }
        }
    }
    
    return newest_file;
}

uint32_t PersistentStorage::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}