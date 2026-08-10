#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Storage {
public:
    static constexpr std::size_t kSizeBytes = 10 * 1024 * 1024;

    Storage();

    std::uint8_t read8(std::uint32_t address) const;
    std::uint16_t read16(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);
    void write16(std::uint32_t address, std::uint16_t value);
    void clear();
    std::string dump(std::uint32_t address, std::size_t length) const;

private:
    void checkAddress(std::uint32_t address, std::size_t width) const;

    std::vector<std::uint8_t> memory_;
};
