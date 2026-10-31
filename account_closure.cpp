#include "account_closure.h"

#include "auth.h"
#include "models.h"
#include "storage.h"

#include <iostream>
#include <limits>
#include <vector>

AccountClosureResult closeAccount(int accountNo) {
    if (accountNo <= 0) {
        return {false, "Invalid account number."};
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

    if (targetAccount->status == STATUS_CLOSED) {
        return {false, "Account is already closed."};
    }

    if (targetAccount->status != STATUS_ACTIVE) {
        return {false, "Account has an invalid status."};
    }

    // Preserve the account record and history; mark it as closed.
    targetAccount->status = STATUS_CLOSED;

    if (!saveAccount(*targetAccount)) {
        return {false, "Failed to save account closure."};
    }

    return {true, "Account closed successfully."};
}

bool accountClosure() {
    if (!isEmployeeAuthenticated()) {
        std::cout << "Access denied. Employee login required.\n";
        return false;
    }

    int accountNo;

    std::cout << "Enter account number to close: ";

    if (!(std::cin >> accountNo)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid account number.\n";
        return false;
    }

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    AccountClosureResult result = closeAccount(accountNo);

    std::cout << result.message << '\n';
    return result.success;
}