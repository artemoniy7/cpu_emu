#pragma once

#include <string>
#include <deque>
#include <vector>

class DualConsole {
public:
    DualConsole();
    ~DualConsole();

    void init();
    void clear();
    void setStatusBar(const std::string& status);
    
    void printCpu(const std::string& msg);
    void addCpuHistory(const std::string& cmd, const std::string& result);
    std::string getCpuInput();
    void render();

private:
    std::deque<std::string> cpu_history_;
    std::string status_bar_;
    static constexpr size_t MAX_HISTORY = 100;
    
    void renderConsole();
};