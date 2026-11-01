#include "auth.h"

#include "models.h"
#include "storage.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

// Demo credentials for the academic console application.
// Change these to the credentials agreed upon by your team.
const std::string EMPLOYEE_USERNAME = "admin";
const std::string EMPLOYEE_PASSWORD = "admin123";

bool employeeAuthenticated = false;

} // namespace

bool loginEmployee() {
    std::string username;
    std::string password;

    std::cout << "\n=== Employee Login ===\n";
    std::cout << "Username: ";
    if (!std::getline(std::cin, username)) {
        return false;
    }

    std::cout << "Password: ";
    if (!std::getline(std::cin, password)) {
        return false;
    }

    if (username == EMPLOYEE_USERNAME &&
        password == EMPLOYEE_PASSWORD) {
        employeeAuthenticated = true;
        std::cout << "Employee login successful.\n";
        return true;
    }

    employeeAuthenticated = false;
    std::cout << "Invalid username or password.\n";
    return false;
}

bool authenticateCustomer(int accountNo, const std::string& pin) {
    std::vector<Account> accounts;

    if (accountNo <= 0 || pin.empty() || !loadAccounts(accounts)) {
        std::cout << "Invalid account number or PIN.\n";
        return false;
    }

    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo &&
            account.status == STATUS_ACTIVE &&
            account.pin == pin) {
            return true;
        }
    }

    std::cout << "Invalid account number or PIN.\n";
    return false;
}

bool isEmployeeAuthenticated() {
    return employeeAuthenticated;
}