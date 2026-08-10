#include "cpu.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

Cpu::Cpu(Storage& storage) : storage_(storage) {
    registers_[SP] = 0xfffe;
}

std::string Cpu::execute(const std::string& line) {
    const auto args = split(line);
    if (args.empty()) {
        return "";
    }

    const std::string op = upper(args[0]);
    try {
        if (op == "HELP") return help();
        if (op == "EXIT" || op == "QUIT") return "EXIT";
        if (op == "REGS") {
            std::ostringstream out;
            static const std::array<const char*, RegisterCount> names{"AX", "BX", "CX", "DX", "SP", "BP", "SI", "DI"};
            for (std::size_t i = 0; i < RegisterCount; ++i) {
                out << names[i] << "=0x" << std::hex << std::setw(4) << std::setfill('0') << registers_[i] << ' ';
            }
            out << flags();
            return out.str();
        }
        if (op == "RESET") {
            registers_.fill(0);
            registers_[SP] = 0xfffe;
            zero_ = carry_ = sign_ = false;
            storage_.clear();
            return "CPU and storage reset";
        }
        if ((op == "MOV" || op == "ADD" || op == "SUB" || op == "CMP") && args.size() == 3) {
            auto& dst = reg(args[1]);
            const auto rhs = readValue(args[2]);
            if (op == "MOV") dst = rhs;
            if (op == "ADD") {
                const auto result = static_cast<std::uint32_t>(dst) + rhs;
                dst = static_cast<std::uint16_t>(result);
                carry_ = result > 0xffff;
            }
            if (op == "SUB" || op == "CMP") {
                carry_ = dst < rhs;
                const auto result = static_cast<std::uint16_t>(dst - rhs);
                if (op == "SUB") dst = result;
                updateZeroSign(result);
                return op == "CMP" ? flags() : "OK " + flags();
            }
            updateZeroSign(dst);
            return "OK " + flags();
        }
        if ((op == "INC" || op == "DEC") && args.size() == 2) {
            auto& value = reg(args[1]);
            value = static_cast<std::uint16_t>(value + (op == "INC" ? 1 : -1));
            updateZeroSign(value);
            return "OK " + flags();
        }
        if ((op == "LOAD" || op == "STORE") && args.size() == 3) {
            const auto address = readAddress(args[2]);
            if (op == "LOAD") {
                reg(args[1]) = storage_.read16(address);
                updateZeroSign(reg(args[1]));
            } else {
                storage_.write16(address, reg(args[1]));
            }
            return "OK";
        }
        if (op == "POKE" && args.size() == 3) {
            storage_.write8(readAddress(args[1]), static_cast<std::uint8_t>(readValue(args[2]) & 0xff));
            return "OK";
        }
        if (op == "PEEK" && args.size() == 2) {
            std::ostringstream out;
            out << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(storage_.read8(readAddress(args[1])));
            return out.str();
        }
        if (op == "DUMP" && args.size() == 3) return storage_.dump(readAddress(args[1]), readAddress(args[2]));
        return "Unknown command or wrong argument count. Type HELP.";
    } catch (const std::exception& ex) {
        return std::string("Error: ") + ex.what();
    }
}

std::string Cpu::help() const {
    return "Commands: HELP, REGS, RESET, MOV R V, ADD R V, SUB R V, CMP R V, INC R, DEC R, LOAD R ADDR, STORE R ADDR, PEEK ADDR, POKE ADDR V, DUMP ADDR LEN, EXIT\nRegisters: AX BX CX DX SP BP SI DI. Numbers: decimal or 0xHEX.";
}

std::uint16_t& Cpu::reg(const std::string& name) {
    static const std::unordered_map<std::string, RegisterIndex> map{{"AX", AX}, {"BX", BX}, {"CX", CX}, {"DX", DX}, {"SP", SP}, {"BP", BP}, {"SI", SI}, {"DI", DI}};
    const auto it = map.find(upper(name));
    if (it == map.end()) throw std::invalid_argument("unknown register: " + name);
    return registers_[it->second];
}

const std::uint16_t& Cpu::reg(const std::string& name) const {
    return const_cast<Cpu*>(this)->reg(name);
}

std::uint16_t Cpu::readValue(const std::string& token) const {
    const auto normalized = upper(token);
    if (normalized == "AX" || normalized == "BX" || normalized == "CX" || normalized == "DX" || normalized == "SP" || normalized == "BP" || normalized == "SI" || normalized == "DI") {
        return reg(normalized);
    }
    return static_cast<std::uint16_t>(parseNumber(token) & 0xffff);
}

std::uint32_t Cpu::readAddress(const std::string& token) const {
    return parseNumber(token);
}

std::vector<std::string> Cpu::split(const std::string& line) {
    std::string normalized = line;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream in(normalized);
    std::vector<std::string> result;
    for (std::string word; in >> word;) result.push_back(word);
    return result;
}

std::string Cpu::upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

std::uint32_t Cpu::parseNumber(const std::string& token) {
    std::size_t pos = 0;
    const auto value = std::stoul(token, &pos, 0);
    if (pos != token.size()) throw std::invalid_argument("invalid number: " + token);
    return static_cast<std::uint32_t>(value);
}

std::string Cpu::flags() const {
    std::ostringstream out;
    out << "ZF=" << zero_ << " CF=" << carry_ << " SF=" << sign_;
    return out.str();
}

void Cpu::updateZeroSign(std::uint16_t value) {
    zero_ = value == 0;
    sign_ = (value & 0x8000) != 0;
}
