#include "../include/disk.hpp"

#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <chrono>

Disk::Disk(const std::string& name, const std::string& filename) 
    : name_(name), filename_(filename) {
    ensureFileExists();
    loadMetadata();
}

Disk::~Disk() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Disk::ensureFileExists() {
    if (!std::filesystem::exists(filename_)) {
        file_.open(filename_, std::ios::binary | std::ios::out);
        if (file_.is_open()) {
            // Create empty disk image
            std::vector<uint8_t> zeros(SECTOR_SIZE * NUM_SECTORS, 0);
            file_.write(reinterpret_cast<const char*>(zeros.data()), zeros.size());
            file_.close();
        }
    }
}

void Disk::loadMetadata() {
    file_.open(filename_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open disk file: " + filename_);
    }
    
    // Check if disk is formatted (look for signature at sector 0)
    file_.seekg(0);
    uint8_t signature[8];
    file_.read(reinterpret_cast<char*>(signature), 8);
    if (file_.gcount() == 8 && 
        signature[0] == 0xFA && signature[1] == 0x11 &&
        signature[2] == 0xFB && signature[3] == 0x22 &&
        signature[4] == 0xFC && signature[5] == 0x33 &&
        signature[6] == 0xFD && signature[7] == 0x44) {
        formatted_ = true;
    } else {
        formatted_ = false;
    }
}

void Disk::saveMetadata() {
    if (formatted_) {
        // Write signature
        uint8_t signature[8] = {0xFA, 0x11, 0xFB, 0x22, 0xFC, 0x33, 0xFD, 0x44};
        file_.seekp(0);
        file_.write(reinterpret_cast<const char*>(signature), 8);
    }
}

void Disk::mount() {
    if (!file_.is_open()) {
        file_.open(filename_, std::ios::binary | std::ios::in | std::ios::out);
        if (!file_.is_open()) {
            throw std::runtime_error("Cannot mount disk: " + filename_);
        }
    }
    mounted_ = true;
}

void Disk::unmount() {
    if (file_.is_open()) {
        file_.close();
    }
    mounted_ = false;
}

void Disk::format() {
    if (!file_.is_open()) {
        mount();
    }
    
    // Clear all sectors
    std::vector<uint8_t> zeros(SECTOR_SIZE * NUM_SECTORS, 0);
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(zeros.data()), zeros.size());
    file_.flush();
    
    formatted_ = true;
    saveMetadata();
}

std::vector<uint8_t> Disk::readSector(uint32_t sector) const {
    checkSector(sector);
    
    if (!file_.is_open()) {
        throw std::runtime_error("Disk not mounted");
    }
    
    std::vector<uint8_t> data(SECTOR_SIZE);
    file_.seekg(sector * SECTOR_SIZE);
    file_.read(reinterpret_cast<char*>(data.data()), SECTOR_SIZE);
    
    if (file_.gcount() != SECTOR_SIZE) {
        throw std::runtime_error("Failed to read sector " + std::to_string(sector));
    }
    
    return data;
}

void Disk::writeSector(uint32_t sector, const std::vector<uint8_t>& data) {
    checkSector(sector);
    
    if (data.size() != SECTOR_SIZE) {
        throw std::invalid_argument("Data size must be exactly SECTOR_SIZE");
    }
    
    if (!file_.is_open()) {
        throw std::runtime_error("Disk not mounted");
    }
    
    file_.seekp(sector * SECTOR_SIZE);
    file_.write(reinterpret_cast<const char*>(data.data()), SECTOR_SIZE);
    file_.flush();
}

std::string Disk::readString(uint32_t sector, uint32_t offset, size_t length) const {
    auto data = readSector(sector);
    if (offset + length > SECTOR_SIZE) {
        throw std::out_of_range("String extends beyond sector boundary");
    }
    
    std::string result;
    for (size_t i = 0; i < length && offset + i < SECTOR_SIZE; ++i) {
        char c = static_cast<char>(data[offset + i]);
        if (c == '\0') break;
        result += c;
    }
    return result;
}

void Disk::writeString(uint32_t sector, uint32_t offset, const std::string& data) {
    auto sector_data = readSector(sector);
    if (offset + data.size() > SECTOR_SIZE) {
        throw std::out_of_range("String extends beyond sector boundary");
    }
    
    for (size_t i = 0; i < data.size(); ++i) {
        sector_data[offset + i] = static_cast<uint8_t>(data[i]);
    }
    writeSector(sector, sector_data);
}

void Disk::checkSector(uint32_t sector) const {
    if (sector >= NUM_SECTORS) {
        throw std::out_of_range("Sector " + std::to_string(sector) + 
                               " out of range (0-" + std::to_string(NUM_SECTORS - 1) + ")");
    }
}

std::string Disk::getInfo() const {
    std::ostringstream info;
    info << "Sectors: " << sector_count_ 
         << ", Size: " << (sector_count_ * SECTOR_SIZE / 1024) << "KB"
         << ", Format: " << (formatted_ ? "FAT12" : "RAW");
    
    if (formatted_) {
        // Read volume label from sector 0x20 (BPB)
        try {
            auto data = readSector(0x20);
            std::string label;
            for (int i = 0x2B; i < 0x36 && i < SECTOR_SIZE; ++i) {
                if (data[i] == 0) break;
                label += static_cast<char>(data[i]);
            }
            if (!label.empty()) {
                info << ", Label: " << label;
            }
        } catch (...) {
            // Ignore errors reading label
        }
    }
    
    return info.str();
}