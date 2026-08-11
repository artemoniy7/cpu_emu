#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

class Disk {
public:
    static constexpr uint32_t SECTOR_SIZE = 512;
    static constexpr uint32_t NUM_SECTORS = 2880; // 1.44MB floppy
    
    Disk(const std::string& name, const std::string& filename);
    ~Disk();
    
    std::string getName() const { return name_; }
    bool isMounted() const { return mounted_; }
    void mount();
    void unmount();
    void format();
    
    std::vector<uint8_t> readSector(uint32_t sector) const;
    void writeSector(uint32_t sector, const std::vector<uint8_t>& data);
    std::string readString(uint32_t sector, uint32_t offset, size_t length) const;
    void writeString(uint32_t sector, uint32_t offset, const std::string& data);
    
    std::string getInfo() const;

private:
    void checkSector(uint32_t sector) const;
    void ensureFileExists();
    void loadMetadata();
    void saveMetadata();
    
    std::string name_;
    std::string filename_;
    bool mounted_ = false;
    mutable std::fstream file_;
    uint32_t sector_count_ = NUM_SECTORS;
    bool formatted_ = false;
};