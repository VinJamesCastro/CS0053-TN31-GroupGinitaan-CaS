#include <iostream>
#include <string>

namespace cas {

void runMenu() {
    std::string choice;

    while (true) {
        std::cout << "\n=== Cents and Sensibility ===\n"
                  << "1. Login\n"
                  << "2. Register account\n"
                  << "3. About\n"
                  << "4. Exit\n"
                  << "Choose an option: ";

        if (!std::getline(std::cin, choice)) {
            std::cout << "\nInput closed. Goodbye!\n";
            return;
        }

        if (choice == "1") {
            std::cout << "Login is coming next.\n";
        } else if (choice == "2") {
            std::cout << "Registration is coming next.\n";
        } else if (choice == "3") {
            std::cout << "CaS is a C++ banking simulation created by Group Ginitaan.\n";
        } else if (choice == "4") {
            std::cout << "Goodbye!\n";
            return;
        } else {
            std::cout << "Invalid choice. Please enter 1, 2, 3, or 4.\n";
        }
    }
}

}  // namespace cas