#include "../include/dual_console.hpp"

#include <iostream>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

DualConsole::DualConsole() = default;
DualConsole::~DualConsole() = default;

void DualConsole::init() {
    clear();
    status_bar_ = "CPU Emulator v2.0 | Type HELP for commands | DISKS for disk info";
}

void DualConsole::clear() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void DualConsole::setStatusBar(const std::string& status) {
    status_bar_ = status;
}

void DualConsole::renderConsole() {
    // Clear and render the console layout
    clear();
    
    // Top border
    std::cout << "\033[1;33m" << "=" << std::string(78, '=') << "=" << "\033[0m\n";
    
    // History area (scrollable)
    int line_count = 0;
    int max_lines = 20; // Adjust based on terminal height
    
    // Show only last max_lines entries
    auto start = cpu_history_.size() > static_cast<size_t>(max_lines) 
                 ? cpu_history_.size() - max_lines : 0;
    
    for (size_t i = start; i < cpu_history_.size(); ++i) {
        std::cout << cpu_history_[i];
        if (i < cpu_history_.size() - 1) std::cout << '\n';
    }
    
    // Separator
    std::cout << "\n\033[1;34m" << std::string(80, '-') << "\033[0m\n";
    
    // Status bar
    std::cout << "\033[1;32m" << status_bar_ << "\033[0m\n";
    
    // Prompt
    std::cout << "\033[1;37m> \033[0m";
    std::cout.flush();
}

void DualConsole::printCpu(const std::string& msg) {
    cpu_history_.push_back(msg);
    if (cpu_history_.size() > MAX_HISTORY) {
        cpu_history_.pop_front();
    }
}

void DualConsole::addCpuHistory(const std::string& cmd, const std::string& result) {
    // Split result into lines for better display
    std::string display = cmd;
    if (!result.empty()) {
        display += "\n  " + result;
    }
    printCpu(display);
}

std::string DualConsole::getCpuInput() {
    std::cout << "\033[1;37m> \033[0m";
    std::cout.flush();
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void DualConsole::render() {
    renderConsole();
}