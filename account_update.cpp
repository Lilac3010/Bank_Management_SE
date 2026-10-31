#include "account_update.h"

#include "account_search.h"
#include "auth.h"
#include "models.h"
#include "storage.h"

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
bool isContactNumberInUse(const std::vector<Account>& accounts,
                          int accountNo,
                          const std::string& contactNumber) {
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

UpdateAccountResult updateAccountDetails(int accountNo,
                                         const std::string& name,
                                         const std::string& address,
                                         const std::string& contactNumber) {
    if (accountNo <= 0) {
        return {false, "Invalid account number."};
    }

    if (!isValidName(name)) {
        return {false, "Invalid name. Use letters, spaces, dots or apostrophes."};
    }

    if (address.empty() || address.find('|') != std::string::npos) {
        return {false, "Invalid address."};
    }

    if (!isValidContactNumber(contactNumber)) {
        return {false, "Contact number must contain exactly 10 digits."};
    }

    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        return {false, "Unable to load account records."};
    }

    Account* targetAccount = nullptr;

    for (Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            targetAccount = &account;
            break;
        }
    }

    if (targetAccount == nullptr) {
        return {false, "Account not found."};
    }

    if (targetAccount->status != "ACTIVE") {
        return {false, "Cannot update a closed account."};
    }

    if (isContactNumberInUse(accounts, accountNo, contactNumber)) {
        return {false, "Contact number is already used by another active account."};
    }

    // Update only the permitted customer details.
    // Account number, PIN, balance and status remain unchanged.
    targetAccount->name = name;
    targetAccount->address = address;
    targetAccount->contactNumber = contactNumber;

    if (!saveAccount(*targetAccount)) {
        return {false, "Failed to save account updates."};
    }

    return {true, "Account details updated successfully."};
}

bool accountUpdate() {
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
            std::cout << "Contact number: " << account.contactNumber << '\n';
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