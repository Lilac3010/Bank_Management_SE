
#include "transactions.h"
#include "storage.h"

#include <iostream>
#include <cmath>

void runWithdrawalTests() {
    int passed = 0;
    int failed = 0;

    auto check = [&](const std::string& name, bool result) {
        if (result) {
            std::cout << "[PASS] " << name << '\n';
            ++passed;
        } else {
            std::cout << "[FAIL] " << name << '\n';
            ++failed;
        }
    };

    // Create a test account with a known balance.
    Account testAccount;
    testAccount.accountNumber = "TEST1001";
    testAccount.name = "Test Customer";
    testAccount.address = "Test Address";
    testAccount.contactNumber = "9999999999";
    testAccount.pin = "1234";
    testAccount.balance = 1000.00;
    testAccount.status = "ACTIVE";

    check("Save initial test account", saveAccount(testAccount));

    std::vector<Account> accounts;
    std::vector<Transaction> transactions;

    check("Load accounts", loadAccounts(accounts));
    check("Load transactions", loadTransactions(transactions));

    // Test 1: Valid withdrawal.
    bool success = withdraw(
        "TEST1001", 200.00, accounts, transactions
    );

    check("Valid withdrawal succeeds", success);
    check(
        "Balance becomes 800",
        std::abs(getBalance("TEST1001", accounts) - 800.00) < 0.001
    );

    // Test 2: Balance persists after reloading from file.
    std::vector<Account> reloadedAccounts;

    check("Reload accounts from file", loadAccounts(reloadedAccounts));
    check(
        "Balance persists as 800",
        std::abs(
            getBalance("TEST1001", reloadedAccounts) - 800.00
        ) < 0.001
    );

    // Test 3: Withdrawal transaction persists.
    std::vector<Transaction> reloadedTransactions;

    check(
        "Reload transaction history",
        loadTransactions(reloadedTransactions)
    );

    bool withdrawalFound = false;

    for (const Transaction& tx : reloadedTransactions) {
        if (tx.accountNumber == "TEST1001" &&
            tx.type == "WITHDRAWAL" &&
            std::abs(tx.amount - 200.00) < 0.001 &&
            std::abs(tx.resultingBalance - 800.00) < 0.001) {
            withdrawalFound = true;
            break;
        }
    }

    check("Withdrawal record persists", withdrawalFound);

    // Test 4: Insufficient balance.
    double balanceBefore = getBalance("TEST1001", accounts);

    success = withdraw(
        "TEST1001", 1200.00, accounts, transactions
    );

    check("Insufficient balance rejected", !success);
    check(
        "Balance unchanged after rejection",
        std::abs(
            getBalance("TEST1001", accounts) - balanceBefore
        ) < 0.001
    );

    // Test 5: Invalid withdrawal amounts.
    check(
        "Zero withdrawal rejected",
        !withdraw("TEST1001", 0.00, accounts, transactions)
    );

    check(
        "Negative withdrawal rejected",
        !withdraw("TEST1001", -50.00, accounts, transactions)
    );

    // Test 6: Nonexistent account.
    check(
        "Unknown account rejected",
        !withdraw(
            "DOES_NOT_EXIST", 50.00, accounts, transactions
        )
    );

    // Test 7: Closed account.
    Account closedAccount = testAccount;
    closedAccount.accountNumber = "TESTCLOSED";
    closedAccount.status = "CLOSED";
    closedAccount.balance = 500.00;

    bool savedClosed = saveAccount(closedAccount);

    check("Save closed test account", savedClosed);

    bool accountsLoaded = loadAccounts(accounts);

    check("Reload accounts for closed-account test", accountsLoaded);

    if (accountsLoaded) {
        check(
            "Closed account withdrawal rejected",
            !withdraw(
                "TESTCLOSED", 50.00, accounts, transactions
            )
        );
    }

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed.\n";
}

int main() {
    std::cout << "FR-4 Withdrawal Tests\n\n";

    runWithdrawalTests();

    return 0;
}
