#include "account_closure.h"
#include "auth.h"
#include "models.h"
#include "storage.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static std::vector<Account> testAccounts;
static bool saveShouldSucceed = true;

bool loadAccounts(std::vector<Account>& accounts) {
    accounts = testAccounts;
    return true;
}

bool saveAccount(const Account& updatedAccount) {
    if (!saveShouldSucceed) {
        return false;
    }

    for (Account& account : testAccounts) {
        if (account.accountNumber == updatedAccount.accountNumber) {
            account = updatedAccount;
            return true;
        }
    }

    return false;
}

bool isEmployeeAuthenticated() {
    return true;
}

bool loginEmployee() {
    return true;
}

bool authenticateCustomer(int, const std::string&) {
    return false;
}

void resetTestData() {
    testAccounts.clear();
    saveShouldSucceed = true;

    Account account;
    account.accountNumber = 1001;
    account.name = "Ananya Rao";
    account.address = "Bengaluru";
    account.contactNumber = "9876543210";
    account.pin = "1234";
    account.balance = 500000;
    account.status = STATUS_ACTIVE;

    testAccounts.push_back(account);
}

void testSuccessfulClosure() {
    resetTestData();

    AccountClosureResult result = closeAccount(1001);

    assert(result.success);
    assert(testAccounts[0].status == STATUS_CLOSED);

    // Other account details must remain unchanged.
    assert(testAccounts[0].accountNumber == 1001);
    assert(testAccounts[0].name == "Ananya Rao");
    assert(testAccounts[0].pin == "1234");
    assert(testAccounts[0].balance == 500000);

    std::cout << "PASS: Account closed successfully\n";
}

void testAccountNotFound() {
    resetTestData();

    AccountClosureResult result = closeAccount(9999);

    assert(!result.success);
    std::cout << "PASS: Account not found\n";
}

void testInvalidAccountNumber() {
    resetTestData();

    AccountClosureResult result = closeAccount(-1);

    assert(!result.success);
    std::cout << "PASS: Invalid account number rejected\n";
}

void testAlreadyClosedAccount() {
    resetTestData();
    testAccounts[0].status = STATUS_CLOSED;

    AccountClosureResult result = closeAccount(1001);

    assert(!result.success);
    assert(testAccounts[0].status == STATUS_CLOSED);
    std::cout << "PASS: Already-closed account rejected\n";
}

void testSaveFailure() {
    resetTestData();
    saveShouldSucceed = false;

    AccountClosureResult result = closeAccount(1001);

    assert(!result.success);
    std::cout << "PASS: Save failure handled\n";
}

int main() {
    testSuccessfulClosure();
    testAccountNotFound();
    testInvalidAccountNumber();
    testAlreadyClosedAccount();
    testSaveFailure();

    std::cout << "\nAll FR-9 tests passed!\n";
    return 0;
}