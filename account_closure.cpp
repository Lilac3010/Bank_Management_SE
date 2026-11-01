
#include "account_closure.h"

#include "auth.h"
#include "models.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Accept a whole line of digits only (surrounding spaces are ignored) for a positive
// number that fits in an int. Same rules as parseAccountNumber in account_search.cpp (FR-7).
bool parseAccountNumber(const std::string& text, int& accountNo) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return false;
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    const std::string digits = text.substr(start, end - start + 1);
    if (digits.size() > 10) {
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

} // namespace

AccountClosureResult closeAccount(int accountNo) {
    // SEC-1: Only authenticated employees can close accounts.
    if (!isEmployeeAuthenticated()) {
        return {false, "Access denied: employee login required."};
    }

    if (accountNo <= 0) {
        return {false, "Error: Account number must be a positive whole number."};
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

    if (targetAccount->status == STATUS_CLOSED) {
        return {false, "Error: Account is already closed."};
    }

    if (targetAccount->status != STATUS_ACTIVE) {
        return {false, "Error: Account has an invalid status."};
    }

    // Preserve the account record and history; mark it as closed.
    targetAccount->status = STATUS_CLOSED;

    if (!saveAccount(*targetAccount)) {
        return {false, "Storage error: account was not closed."};
    }

    return {true, "Account closed successfully."};
}

bool accountClosure() {
    // SEC-1: Only authenticated employees can access account closure.
    if (!isEmployeeAuthenticated()) {
        std::cout << "Access denied: employee login required.\n";
        return false;
    }

    std::string accountText;
    int accountNo = 0;

    std::cout << "\n--- Close Account ---\n";
    std::cout << "Account number: ";

    // Read the whole line so input such as "100001abc" is rejected, not read as 100001.
    if (!std::getline(std::cin, accountText) ||
        !parseAccountNumber(accountText, accountNo)) {
        std::cout << "Error: Account number must be a positive whole number.\n";
        return false;
    }

    AccountClosureResult result = closeAccount(accountNo);

    std::cout << result.message << '\n';
    return result.success;
}
