#include "../include/cpu.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <cctype>
#include <cstdint>
#include <utility>

Cpu::Cpu(Storage& storage, std::vector<Disk*>& disks, InputReader input_reader)
    : storage_(storage), disks_(disks), input_reader_(std::move(input_reader)) {
    registers_.fill(0);
    registers_[SP] = 0xfffe;
    registers_[CS] = 0x0000;
    registers_[DS] = 0x0000;
    registers_[ES] = 0x0000;
    registers_[SS] = 0x0000;
    
    // Map disk letters
    char letter = 'C';
    for (size_t i = 0; i < disks_.size() && i < 26; ++i) {
        disk_map_[letter] = i;
        filesystems_[letter];
        if (letter == 'C') letter = 'D';
        else letter++;
    }
    loadFilesystemsFromDisks();
}

Cpu::ArgType Cpu::detectArgType(const std::string& token) const {
    const auto normalized = upper(token);
    
    // Check for disk
    if (token.size() == 1 && token[0] >= 'A' && token[0] <= 'Z') {
        auto it = disk_map_.find(token[0]);
        if (it != disk_map_.end()) {
            return DISK;
        }
    }
    
    if (normalized == "AX" || normalized == "BX" || normalized == "CX" || 
        normalized == "DX" || normalized == "SP" || normalized == "BP" || 
        normalized == "SI" || normalized == "DI" || normalized == "CS" ||
        normalized == "DS" || normalized == "ES" || normalized == "SS") {
        return REGISTER;
    }
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        return STRING;
    }
    if (token.size() >= 2 && token.front() == '[' && token.back() == ']') {
        return ADDRESS;
    }
    return NUMBER;
}

std::string Cpu::unquote(const std::string& token) const {
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        return token.substr(1, token.size() - 2);
    }
    return token;
}

std::uint32_t Cpu::readAddress(const std::string& token) const {
    std::string addr_token = token;
    
    if (addr_token.size() >= 2 && addr_token.front() == '[' && addr_token.back() == ']') {
        addr_token = addr_token.substr(1, addr_token.size() - 2);
    }
    
    const auto normalized = upper(addr_token);
    if (normalized == "AX" || normalized == "BX" || normalized == "CX" || 
        normalized == "DX" || normalized == "SP" || normalized == "BP" || 
        normalized == "SI" || normalized == "DI") {
        return reg(normalized);
    }
    
    return parseNumber(addr_token);
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
        if ((op == "DEBUG" || op == "DBG") && args.size() <= 2) {
            if (args.size() == 1) {
                return std::string("Debug output is ") + (debug_enabled_ ? "ON" : "OFF");
            }

            const auto mode = upper(args[1]);
            if (mode == "ON" || mode == "1" || mode == "TRUE") {
                debug_enabled_ = true;
                return "Debug output enabled";
            }
            if (mode == "OFF" || mode == "0" || mode == "FALSE") {
                debug_enabled_ = false;
                return "Debug output disabled";
            }
            return "Usage: DEBUG ON|OFF";
        }
        
        if (op == "REGS") {
            std::ostringstream out;
            static const std::array<const char*, RegisterCount> names{
                "AX", "BX", "CX", "DX", 
                "SP", "BP", "SI", "DI",
                "CS", "DS", "ES", "SS"
            };
            for (std::size_t i = 0; i < RegisterCount; ++i) {
                out << names[i] << "=0x" << std::hex << std::setw(4) << std::setfill('0') 
                    << registers_[i] << ' ';
                if (i == 7 || i == 11) out << '\n';
            }
            out << flags();
            return out.str();
        }
        
        if (op == "RESET") {
            registers_.fill(0);
            registers_[SP] = 0xfffe;
            registers_[CS] = registers_[DS] = registers_[ES] = registers_[SS] = 0;
            zero_ = carry_ = sign_ = overflow_ = false;
            storage_.clear();
            return "CPU and storage reset";
        }
        
        // DOS-like file system commands
        if (args.size() == 1 && args[0].size() == 2 && args[0][1] == ':') {
            const char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(args[0][0])));
            if (disk_map_.find(drive) == disk_map_.end()) {
                return "Invalid drive specification";
            }
            current_drive_ = drive;
            return currentDriveName();
        }

        if ((op == "DIR" || op == "LS") && args.size() <= 2) {
            return listDirectory(args.size() == 2 ? args[1] : ".");
        }

        if ((op == "CD" || op == "CHDIR") && args.size() <= 2) {
            return args.size() == 1 ? currentDriveName() : changeDirectory(args[1]);
        }

        if ((op == "MD" || op == "MKDIR") && args.size() == 2) {
            return makeDirectory(args[1]);
        }

        if ((op == "RD" || op == "RMDIR") && args.size() == 2) {
            return removeDirectory(args[1]);
        }

        if ((op == "DEL" || op == "ERASE") && args.size() == 2) {
            return deleteFile(args[1]);
        }

        if (op == "TYPE" && args.size() == 2) {
            return readFile(args[1]);
        }

        if (op == "COPY" && args.size() == 3) {
            return copyFile(args[1], args[2]);
        }


        if ((op == "NEW" || op == "PROGRAM") && args.size() == 2) {
            return createProgram(args[1]);
        }

        if ((op == "APPEND" || op == "LINE") && args.size() >= 3) {
            return appendProgramLine(args[1], joinArgs(args, 2));
        }

        if ((op == "RUN" || op == "EXEC") && args.size() == 2) {
            return runProgram(args[1]);
        }

        if (op == "ECHO" && args.size() >= 2) {
            auto redir = std::find(args.begin(), args.end(), ">");
            bool append = false;
            if (redir == args.end()) {
                redir = std::find(args.begin(), args.end(), ">>");
                append = redir != args.end();
            }
            if (redir != args.end()) {
                const auto index = static_cast<std::size_t>(std::distance(args.begin(), redir));
                if (index + 1 >= args.size()) return "The syntax of the command is incorrect.";
                std::string text = joinArgs(args, 1);
                const auto cut = text.find(append ? ">>" : ">");
                if (cut != std::string::npos) text = text.substr(0, cut);
                while (!text.empty() && text.back() == ' ') text.pop_back();
                return writeFile(args[index + 1], text + "\n", append);
            }
            return joinArgs(args, 1);
        }

        // Disk operations
        if (op == "DISKS") {
            return diskInfo();
        }
        
        if (op == "FORMAT" && args.size() == 2) {
            return formatDisk(args[1]);
        }
        
        if (op == "READ" && args.size() == 3) {
            return readDisk(args[1], parseNumber(args[2]));
        }
        
        if (op == "WRITE" && args.size() >= 3) {
            std::string data;
            for (size_t i = 3; i < args.size(); ++i) {
                if (i > 3) data += " ";
                data += args[i];
            }
            return writeDisk(args[1], parseNumber(args[2]), data);
        }
        
        // Арифметические операции
        if ((op == "MOV" || op == "ADD" || op == "SUB" || op == "CMP") && args.size() == 3) {
            auto& dst = reg(args[1]);
            const auto rhs = readValue(args[2]);
            
            if (op == "MOV") {
                dst = rhs;
            } else if (op == "ADD") {
                const auto result = static_cast<std::uint32_t>(dst) + rhs;
                overflow_ = ((dst & 0x8000) == (rhs & 0x8000)) && 
                           ((result & 0x8000) != (dst & 0x8000));
                dst = static_cast<std::uint16_t>(result);
                carry_ = result > 0xffff;
            } else if (op == "SUB" || op == "CMP") {
                const auto result = static_cast<std::int32_t>(dst) - static_cast<std::int32_t>(rhs);
                overflow_ = (result > 0x7fff || result < -0x8000);
                carry_ = dst < rhs;
                const auto uresult = static_cast<std::uint16_t>(dst - rhs);
                if (op == "SUB") dst = uresult;
                updateZeroSign(uresult);
                if (op == "CMP") {
                    return flagsResult();
                }
            }
            updateZeroSign(dst);
            return okResult();
        }
        
        // Инкремент/декремент
        if ((op == "INC" || op == "DEC") && args.size() == 2) {
            auto& value = reg(args[1]);
            const auto old = value;
            value = static_cast<std::uint16_t>(value + (op == "INC" ? 1 : -1));
            overflow_ = (op == "INC" && old == 0x7fff) || (op == "DEC" && old == 0x8000);
            carry_ = (op == "INC" && old == 0xffff) || (op == "DEC" && old == 0x0000);
            updateZeroSign(value);
            return okResult();
        }
        
        // Логические операции
        if ((op == "AND" || op == "OR" || op == "XOR") && args.size() == 3) {
            auto& dst = reg(args[1]);
            const auto rhs = readValue(args[2]);
            if (op == "AND") dst &= rhs;
            if (op == "OR")  dst |= rhs;
            if (op == "XOR") dst ^= rhs;
            carry_ = false;
            overflow_ = false;
            updateZeroSign(dst);
            return okResult();
        }
        
        if (op == "NOT" && args.size() == 2) {
            auto& dst = reg(args[1]);
            dst = ~dst;
            carry_ = false;
            overflow_ = false;
            updateZeroSign(dst);
            return okResult();
        }
        
        if (op == "TEST" && args.size() == 3) {
            const auto value1 = readValue(args[1]);
            const auto value2 = readValue(args[2]);
            const auto result = value1 & value2;
            carry_ = false;
            overflow_ = false;
            updateZeroSign(result);
            return flagsResult();
        }
        
        // Условные переходы
        if ((op == "JMP" || op == "JE" || op == "JZ" || op == "JNE" || op == "JNZ" ||
             op == "JG" || op == "JNLE" || op == "JL" || op == "JNGE" ||
             op == "JGE" || op == "JNL" || op == "JLE" || op == "JNG" ||
             op == "JA" || op == "JNBE" || op == "JB" || op == "JNAE" ||
             op == "JAE" || op == "JNB" || op == "JBE" || op == "JNA" ||
             op == "JC" || op == "JNC" || op == "JO" || op == "JNO" ||
             op == "JS" || op == "JNS") && args.size() == 2) {
            
            const auto target = readAddress(args[1]);
            bool shouldJump = checkCondition(op);
            
            if (shouldJump) {
                std::ostringstream out;
                out << "Jump to 0x" << std::hex << target;
                return out.str();
            }
            return debug_enabled_ ? "No jump (flags: " + flags() + ")" : "No jump";
        }
        
        // Строковые операции
        if (op == "MOVSB" && args.size() == 1) {
            movsb();
            return okResult();
        }
        
        if (op == "MOVSW" && args.size() == 1) {
            movsw();
            return okResult();
        }
        
        if (op == "CMPSB" && args.size() == 1) {
            const bool equal = cmpsb();
            zero_ = equal;
            return equal ? "Equal" : (debug_enabled_ ? "Not equal " + flags() : "Not equal");
        }
        
        if (op == "SCASB" && args.size() == 1) {
            const bool found = scasb();
            zero_ = found;
            return found ? "Found" : (debug_enabled_ ? "Not found " + flags() : "Not found");
        }
        
        if (op == "STOSB" && args.size() == 1) {
            stosb();
            return okResult();
        }
        
        if (op == "LODSB" && args.size() == 1) {
            lodsb();
            return okResult();
        }
        
        // REP префикс
        if (op == "REP" && args.size() == 2) {
            auto count = reg("CX");
            if (count == 0) return "REP: Count is zero";
            
            const auto subop = upper(args[1]);
            bool stop = false;
            
            while (reg("CX") > 0 && !stop) {
                if (subop == "MOVSB") {
                    movsb();
                } else if (subop == "MOVSW") {
                    movsw();
                } else if (subop == "CMPSB") {
                    if (!cmpsb()) {
                        zero_ = false;
                        stop = true;
                    }
                } else if (subop == "SCASB") {
                    if (!scasb()) {
                        zero_ = false;
                        stop = true;
                    }
                } else if (subop == "STOSB") {
                    stosb();
                } else if (subop == "LODSB") {
                    lodsb();
                } else {
                    return "Unknown REP operation: " + subop;
                }
                reg("CX")--;
            }
            
            std::ostringstream out;
            out << "REP " << subop << " executed " 
                << (reg("CX") == 0 ? "all" : "stopped at count " + std::to_string(reg("CX")))
                << " iterations";
            return debug_enabled_ ? out.str() + " " + flags() : out.str();
        }
        
        if ((op == "REPE" || op == "REPZ") && args.size() == 2) {
            auto count = reg("CX");
            if (count == 0) return "REPE: Count is zero";
            
            const auto subop = upper(args[1]);
            bool equal = true;
            
            while (reg("CX") > 0 && equal) {
                if (subop == "CMPSB") {
                    equal = cmpsb();
                } else if (subop == "SCASB") {
                    equal = scasb();
                } else {
                    return "REPE only supports CMPSB/SCASB";
                }
                reg("CX")--;
            }
            
            zero_ = equal;
            std::ostringstream out;
            out << "REPE " << subop << " " << (equal ? "found match" : "stopped at mismatch");
            return debug_enabled_ ? out.str() + " " + flags() : out.str();
        }
        
        if ((op == "REPNE" || op == "REPNZ") && args.size() == 2) {
            auto count = reg("CX");
            if (count == 0) return "REPNE: Count is zero";
            
            const auto subop = upper(args[1]);
            bool not_equal = true;
            
            while (reg("CX") > 0 && not_equal) {
                if (subop == "CMPSB") {
                    not_equal = !cmpsb();
                } else if (subop == "SCASB") {
                    not_equal = !scasb();
                } else {
                    return "REPNE only supports CMPSB/SCASB";
                }
                reg("CX")--;
            }
            
            zero_ = !not_equal;
            std::ostringstream out;
            out << "REPNE " << subop << " " << (!not_equal ? "found match" : "no match found");
            return debug_enabled_ ? out.str() + " " + flags() : out.str();
        }
        
        // Загрузка/сохранение в память
        if ((op == "LOAD" || op == "STORE") && args.size() == 3) {
            const auto address = readAddress(args[2]);
            if (op == "LOAD") {
                reg(args[1]) = storage_.read16(address);
                updateZeroSign(reg(args[1]));
            } else {
                storage_.write16(address, reg(args[1]));
            }
            return okResult();
        }
        
        // Работа с байтами
        if (op == "POKE" && args.size() == 3) {
            storage_.write8(readAddress(args[1]), static_cast<std::uint8_t>(readValue(args[2]) & 0xff));
            return "OK";
        }
        
        if (op == "PEEK" && args.size() == 2) {
            std::ostringstream out;
            out << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<int>(storage_.read8(readAddress(args[1])));
            return out.str();
        }
        
        // Дамп памяти
        if (op == "DUMP" && args.size() == 3) {
            return storage_.dump(readAddress(args[1]), readAddress(args[2]));
        }
        
        // ASCII поддержка
        if (op == "ASCII" && args.size() == 2) {
            const auto& str = args[1];
            std::ostringstream out;
            for (char c : str) {
                out << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<int>(static_cast<unsigned char>(c)) << ' ';
            }
            return out.str();
        }
        
        if (op == "CHAR" && args.size() == 2) {
            const auto code = readValue(args[1]) & 0xFF;
            if (code >= 32 && code <= 126) {
                return std::string(1, static_cast<char>(code));
            } else {
                return std::string("Non-printable character (0x") + 
                       std::to_string(code) + ")";
            }
        }
        
        if (op == "STRING" && args.size() >= 2) {
            const auto address = readAddress(args[1]);
            std::string str;
            for (std::size_t i = 2; i < args.size(); ++i) {
                if (i > 2) str += " ";
                str += args[i];
            }
            writeString(address, str);
            std::ostringstream out;
            out << "String '" << str << "' written at 0x" << std::hex << address;
            return debug_enabled_ ? out.str() : "OK";
        }
        
        if (op == "PRINT" && args.size() == 2) {
            const auto address = readAddress(args[1]);
            const auto str = readString(address);
            return debug_enabled_ ? "String: " + str : str;
        }

        if (op == "INPUT" && args.size() == 2) {
            return inputValue(readAddress(args[1]));
        }
        
        // Сдвиги
        if ((op == "SHL" || op == "SHR" || op == "ROL" || op == "ROR") && args.size() == 3) {
            auto& dst = reg(args[1]);
            const auto shift = static_cast<std::uint8_t>(readValue(args[2]) & 0x0f);
            
            if (op == "SHL") {
                const auto result = static_cast<std::uint32_t>(dst) << shift;
                carry_ = (result & 0x10000) != 0;
                dst = static_cast<std::uint16_t>(result & 0xffff);
            } else if (op == "SHR") {
                carry_ = (dst & (1 << (shift - 1))) != 0;
                dst = static_cast<std::uint16_t>(dst >> shift);
            } else if (op == "ROL") {
                for (std::uint8_t i = 0; i < shift; ++i) {
                    const std::uint16_t high_bit = (dst & 0x8000) != 0;
                    dst = static_cast<std::uint16_t>((dst << 1) | (high_bit ? 1 : 0));
                }
                carry_ = (dst & 0x8000) != 0;
            } else if (op == "ROR") {
                for (std::uint8_t i = 0; i < shift; ++i) {
                    const std::uint16_t low_bit = (dst & 0x0001) != 0;
                    dst = static_cast<std::uint16_t>((dst >> 1) | (low_bit ? 0x8000 : 0));
                }
                carry_ = (dst & 0x8000) != 0;
            }
            overflow_ = false;
            updateZeroSign(dst);
            return okResult();
        }
        
        return "Unknown command or wrong argument count. Type HELP.";
    } catch (const std::exception& ex) {
        return std::string("Error: ") + ex.what();
    }
}

std::string Cpu::currentDriveName() const {
    return formatPath(current_drive_, currentFs().cwd);
}

Cpu::DriveFs& Cpu::currentFs() {
    return filesystems_[current_drive_];
}

const Cpu::DriveFs& Cpu::currentFs() const {
    return filesystems_.at(current_drive_);
}

std::vector<std::string> Cpu::normalizePath(const std::string& path) const {
    std::vector<std::string> parts;
    std::string rest = path;
    if (rest.size() >= 2 && rest[1] == ':') {
        rest = rest.substr(2);
    } else {
        parts = currentFs().cwd;
    }
    if (!rest.empty() && (rest[0] == '\\' || rest[0] == '/')) {
        parts.clear();
        rest.erase(0, 1);
    }
    std::replace(rest.begin(), rest.end(), '/', '\\');
    std::stringstream ss(rest);
    std::string item;
    while (std::getline(ss, item, '\\')) {
        if (item.empty() || item == ".") continue;
        if (item == "..") {
            if (!parts.empty()) parts.pop_back();
            continue;
        }
        parts.push_back(upper(item));
    }
    return parts;
}

std::string Cpu::formatPath(char drive, const std::vector<std::string>& parts) const {
    std::string out;
    out += drive;
    out += ":\\";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += "\\";
        out += parts[i];
    }
    return out;
}

Cpu::FsNode* Cpu::resolveNode(const std::string& path) {
    auto parts = normalizePath(path);
    FsNode* node = &currentFs().root;
    for (const auto& part : parts) {
        auto it = node->children.find(part);
        if (it == node->children.end()) return nullptr;
        node = it->second.get();
    }
    return node;
}

const Cpu::FsNode* Cpu::resolveNode(const std::string& path) const {
    return const_cast<Cpu*>(this)->resolveNode(path);
}

Cpu::FsNode* Cpu::resolveParent(const std::string& path, std::string& leaf) {
    auto parts = normalizePath(path);
    if (parts.empty()) return nullptr;
    leaf = parts.back();
    parts.pop_back();
    FsNode* node = &currentFs().root;
    for (const auto& part : parts) {
        auto it = node->children.find(part);
        if (it == node->children.end() || !it->second->directory) return nullptr;
        node = it->second.get();
    }
    return node;
}

std::string Cpu::listDirectory(const std::string& path) const {
    const FsNode* node = resolveNode(path);
    if (!node) return "File Not Found";
    if (!node->directory) return path;
    std::ostringstream out;
    out << " Directory of " << currentDriveName() << "\n\n";
    out << "<DIR>          .\n<DIR>          ..\n";
    for (const auto& [name, child] : node->children) {
        if (child->directory) out << "<DIR>          " << name << '\n';
        else out << std::setw(14) << std::setfill(' ') << child->content.size() << " " << name << '\n';
    }
    return out.str();
}

std::string Cpu::changeDirectory(const std::string& path) {
    FsNode* node = resolveNode(path);
    if (!node || !node->directory) return "Invalid directory";
    currentFs().cwd = normalizePath(path);
    return currentDriveName();
}

std::string Cpu::makeDirectory(const std::string& path) {
    std::string leaf;
    FsNode* parent = resolveParent(path, leaf);
    if (!parent) return "Path not found";
    if (parent->children.count(leaf)) return "A subdirectory or file already exists";
    auto node = std::make_unique<FsNode>();
    node->directory = true;
    parent->children[leaf] = std::move(node);
    persistCurrentFs();
    return "Directory created";
}

std::string Cpu::removeDirectory(const std::string& path) {
    std::string leaf;
    FsNode* parent = resolveParent(path, leaf);
    if (!parent) return "Path not found";
    auto it = parent->children.find(leaf);
    if (it == parent->children.end() || !it->second->directory) return "Invalid directory";
    if (!it->second->children.empty()) return "The directory is not empty";
    parent->children.erase(it);
    persistCurrentFs();
    return "Directory removed";
}

std::string Cpu::writeFile(const std::string& path, const std::string& content, bool append) {
    std::string leaf;
    FsNode* parent = resolveParent(path, leaf);
    if (!parent) return "Path not found";
    auto& slot = parent->children[leaf];
    if (!slot) { slot = std::make_unique<FsNode>(); slot->directory = false; }
    if (slot->directory) return "Access denied";
    if (append) slot->content += content; else slot->content = content;
    persistCurrentFs();
    return "1 file(s) written";
}

std::string Cpu::readFile(const std::string& path) const {
    const FsNode* node = resolveNode(path);
    if (!node || node->directory) return "File Not Found";
    return node->content;
}

std::string Cpu::deleteFile(const std::string& path) {
    std::string leaf;
    FsNode* parent = resolveParent(path, leaf);
    if (!parent) return "Path not found";
    auto it = parent->children.find(leaf);
    if (it == parent->children.end() || it->second->directory) return "File Not Found";
    parent->children.erase(it);
    persistCurrentFs();
    return "1 file(s) deleted";
}

std::string Cpu::copyFile(const std::string& from, const std::string& to) {
    const FsNode* src = resolveNode(from);
    if (!src || src->directory) return "File Not Found";
    const auto result = writeFile(to, src->content, false);
    return result == "1 file(s) written" ? "1 file(s) copied" : result;
}

std::string Cpu::joinArgs(const std::vector<std::string>& args, std::size_t first) const {
    std::string out;
    for (std::size_t i = first; i < args.size(); ++i) {
        if (i > first) out += ' ';
        out += args[i];
    }
    return out;
}

std::string Cpu::createProgram(const std::string& path) {
    const auto result = writeFile(path, "", false);
    return result == "1 file(s) written" ? "Program file created" : result;
}

std::string Cpu::appendProgramLine(const std::string& path, const std::string& command) {
    if (command.empty()) return "The syntax of the command is incorrect.";
    const auto result = writeFile(path, command + "\n", true);
    return result == "1 file(s) written" ? "Program line added" : result;
}

std::string Cpu::runProgram(const std::string& path) {
    const FsNode* node = resolveNode(path);
    if (!node || node->directory) return "Program not found";

    std::istringstream input(node->content);
    std::ostringstream out;
    std::string command;
    std::size_t executed = 0;
    static constexpr std::size_t kMaxProgramCommands = 1000;

    out << "Running " << path << " from " << currentDriveName() << "\n";
    while (std::getline(input, command)) {
        command.erase(0, command.find_first_not_of(" \t\r\n"));
        const auto end = command.find_last_not_of(" \t\r\n");
        if (end == std::string::npos) continue;
        command.erase(end + 1);
        if (command.empty() || command[0] == ';' || command[0] == '#') continue;
        if (++executed > kMaxProgramCommands) {
            out << "Program stopped: command limit reached";
            return out.str();
        }
        const auto result = execute(command);
        out << "> " << command << "\n" << result << "\n";
        if (upper(command) == "EXIT" || upper(command) == "QUIT") break;
    }
    out << "Program finished: " << executed << " command(s) executed";
    return out.str();
}

void Cpu::persistCurrentFs() {
    persistFs(current_drive_);
}

void Cpu::persistFs(char drive) {
    auto diskIt = disk_map_.find(drive);
    auto fsIt = filesystems_.find(drive);
    if (diskIt == disk_map_.end() || fsIt == filesystems_.end() || diskIt->second >= disks_.size()) return;
    Disk* disk = disks_[diskIt->second];
    if (!disk || !disk->isMounted()) return;

    std::ostringstream serialized;
    serialized << "EMUFS1\n";
    serializeNode(serialized, fsIt->second.root, "");
    const auto data = serialized.str();
    const auto maxBytes = static_cast<std::size_t>((Disk::NUM_SECTORS - 1) * Disk::SECTOR_SIZE);
    if (data.size() > maxBytes) throw std::runtime_error("emulated file system is full");

    std::size_t offset = 0;
    for (std::uint32_t sector = 1; sector < Disk::NUM_SECTORS && offset < data.size(); ++sector) {
        std::vector<uint8_t> bytes(Disk::SECTOR_SIZE, 0);
        const auto chunk = std::min<std::size_t>(Disk::SECTOR_SIZE, data.size() - offset);
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(offset), chunk, bytes.begin());
        disk->writeSector(sector, bytes);
        offset += chunk;
    }
}

void Cpu::loadFilesystemsFromDisks() {
    for (const auto& [drive, index] : disk_map_) {
        if (index >= disks_.size() || !disks_[index] || !disks_[index]->isMounted()) continue;
        std::string data;
        for (std::uint32_t sector = 1; sector < Disk::NUM_SECTORS; ++sector) {
            auto bytes = disks_[index]->readSector(sector);
            for (auto byte : bytes) {
                if (byte == 0) {
                    sector = Disk::NUM_SECTORS;
                    break;
                }
                data += static_cast<char>(byte);
            }
        }
        if (data.rfind("EMUFS1\n", 0) == 0) deserializeFs(drive, data);
    }
}

void Cpu::serializeNode(std::ostringstream& out, const FsNode& node, const std::string& path) const {
    for (const auto& [name, child] : node.children) {
        const auto childPath = path.empty() ? name : path + "\\" + name;
        if (child->directory) {
            out << "D|" << childPath << "\n";
            serializeNode(out, *child, childPath);
        } else {
            out << "F|" << childPath << "|" << hexEncode(child->content) << "\n";
        }
    }
}

void Cpu::deserializeFs(char drive, const std::string& data) {
    auto& fs = filesystems_[drive];
    fs.root.children.clear();
    std::istringstream input(data);
    std::string line;
    std::getline(input, line); // signature
    while (std::getline(input, line)) {
        if (line.size() < 3 || line[1] != '|') continue;
        const char type = line[0];
        if (type == 'D') {
            const char previousDrive = current_drive_;
            current_drive_ = drive;
            makeDirectory(line.substr(2));
            current_drive_ = previousDrive;
        } else if (type == 'F') {
            const auto sep = line.find('|', 2);
            if (sep == std::string::npos) continue;
            const char previousDrive = current_drive_;
            current_drive_ = drive;
            writeFile(line.substr(2, sep - 2), hexDecode(line.substr(sep + 1)), false);
            current_drive_ = previousDrive;
        }
    }
}

std::string Cpu::hexEncode(const std::string& data) {
    std::ostringstream out;
    for (unsigned char c : data) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    return out.str();
}

std::string Cpu::hexDecode(const std::string& hex) {
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out += static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16));
    }
    return out;
}

// Disk operations implementation
std::string Cpu::diskInfo() const {
    std::ostringstream out;
    out << "Available disks:\n";
    for (const auto& pair : disk_map_) {
        char letter = pair.first;
        size_t idx = pair.second;
        if (idx < disks_.size() && disks_[idx] != nullptr) {
            const Disk& disk = *disks_[idx];
            out << "  " << letter << ": " << disk.getName() 
                << " (" << (disk.isMounted() ? "mounted" : "unmounted") 
                << ", " << Disk::NUM_SECTORS << " sectors)\n";
            if (disk.isMounted()) {
                out << "    " << disk.getInfo() << "\n";
            }
        }
    }
    return out.str();
}

std::string Cpu::formatDisk(const std::string& diskName) {
    if (diskName.empty()) {
        return "Error: No disk specified";
    }
    const char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(diskName[0])));
    
    auto it = disk_map_.find(drive);
    if (it == disk_map_.end()) {
        return "Error: Disk " + diskName + " not found";
    }
    
    if (it->second >= disks_.size() || disks_[it->second] == nullptr) {
        return "Error: Disk not available";
    }
    
    try {
        disks_[it->second]->format();
        disks_[it->second]->mount();
        filesystems_[drive].root.children.clear();
        filesystems_[drive].cwd.clear();
        persistFs(drive);
        return "Disk " + diskName + " formatted and mounted successfully";
    } catch (const std::exception& e) {
        return std::string("Error formatting disk: ") + e.what();
    }
}

std::string Cpu::readDisk(const std::string& diskName, uint32_t sector) {
    if (diskName.empty()) {
        return "Error: No disk specified";
    }
    
    auto it = disk_map_.find(diskName[0]);
    if (it == disk_map_.end()) {
        return "Error: Disk " + diskName + " not found";
    }
    
    if (it->second >= disks_.size() || disks_[it->second] == nullptr) {
        return "Error: Disk not available";
    }
    
    try {
        auto data = disks_[it->second]->readSector(sector);
        std::ostringstream out;
        out << "Sector " << sector << " (hex dump):\n";
        for (size_t i = 0; i < data.size(); ++i) {
            if (i % 16 == 0) {
                if (i > 0) out << '\n';
                out << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
            }
            out << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<int>(data[i]) << ' ';
        }
        return out.str();
    } catch (const std::exception& e) {
        return std::string("Error reading disk: ") + e.what();
    }
}

std::string Cpu::writeDisk(const std::string& diskName, uint32_t sector, const std::string& data) {
    if (diskName.empty()) {
        return "Error: No disk specified";
    }
    
    auto it = disk_map_.find(diskName[0]);
    if (it == disk_map_.end()) {
        return "Error: Disk " + diskName + " not found";
    }
    
    if (it->second >= disks_.size() || disks_[it->second] == nullptr) {
        return "Error: Disk not available";
    }
    
    try {
        std::vector<uint8_t> bytes;
        bytes.reserve(Disk::SECTOR_SIZE);
        
        std::string processed_data = data;
        if (processed_data.size() >= 2 && processed_data.front() == '"' && processed_data.back() == '"') {
            processed_data = processed_data.substr(1, processed_data.size() - 2);
        }
        
        for (char c : processed_data) {
            bytes.push_back(static_cast<uint8_t>(c));
        }
        
        if (bytes.size() > Disk::SECTOR_SIZE) {
            bytes.resize(Disk::SECTOR_SIZE);
        } else {
            bytes.resize(Disk::SECTOR_SIZE, 0);
        }
        
        disks_[it->second]->writeSector(sector, bytes);
        
        std::ostringstream out;
        out << "Data written to sector " << sector << " on disk " << diskName;
        out << " (" << processed_data.length() << " bytes)";
        return out.str();
    } catch (const std::exception& e) {
        return std::string("Error writing disk: ") + e.what();
    }
}

// Реализация строковых операций
void Cpu::movsb() {
    const auto src = reg("SI");
    const auto dst = reg("DI");
    storage_.write8(dst, storage_.read8(src));
    reg("SI")++;
    reg("DI")++;
}

void Cpu::movsw() {
    const auto src = reg("SI");
    const auto dst = reg("DI");
    storage_.write16(dst, storage_.read16(src));
    reg("SI") += 2;
    reg("DI") += 2;
}

bool Cpu::cmpsb() {
    const auto src = reg("SI");
    const auto dst = reg("DI");
    const auto byte1 = storage_.read8(src);
    const auto byte2 = storage_.read8(dst);
    reg("SI")++;
    reg("DI")++;
    return byte1 == byte2;
}

bool Cpu::scasb() {
    const auto target = reg("AX") & 0xFF;
    const auto dst = reg("DI");
    const auto value = storage_.read8(dst);
    reg("DI")++;
    return target == value;
}

void Cpu::stosb() {
    const auto dst = reg("DI");
    storage_.write8(dst, static_cast<std::uint8_t>(reg("AX") & 0xFF));
    reg("DI")++;
}

void Cpu::lodsb() {
    const auto src = reg("SI");
    const auto value = storage_.read8(src);
    reg("AX") = (reg("AX") & 0xFF00) | value;
    reg("SI")++;
}

std::string Cpu::readString(std::uint32_t address) const {
    std::string result;
    std::uint8_t byte;
    std::uint32_t addr = address;
    do {
        byte = storage_.read8(addr++);
        if (byte != 0) {
            result += static_cast<char>(byte);
        }
    } while (byte != 0);
    return result;
}

void Cpu::writeString(std::uint32_t address, const std::string& str) {
    for (std::size_t i = 0; i < str.length(); ++i) {
        storage_.write8(address + i, static_cast<unsigned char>(str[i]));
    }
    storage_.write8(address + str.length(), 0);
}

std::string Cpu::inputValue(std::uint32_t address) {
    if (!input_reader_) {
        return "Error: input is not available";
    }

    const auto value = input_reader_();
    writeString(address, value);

    std::ostringstream out;
    out << "Input written at 0x" << std::hex << address;
    return debug_enabled_ ? out.str() : "OK";
}

// Проверка условий для условных прыжков
bool Cpu::checkCondition(const std::string& condition) {
    if (condition == "JMP") return true;
    if (condition == "JE" || condition == "JZ") return zero_;
    if (condition == "JNE" || condition == "JNZ") return !zero_;
    if (condition == "JG" || condition == "JNLE") return !zero_ && !carry_;
    if (condition == "JL" || condition == "JNGE") return carry_;
    if (condition == "JGE" || condition == "JNL") return !carry_;
    if (condition == "JLE" || condition == "JNG") return zero_ || carry_;
    if (condition == "JA" || condition == "JNBE") return !carry_ && !zero_;
    if (condition == "JB" || condition == "JNAE") return carry_;
    if (condition == "JAE" || condition == "JNB") return !carry_;
    if (condition == "JBE" || condition == "JNA") return carry_ || zero_;
    if (condition == "JC") return carry_;
    if (condition == "JNC") return !carry_;
    if (condition == "JO") return overflow_;
    if (condition == "JNO") return !overflow_;
    if (condition == "JS") return sign_;
    if (condition == "JNS") return !sign_;
    return false;
}

std::uint16_t& Cpu::reg(const std::string& name) {
    static const std::unordered_map<std::string, RegisterIndex> map{
        {"AX", AX}, {"BX", BX}, {"CX", CX}, {"DX", DX},
        {"SP", SP}, {"BP", BP}, {"SI", SI}, {"DI", DI},
        {"CS", CS}, {"DS", DS}, {"ES", ES}, {"SS", SS}
    };
    const auto it = map.find(upper(name));
    if (it == map.end()) throw std::invalid_argument("unknown register: " + name);
    return registers_[it->second];
}

const std::uint16_t& Cpu::reg(const std::string& name) const {
    return const_cast<Cpu*>(this)->reg(name);
}

std::uint16_t Cpu::readValue(const std::string& token) const {
    const auto normalized = upper(token);
    if (normalized == "AX" || normalized == "BX" || normalized == "CX" || 
        normalized == "DX" || normalized == "SP" || normalized == "BP" || 
        normalized == "SI" || normalized == "DI" || normalized == "CS" ||
        normalized == "DS" || normalized == "ES" || normalized == "SS") {
        return reg(normalized);
    }
    if (detectArgType(token) == STRING) {
        const std::string str = unquote(token);
        if (!str.empty()) {
            return static_cast<uint16_t>(static_cast<unsigned char>(str[0]));
        }
        return 0;
    }
    return static_cast<std::uint16_t>(parseNumber(token) & 0xffff);
}

std::vector<std::string> Cpu::split(const std::string& line) {
    std::vector<std::string> result;
    std::string current;
    bool in_quotes = false;
    bool escape = false;
    
    for (char ch : line) {
        if (escape) {
            if (ch == 'n') current += '\n';
            else if (ch == 't') current += '\t';
            else if (ch == 'r') current += '\r';
            else if (ch == '\\') current += '\\';
            else if (ch == '"') current += '"';
            else current += ch;
            escape = false;
            continue;
        }
        
        if (ch == '\\') {
            escape = true;
            continue;
        }
        
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        
        if (ch == ' ' || ch == '\t' || ch == ',') {
            if (in_quotes) {
                current += ch;
            } else if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
            continue;
        }
        
        current += ch;
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}

std::string Cpu::upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), 
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
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
    out << "ZF=" << zero_ << " CF=" << carry_ << " SF=" << sign_ << " OF=" << overflow_;
    return out.str();
}

std::string Cpu::okResult() const {
    return debug_enabled_ ? "OK " + flags() : "OK";
}

std::string Cpu::flagsResult() const {
    return debug_enabled_ ? flags() : "OK";
}

void Cpu::updateZeroSign(std::uint16_t value) {
    zero_ = value == 0;
    sign_ = (value & 0x8000) != 0;
}

void Cpu::setFlagsForComparison(int result) {
    zero_ = (result == 0);
    sign_ = (result < 0);
    carry_ = (result < 0 || result > 0xFFFF);
}

std::string Cpu::help() const {
    return "===== 16-bit CPU Emulator with Real Disk Support =====\n"
           "\nDOS-LIKE FILE SYSTEM:\n"
           "  C: / D: / E: / F: - Switch current drive\n"
           "  DIR [PATH]        - List files and directories\n"
           "  CD [PATH]         - Show/change current directory\n"
           "  MD/MKDIR PATH     - Create directory\n"
           "  RD/RMDIR PATH     - Remove empty directory\n"
           "  ECHO TEXT > FILE  - Write file, >> appends\n"
           "  TYPE FILE         - Print file contents\n"
           "  COPY SRC DST      - Copy file\n"
           "  DEL/ERASE FILE    - Delete file\n"
           "  NEW FILE          - Create an empty program file\n"
           "  APPEND FILE CMD   - Add one command to a program\n"
           "  RUN/EXEC FILE     - Load and execute a program from disk\n"
           "\nDISK OPERATIONS:\n"
           "  DISKS             - Show all available disks\n"
           "  FORMAT X          - Format disk X (C, D, E, or F)\n"
           "  READ X, SECTOR    - Read sector from disk X\n"
           "  WRITE X, SECTOR, data - Write data to sector\n"
           "\nREGISTER OPERATIONS:\n"
           "  DEBUG/DBG ON|OFF  - Enable/disable debug output (enabled by default)\n"
           "  REGS              - Show all registers\n"
           "  RESET             - Reset CPU and clear memory\n"
           "\nDATA MOVEMENT:\n"
           "  MOV R, V          - Move value to register\n"
           "  LOAD R, ADDR      - Load from memory\n"
           "  STORE R, ADDR     - Store to memory\n"
           "  PEEK ADDR         - Read byte\n"
           "  POKE ADDR, V      - Write byte\n"
           "\nARITHMETIC & LOGICAL:\n"
           "  ADD/SUB R, V      - Add/Subtract\n"
           "  INC/DEC R         - Increment/Decrement\n"
           "  CMP R, V          - Compare\n"
           "  AND/OR/XOR R, V   - Bitwise operations\n"
           "  NOT R             - Bitwise NOT\n"
           "  TEST R, V         - Test bits\n"
           "  SHL/SHR R, V      - Shift left/right\n"
           "  ROL/ROR R, V      - Rotate left/right\n"
           "\nCONDITIONAL JUMPS:\n"
           "  JMP ADDR          - Unconditional\n"
           "  JE/JZ ADDR        - Equal/Zero\n"
           "  JNE/JNZ ADDR      - Not equal\n"
           "  JG/JL/JGE/JLE     - Signed comparisons\n"
           "  JA/JB/JAE/JBE     - Unsigned comparisons\n"
           "  JC/JNC/JO/JNO/JS/JNS - Flag checks\n"
           "\nSTRING OPERATIONS:\n"
           "  MOVSB/MOVSW       - Move byte/word\n"
           "  CMPSB             - Compare bytes\n"
           "  SCASB             - Scan byte\n"
           "  STOSB             - Store byte\n"
           "  LODSB             - Load byte\n"
           "  REP/REPE/REPNE OP - Repeat operations\n"
           "\nTEXT & MEMORY:\n"
           "  ASCII \"text\"      - Show hex codes\n"
           "  CHAR V            - Show character\n"
           "  STRING ADDR \"text\" - Write string\n"
           "  INPUT ADDR        - Read keyboard input into memory\n"
           "  PRINT ADDR        - Read string\n"
           "  DUMP ADDR, LEN    - Dump memory\n"
           "\nREGISTERS: AX BX CX DX SP BP SI DI CS DS ES SS\n"
           "FLAGS: ZF (zero), CF (carry), SF (sign), OF (overflow)\n"
           "NUMBERS: decimal or 0xHEX\n"
           "MEMORY: [address] or register\n"
           "DISKS: C:, D:, E:, F:\n"
           "\nSYSTEM: EXIT, QUIT, CLEAR, CLS\n";
}
