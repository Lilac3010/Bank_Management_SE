// FR-2 tests, using the REAL generateAccountNumber from account_number.cpp.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr2.exe tests_fr2.cpp account.cpp account_number.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_fr2.cpp account.cpp account_number.cpp /Fe:tests_fr2.exe
//
// Build ONLY these three files. Never link tests.cpp (it has a fake generator) or
// main.cpp / auth.cpp / storage.cpp into this program.
#include <climits>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "account.h"
#include "auth.h"
#include "storage.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces whose implementations do not exist yet:
//   isEmployeeAuthenticated  -> FR-10 (auth.cpp)
//   loadAccounts/saveAccount -> FR-13 (storage.cpp)
// There is deliberately NO fake generateAccountNumber here.
// Defined only in this file; never linked into main.
namespace fake {
bool employeeLoggedIn = true;
std::vector<Account> store;
int saveCalls = 0;
}

bool isEmployeeAuthenticated() { return fake::employeeLoggedIn; }

bool loadAccounts(std::vector<Account>& accounts) {
    accounts = fake::store;
    return true;
}

bool saveAccount(const Account& account) {
    ++fake::saveCalls;
    for (Account& a : fake::store) {
        if (a.accountNumber == account.accountNumber) {
            a = account;
            return true;
        }
    }
    fake::store.push_back(account);
    return true;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

void reset() {
    fake::employeeLoggedIn = true;
    fake::store.clear();
    fake::saveCalls = 0;
}

Account makeAccount(int number, const std::string& status = STATUS_ACTIVE) {
    Account a;
    a.accountNumber = number;
    a.name = "Existing Customer";
    a.address = "12 MG Road";
    a.contactNumber = "9000000000";
    a.pin = "1234";
    a.balance = 100000;
    a.status = status;
    return a;
}

std::vector<Account> accountsWithNumbers(const std::vector<int>& numbers) {
    std::vector<Account> accounts;
    for (int n : numbers) accounts.push_back(makeAccount(n));
    return accounts;
}

bool isUnused(int number, const std::vector<Account>& accounts) {
    for (const Account& a : accounts)
        if (a.accountNumber == number) return false;
    return true;
}

// Distinct 10-digit contact numbers: 9100000000, 9100000001, ...
std::string contactFor(int i) {
    std::string digits = std::to_string(i);
    return "91" + std::string(8 - digits.size(), '0') + digits;
}

// Generator on its own
void testGeneratorRules() {
    std::vector<Account> empty;
    check(generateAccountNumber(empty) == 100001, "FR-2 empty list gives 100001");

    std::vector<Account> one = accountsWithNumbers({100001});
    int second = generateAccountNumber(one);
    check(second == 100002 && isUnused(second, one), "FR-2 second account gets 100002");

    std::vector<Account> unsorted = accountsWithNumbers({100005, 100002, 100009});
    int next = generateAccountNumber(unsorted);
    check(next == 100010 && isUnused(next, unsorted),
          "FR-2 unsorted list gives highest + 1 (100010)");

    std::vector<Account> closedHighest = {makeAccount(100001), makeAccount(100007, STATUS_CLOSED)};
    next = generateAccountNumber(closedHighest);
    check(next == 100008 && isUnused(next, closedHighest),
          "FR-2 CLOSED account's number counts: gives 100008, not 100002");

    std::vector<Account> allClosed = {makeAccount(100001, STATUS_CLOSED),
                                      makeAccount(100002, STATUS_CLOSED)};
    next = generateAccountNumber(allClosed);
    check(next == 100003 && isUnused(next, allClosed),
          "FR-2 all accounts CLOSED: closed numbers not reused (100003)");

    std::vector<Account> belowStart = accountsWithNumbers({5, 42});
    next = generateAccountNumber(belowStart);
    check(next == 100001 && isUnused(next, belowStart),
          "FR-2 numbers below the start give 100001");

    std::vector<Account> invalid = accountsWithNumbers({0, -3});
    check(generateAccountNumber(invalid) == 100001, "FR-2 numbers <= 0 are ignored (100001)");

    std::vector<Account> gaps = accountsWithNumbers({100001, 100010});
    next = generateAccountNumber(gaps);
    check(next == 100011 && isUnused(next, gaps), "FR-2 gaps are not filled (100011)");

    check(generateAccountNumber(unsorted) == generateAccountNumber(unsorted),
          "FR-2 deterministic: same list gives same number");
}

// Boundary of the int account-number type
void testBoundary() {
    std::vector<Account> nearMax = accountsWithNumbers({INT_MAX - 1});
    check(generateAccountNumber(nearMax) == INT_MAX, "FR-2 INT_MAX - 1 gives INT_MAX");

    std::vector<Account> atMax = accountsWithNumbers({100001, INT_MAX});
    check(generateAccountNumber(atMax) == 0, "FR-2 returns 0 when INT_MAX is already used");
}

// Repeated generation
void testRepeatedGeneration() {
    std::vector<Account> accounts;
    std::set<int> seen;
    bool unique = true;
    bool increasing = true;
    int previous = 0;
    for (int i = 0; i < 1000; ++i) {
        int number = generateAccountNumber(accounts);
        if (!isUnused(number, accounts) || !seen.insert(number).second) unique = false;
        if (number <= previous) increasing = false;
        previous = number;
        accounts.push_back(makeAccount(number));
    }
    check(unique, "FR-2 1000 generated numbers are all unique");
    check(increasing && accounts.front().accountNumber == 100001
          && accounts.back().accountNumber == 101000,
          "FR-2 1000 numbers increase from 100001 to 101000");
}

// FR-1 account creation using the real generator (fake auth and storage)
void testCreationUsesRealGenerator() {
    reset();
    CreateAccountResult first = createAccountWithDetails("Asha Rao", "12 MG Road",
                                                         "9876543210", "1234", 100000);
    CreateAccountResult second = createAccountWithDetails("Ravi Kumar", "5 Park Street",
                                                          "9123456780", "5678", 250000);
    check(first.success && first.accountNumber == 100001
          && first.message.find("Account number: 100001") != std::string::npos,
          "FR-1+2 first account created as 100001");
    check(second.success && second.accountNumber == 100002
          && second.message.find("Account number: 100002") != std::string::npos,
          "FR-1+2 second account created as 100002");
    check(fake::store.size() == 2 && fake::store[0].accountNumber == 100001
          && fake::store[1].accountNumber == 100002,
          "FR-1+2 both accounts saved with their own numbers");

    reset();
    fake::store.push_back(makeAccount(100001));
    fake::store.push_back(makeAccount(100003, STATUS_CLOSED));
    CreateAccountResult r = createAccountWithDetails("Asha Rao", "12 MG Road",
                                                     "9876543210", "1234", 100000);
    check(r.success && r.accountNumber == 100004 && fake::store.size() == 3,
          "FR-1+2 closed account 100003 not reused; new account is 100004");
}

void testManyCreations() {
    reset();
    std::set<int> numbers;
    bool allCreated = true;
    for (int i = 0; i < 50; ++i) {
        CreateAccountResult r = createAccountWithDetails("Customer", "12 MG Road",
                                                         contactFor(i), "1234", 100000);
        if (!r.success) allCreated = false;
        numbers.insert(r.accountNumber);
    }
    check(allCreated && numbers.size() == 50 && fake::store.size() == 50,
          "FR-1+2 50 accounts created with 50 unique numbers");
}

// Failed generation must not save or report success
void testGenerationFailure() {
    reset();
    fake::store.push_back(makeAccount(INT_MAX));
    CreateAccountResult r = createAccountWithDetails("Asha Rao", "12 MG Road",
                                                     "9876543210", "1234", 100000);
    check(!r.success && r.accountNumber == 0 && fake::saveCalls == 0
          && fake::store.size() == 1 && fake::store[0].accountNumber == INT_MAX
          && r.message.find("Could not generate a unique account number") != std::string::npos,
          "FR-1+2 no number available: creation fails, nothing saved, no success");
}

} // namespace

int main() {
    std::cout << "FR-2 tests with the real generator. Employee login and storage are test\n"
                 "doubles: these results do NOT verify FR-10, FR-13 or persistence.\n\n";

    testGeneratorRules();
    testBoundary();
    testRepeatedGeneration();
    testCreationUsesRealGenerator();
    testManyCreations();
    testGenerationFailure();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
