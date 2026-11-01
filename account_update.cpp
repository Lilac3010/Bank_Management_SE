#include "account_update.h"

#include "auth.h"
#include "models.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Remove leading and trailing spaces, as FR-1 does before validating.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Accept a whole line of digits only for a positive number that fits in an int.
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

// Validate customer name: letters, spaces, dots and apostrophes,
// with at least one letter.
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
        return {false, "Error: Account number must be a positive whole number."};
    }

    // Same rules as FR-1 account creation: surrounding spaces are ignored.
    const std::string newName = trim(name);
    const std::string newAddress = trim(address);
    const std::string newContactNumber = trim(contactNumber);

    if (!isValidName(newName)) {
        return {
            false,
            "Error: Invalid name. Use letters, spaces, dots or apostrophes."
        };
    }

    if (newAddress.empty() || newAddress.find('|') != std::string::npos) {
        return {false, "Error: Invalid address."};
    }

    if (!isValidContactNumber(newContactNumber)) {
        return {
            false,
            "Error: Contact number must contain exactly 10 digits."
        };
    }

    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        return {false, "Storage error: could not read account data."};
    }

    Account* targetAccount = nullptr;

    for (Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            targetAccount = &account;
            break;
        }
    }

    if (targetAccount == nullptr) {
        return {false, "Error: Account not found."};
    }

    if (targetAccount->status != "ACTIVE") {
        return {false, "Error: Cannot update a closed account."};
    }

    if (isContactNumberInUse(accounts, accountNo, newContactNumber)) {
        return {
            false,
            "Error: Contact number is already used by another active account."
        };
    }

    // Update only the permitted customer details.
    // Account number, PIN, balance and status remain unchanged.
    targetAccount->name = newName;
    targetAccount->address = newAddress;
    targetAccount->contactNumber = newContactNumber;

    if (!saveAccount(*targetAccount)) {
        return {false, "Storage error: account details were not updated."};
    }

    return {true, "Account details updated successfully."};
}

bool accountUpdate() {
    // SEC-1: Only authenticated employees can access account update.
    if (!isEmployeeAuthenticated()) {
        std::cout << "Access denied: employee login required.\n";
        return false;
    }

    std::string accountText;
    int accountNo = 0;

    std::cout << "\n--- Update Account ---\n";
    std::cout << "Account number: ";

    // Read the whole line so input such as "100001abc" is rejected.
    if (!std::getline(std::cin, accountText) ||
        !parseAccountNumber(accountText, accountNo)) {
        std::cout << "Error: Account number must be a positive whole number.\n";
        return false;
    }

    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        std::cout << "Storage error: could not read account data.\n";
        return false;
    }

    bool found = false;

    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            if (account.status != "ACTIVE") {
                std::cout << "Error: Cannot update a closed account.\n";
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
        std::cout << "Error: Account not found.\n";
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