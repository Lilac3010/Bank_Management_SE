
#include "account_update.h"

#include "auth.h"
#include "models.h"
#include "storage.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Validate customer name: letters, spaces, dots and apostrophes.
bool isValidName(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    for (unsigned char ch : name) {
        if (!std::isalpha(ch) && ch != ' ' && ch != '.' && ch != '\'') {
            return false;
        }
    }

    return true;
}

// Validate contact number: exactly 10 digits.
bool isValidContactNumber(const std::string& contactNumber) {
    if (contactNumber.length() != 10) {
        return false;
    }

    for (unsigned char ch : contactNumber) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }

    return true;
}

// Check whether another active account already uses this contact number.
bool isContactNumberInUse(
    const std::vector<Account>& accounts,
    int accountNo,
    const std::string& contactNumber
) {
    for (const Account& account : accounts) {
        if (account.accountNumber != accountNo &&
            account.status == "ACTIVE" &&
            account.contactNumber == contactNumber) {
            return true;
        }
    }

    return false;
}

} // namespace

UpdateAccountResult updateAccountDetails(
    int accountNo,
    const std::string& name,
    const std::string& address,
    const std::string& contactNumber
) {
    // SEC-1: Only authenticated employees can update account details.
    if (!isEmployeeAuthenticated()) {
        return {false, "Access denied: employee login required."};
    }

    if (accountNo <= 0) {
        return {false, "Invalid account number."};
    }

    if (name.empty() || address.empty() || contactNumber.empty()) {
        return {
            false,
            "Name, address, and contact number cannot be empty."
        };
    }

    if (!isValidName(name)) {
        return {
            false,
            "Invalid name. Use letters, spaces, dots, or apostrophes only."
        };
    }

    if (!isValidContactNumber(contactNumber)) {
        return {
            false,
            "Invalid contact number. It must contain exactly 10 digits."
        };
    }

    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        return {false, "Unable to load account records."};
    }

    auto it = std::find_if(
        accounts.begin(),
        accounts.end(),
        [accountNo](const Account& account) {
            return account.accountNumber == accountNo &&
                   account.status == "ACTIVE";
        }
    );

    if (it == accounts.end()) {
        return {false, "Active account not found."};
    }

    if (isContactNumberInUse(accounts, accountNo, contactNumber)) {
        return {
            false,
            "Contact number already belongs to another active account."
        };
    }

    // Update the customer details.
    it->name = name;
    it->address = address;
    it->contactNumber = contactNumber;

    // Save the updated account using the existing FR-13 storage function.
    if (!saveAccount(*it)) {
        return {false, "Failed to save account details."};
    }

    return {true, "Account details updated successfully."};
}

bool accountUpdate() {
    // SEC-1: Only authenticated employees can access account update.
    if (!isEmployeeAuthenticated()) {
        std::cout << "Access denied. Employee login required.\n";
        return false;
    }

    int accountNo;

    std::cout << "Enter account number to update: ";

    if (!(std::cin >> accountNo)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "Invalid account number.\n";
        return false;
    }

    std::cin.ignore(10000, '\n');

    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        std::cout << "Unable to load account records.\n";
        return false;
    }

    bool found = false;

    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            if (account.status != "ACTIVE") {
                std::cout << "Cannot update a closed account.\n";
                return false;
            }

            found = true;

            std::cout << "\nCurrent customer details:\n";
            std::cout << "Name: " << account.name << '\n';
            std::cout << "Address: " << account.address << '\n';
            std::cout << "Contact number: "
                      << account.contactNumber << '\n';

            break;
        }
    }

    if (!found) {
        std::cout << "Account not found.\n";
        return false;
    }

    std::string name;
    std::string address;
    std::string contactNumber;

    std::cout << "\nEnter updated name: ";
    std::getline(std::cin, name);

    std::cout << "Enter updated address: ";
    std::getline(std::cin, address);

    std::cout << "Enter updated contact number: ";
    std::getline(std::cin, contactNumber);

    UpdateAccountResult result =
        updateAccountDetails(accountNo, name, address, contactNumber);

    std::cout << result.message << '\n';

    return result.success;
}
