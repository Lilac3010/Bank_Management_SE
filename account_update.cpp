#include "account_update.h"

#include "account_search.h"
#include "auth.h"
#include "models.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Remove leading and trailing spaces, as FR-1 does before validating (trim in account.cpp).
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Accept a whole line of digits only (surrounding spaces are ignored) for a positive
// number that fits in an int. Same rules as parseAccountNumber in account_search.cpp (FR-7).
bool parseAccountNumber(const std::string& text, int& accountNo) {
    const std::string digits = trim(text);
    if (digits.empty() || digits.size() > 10) {
        return false;
    }

    for (unsigned char ch : digits) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }

    const long long value = std::stoll(digits);
    if (value <= 0 || value > INT_MAX) {
        return false;
    }

    accountNo = static_cast<int>(value);
    return true;
}

// Validate customer name: letters, spaces, dots and apostrophes, with at least one
// letter. Same rule as isValidName in account.cpp (FR-1).
bool isValidName(const std::string& name) {
    bool hasLetter = false;

    for (unsigned char ch : name) {
        if (std::isalpha(ch)) {
            hasLetter = true;
        } else if (ch != ' ' && ch != '.' && ch != '\'') {
            return false;
        }
    }

    return hasLetter;
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

    // Same rules as FR-1 account creation: surrounding spaces are ignored, so a value
    // made only of spaces counts as empty.
    const std::string newName = trim(name);
    const std::string newAddress = trim(address);
    const std::string newContactNumber = trim(contactNumber);

    if (!isValidName(newName)) {
        return {false, "Invalid name. Use letters, spaces, dots or apostrophes."};
    }

    if (newAddress.empty() || newAddress.find('|') != std::string::npos) {
        return {false, "Invalid address."};
    }

    if (!isValidContactNumber(newContactNumber)) {
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

    if (isContactNumberInUse(accounts, accountNo, newContactNumber)) {
        return {false, "Contact number is already used by another active account."};
    }

    // Update only the permitted customer details.
    // Account number, PIN, balance and status remain unchanged.
    targetAccount->name = newName;
    targetAccount->address = newAddress;
    targetAccount->contactNumber = newContactNumber;

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

    std::string accountText;
    int accountNo = 0;
    std::cout << "Enter account number to update: ";

    // Read the whole line so input such as "100001abc" is rejected, not read as 100001.
    if (!std::getline(std::cin, accountText) ||
        !parseAccountNumber(accountText, accountNo)) {
        std::cout << "Invalid account number.\n";
        return false;
    }

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