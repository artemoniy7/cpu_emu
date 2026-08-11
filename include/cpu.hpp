#pragma once

#include "storage.hpp"
#include "disk.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

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
    std::string okResult() const;
    std::string flagsResult() const;
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

    struct FsNode {
        bool directory = true;
        std::string content;
        std::map<std::string, std::unique_ptr<FsNode>> children;
    };

    struct DriveFs {
        FsNode root;
        std::vector<std::string> cwd;
    };

    std::string currentDriveName() const;
    DriveFs& currentFs();
    const DriveFs& currentFs() const;
    FsNode* resolveNode(const std::string& path);
    const FsNode* resolveNode(const std::string& path) const;
    FsNode* resolveParent(const std::string& path, std::string& leaf);
    std::vector<std::string> normalizePath(const std::string& path) const;
    std::string formatPath(char drive, const std::vector<std::string>& parts) const;
    std::string listDirectory(const std::string& path) const;
    std::string changeDirectory(const std::string& path);
    std::string makeDirectory(const std::string& path);
    std::string removeDirectory(const std::string& path);
    std::string writeFile(const std::string& path, const std::string& content, bool append);
    std::string readFile(const std::string& path) const;
    std::string deleteFile(const std::string& path);
    std::string copyFile(const std::string& from, const std::string& to);
    std::string joinArgs(const std::vector<std::string>& args, std::size_t first) const;
    std::string createProgram(const std::string& path);
    std::string appendProgramLine(const std::string& path, const std::string& command);
    std::string runProgram(const std::string& path);
    void persistCurrentFs();
    void persistFs(char drive);
    void loadFilesystemsFromDisks();
    void serializeNode(std::ostringstream& out, const FsNode& node, const std::string& path) const;
    void deserializeFs(char drive, const std::string& data);
    static std::string hexEncode(const std::string& data);
    static std::string hexDecode(const std::string& hex);

    Storage& storage_;
    std::vector<Disk*>& disks_;
    std::array<std::uint16_t, RegisterCount> registers_{};
    bool zero_ = false;
    bool carry_ = false;
    bool sign_ = false;
    bool overflow_ = false;
    bool debug_enabled_ = true;
    
    // Disk mapping
    std::unordered_map<char, size_t> disk_map_;
    std::map<char, DriveFs> filesystems_;
    char current_drive_ = 'C';
};
