#include <iostream>
#include <string>

#include "account.h"
#include "auth.h"
#include "transactions.h"

namespace {

void employeeMenu() {
    while (true) {
        std::cout << "\n=== Employee Menu ===\n"
                  << "1. Create Account\n"
                  << "0. Back to Main Menu\n"
                  << "Enter choice: ";
        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "0") return;
        if (choice == "1") createAccount();
        else std::cout << "Invalid choice. Please try again.\n";
    }
}

} // namespace

int main() {
    while (true) {
        std::cout << "\n=== Bank Management System ===\n"
                  << "1. Employee Login\n"
                  << "2. Deposit Money\n"
                  << "0. Exit\n"
                  << "Enter choice: ";
        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "0") break;
        if (choice == "1") {
            if (loginEmployee()) employeeMenu();
            else std::cout << "Returning to main menu.\n";
        } else if (choice == "2") {
            depositMoney();
        } else {
            std::cout << "Invalid choice. Please try again.\n";
        }
    }
    return 0;
}
