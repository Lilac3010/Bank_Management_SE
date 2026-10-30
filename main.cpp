
#include <iostream>
#include <string>

#include "account.h"
#include "account_search.h"
#include "auth.h"
#include "transactions.h"
#include "balance_enquiry.h"
#include "mini_statement.h"

namespace {

void employeeMenu() {
    while (true) {
        std::cout << "\n=== Employee Menu ===\n"
                  << "1. Create Account\n"
                  << "2. Search Account\n"
                  << "0. Back to Main Menu\n"
                  << "Enter choice: ";

        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "0") {
            return;
        }

        if (choice == "1") {
            createAccount();
        } else if (choice == "2") {
            accountSearch();
        } else {
            std::cout << "Invalid choice. Please try again.\n";
        }
    }
}

} // namespace

int main() {
    while (true) {
        std::cout << "\n=== Bank Management System ===\n"
                  << "1. Employee Login\n"
                  << "2. Deposit Money\n"
                  << "3. Withdraw Money\n"
                  << "4. Balance Enquiry\n"
                  << "5. Mini-Statement\n"
                  << "0. Exit\n"
                  << "Enter choice: ";

        std::string choice;
        if (!std::getline(std::cin, choice) || choice == "0") {
            break;
        }

        if (choice == "1") {
            if (loginEmployee()) {
                employeeMenu();
            } else {
                std::cout << "Returning to main menu.\n";
            }
        } else if (choice == "2") {
            depositMoney();
        } else if (choice == "3") {
            withdrawMoney();
        } else if (choice == "4") {
            balanceEnquiry();
        } else if (choice == "5") {
            miniStatement();
        } else {
            std::cout << "Invalid choice. Please try again.\n";
        }
    }

    return 0;
}
