#include "transactions.h"
#include "storage.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

double getBalance(
    const std::string& accountNo,
    const std::vector<Account>& accounts
) {
    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            return account.balance;
        }
    }

    // -1 indicates that the account was not found.
    return -1.0;
}

bool withdraw(
    const std::string& accountNo,
    double amount,
    std::vector<Account>& accounts,
    std::vector<Transaction>& transactions
) {
    // 1. Validate the withdrawal amount.
    if (!std::isfinite(amount) || amount <= 0.0) {
        return false;
    }

    // 2. Find the account.
    Account* account = nullptr;

    for (Account& item : accounts) {
        if (item.accountNumber == accountNo) {
            account = &item;
            break;
        }
    }

    if (account == nullptr) {
        return false;
    }

    // 3. Check account status.
    if (account->status != "ACTIVE") {
        return false;
    }

    // 4. Check available balance.
    if (amount > account->balance) {
        return false;
    }

    // 5. Prepare the updated account and transaction.
    Account previousAccount = *account;

    Account updatedAccount = previousAccount;
    updatedAccount.balance =
        std::round((updatedAccount.balance - amount) * 100.0)
        / 100.0;

    if (updatedAccount.balance < 0.0) {
        return false;
    }

    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    if (localTime == nullptr) {
        return false;
    }

    std::ostringstream timestamp;
    timestamp << std::put_time(
        localTime, "%Y-%m-%d %H:%M:%S"
    );

    Transaction transaction;
    transaction.transactionId = timestamp.str();
    transaction.accountNumber = accountNo;
    transaction.type = "WITHDRAWAL";
    transaction.amount = amount;
    transaction.dateTime = timestamp.str();
    transaction.resultingBalance = updatedAccount.balance;

    // 6. Save the account's new balance.
    if (!saveAccount(updatedAccount)) {
        return false;
    }

    // 7. Save the transaction record.
    if (!saveTransaction(transaction)) {
        // Attempt to restore the original balance.
        if (!saveAccount(previousAccount)) {
            // Persistence needs recovery; do not claim success.
            return false;
        }

        return false;
    }

    // 8. Update in-memory data only after both saves succeed.
    *account = updatedAccount;
    transactions.push_back(transaction);

    return true;
}