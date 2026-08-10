#include "storage.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

Storage::Storage() : memory_(kSizeBytes, 0) {}

std::uint8_t Storage::read8(std::uint32_t address) const {
    checkAddress(address, 1);
    return memory_[address];
}

std::uint16_t Storage::read16(std::uint32_t address) const {
    checkAddress(address, 2);
    return static_cast<std::uint16_t>(memory_[address] | (memory_[address + 1] << 8));
}

void Storage::write8(std::uint32_t address, std::uint8_t value) {
    checkAddress(address, 1);
    memory_[address] = value;
}

void Storage::write16(std::uint32_t address, std::uint16_t value) {
    checkAddress(address, 2);
    memory_[address] = static_cast<std::uint8_t>(value & 0xff);
    memory_[address + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void Storage::clear() {
    std::fill(memory_.begin(), memory_.end(), 0);
}

std::string Storage::dump(std::uint32_t address, std::size_t length) const {
    checkAddress(address, length == 0 ? 1 : length);

    std::ostringstream out;
    for (std::size_t i = 0; i < length; ++i) {
        if (i % 16 == 0) {
            if (i != 0) {
                out << '\n';
            }
            out << "0x" << std::hex << std::setw(6) << std::setfill('0') << (address + i) << ": ";
        }
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(memory_[address + i]) << ' ';
    }
    return out.str();
}

void Storage::checkAddress(std::uint32_t address, std::size_t width) const {
    if (address >= memory_.size() || width > memory_.size() - address) {
        throw std::out_of_range("address is outside 10 MB storage");
    }
}
