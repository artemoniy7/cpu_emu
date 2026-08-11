#include "../include/storage.hpp"

#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <filesystem>

Storage::Storage(const std::string& filename) : filename_(filename) {
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!file_.is_open()) {
        file_.clear();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open()) {
            throw std::runtime_error("Cannot open storage file: " + filename_);
        }
        file_.seekp(0);
        std::vector<std::uint8_t> zeros(kSizeBytes, 0);
        file_.write(reinterpret_cast<const char*>(zeros.data()), kSizeBytes);
        file_.flush();
    }
    
    ensureFileSize();
}

Storage::~Storage() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Storage::ensureFileSize() const {
    file_.seekg(0, std::ios::end);
    const auto size = file_.tellg();
    if (size < static_cast<std::streamoff>(kSizeBytes)) {
        file_.clear();
        file_.seekp(size);
        std::vector<std::uint8_t> zeros(kSizeBytes - size, 0);
        file_.write(reinterpret_cast<const char*>(zeros.data()), zeros.size());
        file_.flush();
    }
}

void Storage::flush() const {
    file_.flush();
}

std::uint8_t Storage::read8(std::uint32_t address) const {
    checkAddress(address, 1);
    file_.seekg(address);
    std::uint8_t value;
    file_.read(reinterpret_cast<char*>(&value), 1);
    if (!file_) {
        throw std::runtime_error("Failed to read from storage");
    }
    return value;
}

std::uint16_t Storage::read16(std::uint32_t address) const {
    checkAddress(address, 2);
    file_.seekg(address);
    std::uint8_t bytes[2];
    file_.read(reinterpret_cast<char*>(bytes), 2);
    if (!file_) {
        throw std::runtime_error("Failed to read from storage");
    }
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

void Storage::write8(std::uint32_t address, std::uint8_t value) {
    checkAddress(address, 1);
    file_.seekp(address);
    file_.write(reinterpret_cast<const char*>(&value), 1);
    if (!file_) {
        throw std::runtime_error("Failed to write to storage");
    }
    file_.flush();
}

void Storage::write16(std::uint32_t address, std::uint16_t value) {
    checkAddress(address, 2);
    std::uint8_t bytes[2] = {
        static_cast<std::uint8_t>(value & 0xff),
        static_cast<std::uint8_t>((value >> 8) & 0xff)
    };
    file_.seekp(address);
    file_.write(reinterpret_cast<const char*>(bytes), 2);
    if (!file_) {
        throw std::runtime_error("Failed to write to storage");
    }
    file_.flush();
}

void Storage::clear() {
    file_.close();
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot reopen storage file");
    }
    std::vector<std::uint8_t> zeros(kSizeBytes, 0);
    file_.write(reinterpret_cast<const char*>(zeros.data()), kSizeBytes);
    file_.flush();
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
        const auto value = read8(address + i);
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value) << ' ';
    }
    return out.str();
}

void Storage::checkAddress(std::uint32_t address, std::size_t width) const {
    if (address >= kSizeBytes || width > kSizeBytes - address) {
        throw std::out_of_range("address is outside 10 MB storage");
    }
}