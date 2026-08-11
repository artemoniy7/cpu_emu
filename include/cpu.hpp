#pragma once

#include "storage.hpp"
#include "disk.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

class Cpu {
public:
    explicit Cpu(Storage& storage, std::vector<Disk*>& disks);

    std::string execute(const std::string& line);
    std::string help() const;

private:
    enum RegisterIndex : std::size_t { 
        AX = 0, BX, CX, DX, 
        SP, BP, SI, DI,
        CS, DS, ES, SS,
        RegisterCount 
    };
    
    enum ArgType { REGISTER, NUMBER, STRING, ADDRESS, DISK };
    
    ArgType detectArgType(const std::string& token) const;
    std::string unquote(const std::string& token) const;
    std::uint16_t& reg(const std::string& name);
    const std::uint16_t& reg(const std::string& name) const;
    std::uint16_t readValue(const std::string& token) const;
    std::uint32_t readAddress(const std::string& token) const;
    static std::vector<std::string> split(const std::string& line);
    static std::string upper(std::string value);
    static std::uint32_t parseNumber(const std::string& token);
    std::string flags() const;
    void updateZeroSign(std::uint16_t value);
    
    void movsb();
    void movsw();
    bool cmpsb();
    bool scasb();
    void stosb();
    void lodsb();
    std::string readString(std::uint32_t address) const;
    void writeString(std::uint32_t address, const std::string& str);
    
    void setFlagsForComparison(int result);
    bool checkCondition(const std::string& condition);
    
    // Disk operations
    std::string diskInfo() const;
    std::string formatDisk(const std::string& diskName);
    std::string readDisk(const std::string& diskName, uint32_t sector);
    std::string writeDisk(const std::string& diskName, uint32_t sector, const std::string& data);

    Storage& storage_;
    std::vector<Disk*>& disks_;
    std::array<std::uint16_t, RegisterCount> registers_{};
    bool zero_ = false;
    bool carry_ = false;
    bool sign_ = false;
    bool overflow_ = false;
    
    // Disk mapping
    std::unordered_map<char, size_t> disk_map_;
};