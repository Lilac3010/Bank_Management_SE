
#include <iostream>
#include <string>
#include <vector>

#include "transactions.h"
#include "storage.h"
#include "auth.h"

namespace {
    int passed = 0;
    int failed = 0;

    std::vector<Account> testAccounts;
    std::vector<Transaction> testTransactions;

    bool accountsLoadWorks = true;
    bool transactionsLoadWorks = true;
    bool accountSaveWorks = true;
    bool transactionSaveWorks = true;
    bool rollbackSaveWorks = true;
    int accountSaveCalls = 0;

    void check(bool condition, const std::string& testName) {
        if (condition) {
            ++passed;
            std::cout << "[PASS] " << testName << '\n';
        } else {
            ++failed;
            std::cout << "[FAIL] " << testName << '\n';
        }
    }

    void reset() {
        testAccounts.clear();
        testTransactions.clear();

        accountsLoadWorks = true;
        transactionsLoadWorks = true;
        accountSaveWorks = true;
        transactionSaveWorks = true;
        rollbackSaveWorks = true;
        accountSaveCalls = 0;

        Account account;
        account.accountNumber = 100001;
        account.name = "Test Customer";
        account.pin = "1234";
        account.balance = 50000; // Rs. 500.00
        account.status = STATUS_ACTIVE;

        testAccounts.push_back(account);
    }

    const Account* findTestAccount(int accountNo) {
        for (const Account& account : testAccounts) {
            if (account.accountNumber == accountNo) {
                return &account;
            }
        }
        return nullptr;
    }
}

// Fake storage: tests do not access actual data files.
bool loadAccounts(std::vector<Account>& accounts) {
    if (!accountsLoadWorks) return false;
    accounts = testAccounts;
    return true;
}

bool saveAccount(const Account& account) {
    ++accountSaveCalls;

    if (!accountSaveWorks) return false;

    // The second account save is the rollback attempt.
    if (accountSaveCalls == 2 && !rollbackSaveWorks) {
        return false;
    }

    for (Account& existing : testAccounts) {
        if (existing.accountNumber == account.accountNumber) {
            existing = account;
            return true;
        }
    }

    testAccounts.push_back(account);
    return true;
}

bool loadTransactions(std::vector<Transaction>& transactions) {
    if (!transactionsLoadWorks) return false;
    transactions = testTransactions;
    return true;
}

bool saveTransaction(const Transaction& txn) {
    if (!transactionSaveWorks) return false;
    testTransactions.push_back(txn);
    return true;
}

// Fake authentication.
bool loginEmployee() {
    return false;
}

bool authenticateCustomer(int accountNo, const std::string& pin) {
    const Account* account = findTestAccount(accountNo);
    return account != nullptr
        && account->pin == pin
        && account->status == STATUS_ACTIVE;
}

bool isEmployeeAuthenticated() {
    return false;
}

int main() {
    std::cout << "\n=== FR-4 Withdrawal Tests ===\n";

    // Test 1: successful withdrawal
    reset();
    WithdrawalResult result = withdraw(100001, 15000);
    check(result.success, "Successful withdrawal");
    check(result.newBalance == 35000, "Balance reduced correctly");
    check(findTestAccount(100001)->balance == 35000,
          "Saved account balance is correct");
    check(testTransactions.size() == 1,
          "Withdrawal transaction recorded");
    check(testTransactions[0].type == TXN_WITHDRAWAL,
          "Transaction type is WITHDRAW");
    check(testTransactions[0].amount == 15000,
          "Transaction amount is correct");
    check(result.transactionId == 1,
          "First transaction ID is 1");

    // Test 2: withdraw the entire balance
    reset();
    result = withdraw(100001, 50000);
    check(result.success && result.newBalance == 0,
          "Withdrawal of entire balance");

    // Test 3: insufficient balance
    reset();
    result = withdraw(100001, 50001);
    check(!result.success, "Insufficient balance rejected");
    check(findTestAccount(100001)->balance == 50000,
          "Insufficient balance leaves balance unchanged");
    check(testTransactions.empty(),
          "No transaction recorded for rejected withdrawal");

    // Test 4: zero and negative amounts
    reset();
    result = withdraw(100001, 0);
    check(!result.success, "Zero amount rejected");
    result = withdraw(100001, -100);
    check(!result.success, "Negative amount rejected");

    // Test 5: unknown account
    reset();
    result = withdraw(999999, 1000);
    check(!result.success, "Unknown account rejected");

    // Test 6: closed account
    reset();
    testAccounts[0].status = STATUS_CLOSED;
    result = withdraw(100001, 1000);
    check(!result.success, "Closed account rejected");

    // Test 7: next transaction ID
    reset();
    Transaction previous;
    previous.transactionId = 7;
    previous.accountNumber = 100001;
    previous.type = TXN_DEPOSIT;
    previous.amount = 10000;
    previous.dateTime = "2026-10-24 10:00:00";
    testTransactions.push_back(previous);

    result = withdraw(100001, 1000);
    check(result.success && result.transactionId == 8,
          "Next transaction ID follows highest existing ID");

    // Test 8: account loading failure
    reset();
    accountsLoadWorks = false;
    result = withdraw(100001, 1000);
    check(!result.success, "Account loading failure handled");

    // Test 9: transaction loading failure
    reset();
    transactionsLoadWorks = false;
    result = withdraw(100001, 1000);
    check(!result.success, "Transaction loading failure handled");
    check(findTestAccount(100001)->balance == 50000,
          "No balance change when transaction loading fails");

    // Test 10: account saving failure
    reset();
    accountSaveWorks = false;
    result = withdraw(100001, 1000);
    check(!result.success, "Account saving failure handled");
    check(findTestAccount(100001)->balance == 50000,
          "Balance unchanged when account save fails");

    // Test 11: transaction save fails; rollback succeeds
    reset();
    transactionSaveWorks = false;
    result = withdraw(100001, 1000);
    check(!result.success, "Transaction saving failure handled");
    check(findTestAccount(100001)->balance == 50000,
          "Balance restored after transaction save failure");

    // Test 12: transaction save and rollback both fail
    reset();
    transactionSaveWorks = false;
    rollbackSaveWorks = false;
    result = withdraw(100001, 1000);
    check(!result.success,
          "Withdrawal reports failure when transaction save fails");
    check(result.message.find("may be inconsistent") != std::string::npos,
          "Rollback failure reports possible inconsistency");
    check(findTestAccount(100001)->balance == 49000,
          "Failed rollback leaves the deducted balance in the fake store");

    std::cout << "\nPassed: " << passed
              << "\nFailed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}
