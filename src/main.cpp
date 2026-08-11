#include "../include/cpu.hpp"
#include "../include/storage.hpp"
#include "../include/disk.hpp"
#include "../include/dual_console.hpp"

#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <memory>

int main() {
    try {
        DualConsole console;
        console.init();
        
        // Initialize storage
        Storage storage("memory.bin");
        
        // Initialize disks using unique_ptr
        std::vector<std::unique_ptr<Disk>> disk_ptrs;
        std::vector<Disk*> disks; // Raw pointers for Cpu
        
        std::vector<std::string> disk_names = {"disk_c.bin", "disk_d.bin", "disk_e.bin", "disk_f.bin"};
        char disk_letter = 'C';
        
        for (const auto& name : disk_names) {
            auto disk = std::make_unique<Disk>(std::string(1, disk_letter), name);
            disk->mount();
            disks.push_back(disk.get());
            disk_ptrs.push_back(std::move(disk));
            disk_letter++;
        }
        
        Cpu cpu(storage, disks);
        
        console.printCpu("===== CPU Emulator v2.0 with Real Disk Support =====");
        console.printCpu("Disks available: C:, D:, E:, F:");
        console.printCpu("Type HELP for commands or DISKS for disk info");
        console.printCpu("");
        
        bool running = true;
        std::string line;
        int command_count = 0;
        
        while (running) {
            line = console.getCpuInput();
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            
            if (line.empty()) {
                continue;
            }
            
            command_count++;
            
            // Check exit commands (case insensitive)
            std::string upperLine = line;
            std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), 
                          [](unsigned char c) { return std::toupper(c); });
            
            if (upperLine == "EXIT" || upperLine == "QUIT") {
                console.printCpu("");
                console.printCpu("Shutting down...");
                console.printCpu("Commands executed: " + std::to_string(command_count));
                running = false;
                break;
            }
            
            // Clear commands (case insensitive)
            if (upperLine == "CLEAR" || upperLine == "CLS") {
                console.init();
                console.printCpu("Screen cleared.");
                console.printCpu("CPU Emulator ready.");
                continue;
            }
            
            // Execute CPU command (pass original line, CPU will handle case)
            const auto result = cpu.execute(line);
            console.addCpuHistory(line, result);
            
            // Debug: if result is empty, show something
            if (result.empty()) {
                console.printCpu("  (Command returned empty result)");
            }
        }
        
        std::cout << "\nDone." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cin.get();
        return 1;
    }
}