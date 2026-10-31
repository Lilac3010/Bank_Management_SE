#include "account_update.h"
#include "models.h"
#include "storage.h"
#include "auth.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Fake account storage for unit testing.
static std::vector<Account> testAccounts;
static bool employeeAuthenticated = true;
static bool saveShouldSucceed = true;

// Fake storage functions.
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

    testAccounts.push_back(updatedAccount);
    return true;
}

// Fake authentication function.
bool isEmployeeAuthenticated() {
    return employeeAuthenticated;
}

// Other auth functions may be referenced by the project headers,
// but are not needed for these tests.
bool loginEmployee() {
    return employeeAuthenticated;
}

bool authenticateCustomer(int, const std::string&) {
    return false;
}

void resetTestData() {
    testAccounts.clear();

    Account account1;
    account1.accountNumber = 1001;
    account1.name = "Ananya Rao";
    account1.address = "Bengaluru";
    account1.contactNumber = "9876543210";
    account1.pin = "1234";
    account1.balance = 500000;
    account1.status = "ACTIVE";

    Account account2;
    account2.accountNumber = 1002;
    account2.name = "Rahul Kumar";
    account2.address = "Mysuru";
    account2.contactNumber = "9123456780";
    account2.pin = "5678";
    account2.balance = 250000;
    account2.status = "ACTIVE";

    testAccounts.push_back(account1);
    testAccounts.push_back(account2);

    employeeAuthenticated = true;
    saveShouldSucceed = true;
}

void testSuccessfulUpdate() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai", "9988776655");

    assert(result.success);
    assert(testAccounts[0].name == "Ananya Sharma");
    assert(testAccounts[0].address == "Chennai");
    assert(testAccounts[0].contactNumber == "9988776655");

    // Uneditable details must remain unchanged.
    assert(testAccounts[0].accountNumber == 1001);
    assert(testAccounts[0].pin == "1234");
    assert(testAccounts[0].balance == 500000);
    assert(testAccounts[0].status == "ACTIVE");

    std::cout << "PASS: Successful account update\n";
}

void testAccountNotFound() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(9999, "Ananya Sharma", "Chennai", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Account not found\n";
}

void testInvalidName() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya123", "Chennai", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Invalid name rejected\n";
}

void testEmptyAddress() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Empty address rejected\n";
}

void testAddressWithDelimiter() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai|India", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Address containing storage delimiter rejected\n";
}

void testInvalidContactNumber() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai", "12345");

    assert(!result.success);
    std::cout << "PASS: Invalid contact number rejected\n";
}

void testDuplicateContactNumber() {
    resetTestData();

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai", "9123456780");

    assert(!result.success);
    std::cout << "PASS: Duplicate contact number rejected\n";
}

void testClosedAccountCannotBeUpdated() {
    resetTestData();
    testAccounts[0].status = "CLOSED";

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Closed account update rejected\n";
}

void testSaveFailure() {
    resetTestData();
    saveShouldSucceed = false;

    UpdateAccountResult result =
        updateAccountDetails(1001, "Ananya Sharma", "Chennai", "9988776655");

    assert(!result.success);
    std::cout << "PASS: Save failure handled\n";
}

int main() {
    testSuccessfulUpdate();
    testAccountNotFound();
    testInvalidName();
    testEmptyAddress();
    testAddressWithDelimiter();
    testInvalidContactNumber();
    testDuplicateContactNumber();
    testClosedAccountCannotBeUpdated();
    testSaveFailure();

    std::cout << "\nAll FR-8 tests passed!\n";
    return 0;
}