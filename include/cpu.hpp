#pragma once

#include "storage.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Cpu {
public:
    explicit Cpu(Storage& storage);

    std::string execute(const std::string& line);
    std::string help() const;

private:
    enum RegisterIndex : std::size_t { AX = 0, BX, CX, DX, SP, BP, SI, DI, RegisterCount };

    std::uint16_t& reg(const std::string& name);
    const std::uint16_t& reg(const std::string& name) const;
    std::uint16_t readValue(const std::string& token) const;
    std::uint32_t readAddress(const std::string& token) const;
    static std::vector<std::string> split(const std::string& line);
    static std::string upper(std::string value);
    static std::uint32_t parseNumber(const std::string& token);
    std::string flags() const;
    void updateZeroSign(std::uint16_t value);

    Storage& storage_;
    std::array<std::uint16_t, RegisterCount> registers_{};
    bool zero_ = false;
    bool carry_ = false;
    bool sign_ = false;
};
