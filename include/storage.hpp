#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <fstream>

class Storage {
public:
    static constexpr std::size_t kSizeBytes = 10 * 1024 * 1024;

    Storage(const std::string& filename = "memory.bin");
    ~Storage();

    std::uint8_t read8(std::uint32_t address) const;
    std::uint16_t read16(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);
    void write16(std::uint32_t address, std::uint16_t value);
    void clear();
    std::string dump(std::uint32_t address, std::size_t length) const;

private:
    void checkAddress(std::uint32_t address, std::size_t width) const;
    void ensureFileSize() const;
    void flush() const;

    std::string filename_;
    mutable std::fstream file_;
};