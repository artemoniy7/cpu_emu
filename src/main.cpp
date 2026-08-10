#include "cpu.hpp"
#include "storage.hpp"

#include <iostream>
#include <string>

int main() {
    Storage storage;
    Cpu cpu(storage);

    std::cout << "16-bit CPU emulator with 10 MB storage\n";
    std::cout << "Type HELP for commands or EXIT to quit.\n";

    for (std::string line; std::cout << "> " && std::getline(std::cin, line);) {
        const auto response = cpu.execute(line);
        if (response == "EXIT") {
            break;
        }
        if (!response.empty()) {
            std::cout << response << '\n';
        }
    }

    std::cout << "Bye!\n";
    return 0;
}
