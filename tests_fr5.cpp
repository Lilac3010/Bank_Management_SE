// FR-5 balance enquiry tests.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr5.exe tests_fr5.cpp balance_enquiry.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_fr5.cpp balance_enquiry.cpp /Fe:tests_fr5.exe
//
// Build ONLY these two files. Never link other test files (they define main() and the
// same fakes) or main.cpp / auth.cpp / storage.cpp into this program.
//
// Money is in paise: Rs. 1250.50 = 125050.
#include <climits>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "auth.h"
#include "balance_enquiry.h"
#include "storage.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces whose implementations do not exist yet:
//   authenticateCustomer -> FR-11 (auth.cpp)
//   loadAccounts         -> FR-13 (storage.cpp)
// Defined only in this file; never linked into main.
namespace fake {
std::vector<Account> accounts;
bool forceAuthFailure = false;
bool acceptAnyPin = false;     // simulates an auth module that knows an account storage lacks
bool failLoadAccounts = false;
int authCalls = 0;
int loadCalls = 0;
int lastAuthAccount = 0;
std::string lastAuthPin;
}

// Accepts the stored PIN of a stored account, whatever its status.
bool authenticateCustomer(int accountNo, const std::string& pin) {
    ++fake::authCalls;
    fake::lastAuthAccount = accountNo;
    fake::lastAuthPin = pin;
    if (fake::forceAuthFailure) return false;
    if (fake::acceptAnyPin) return true;
    for (const Account& a : fake::accounts)
        if (a.accountNumber == accountNo && a.pin == pin) return true;
    return false;
}

bool loadAccounts(std::vector<Account>& accounts) {
    ++fake::loadCalls;
    if (fake::failLoadAccounts) return false;
    accounts = fake::accounts;
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

Account makeAccount(int number, long long balance, const std::string& status = STATUS_ACTIVE) {
    Account a;
    a.accountNumber = number;
    a.name = "Customer " + std::to_string(number);
    a.address = "12 MG Road";
    a.contactNumber = "9000000000";
    a.pin = "1234";
    a.balance = balance;
    a.status = status;
    return a;
}

// One ACTIVE account 100001 with Rs. 1250.50.
void reset() {
    fake::accounts = {makeAccount(100001, 125050)};
    fake::forceAuthFailure = false;
    fake::acceptAnyPin = false;
    fake::failLoadAccounts = false;
    fake::authCalls = 0;
    fake::loadCalls = 0;
    fake::lastAuthAccount = 0;
    fake::lastAuthPin.clear();
}

bool runEnquiry(const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = balanceEnquiry();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    output = out.str();
    return ok;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool noBalanceShown(const std::string& output) {
    return !contains(output, "Rs.") && !contains(output, "Current balance");
}

void testSuccessfulEnquiry() {
    reset();
    std::string output;
    bool ok = runEnquiry("100001\n1234\n", output);
    check(ok && contains(output, "Current balance for account 100001: Rs. 1250.50"),
          "Correct PIN shows balance Rs. 1250.50 for account 100001");
    check(fake::authCalls == 1 && fake::lastAuthAccount == 100001 && fake::lastAuthPin == "1234",
          "authenticateCustomer called once with account 100001 and PIN 1234");

    reset();
    ok = runEnquiry(" 100001 \n 1234 \n", output);
    check(ok && fake::lastAuthPin == "1234" && contains(output, "Rs. 1250.50"),
          "Spaces around account number and PIN are trimmed");
}

void testFormatting() {
    struct { long long paise; const char* shown; } cases[] = {
        {125050, "Rs. 1250.50"}, {1, "Rs. 0.01"}, {0, "Rs. 0.00"}, {100000, "Rs. 1000.00"},
        {10, "Rs. 0.10"}, {LLONG_MAX, "Rs. 92233720368547758.07"}, {-500, "Rs. -5.00"}};
    for (const auto& c : cases) {
        reset();
        fake::accounts[0].balance = c.paise;
        std::string output;
        bool ok = runEnquiry("100001\n1234\n", output);
        check(ok && contains(output, std::string(": ") + c.shown + "\n"),
              std::to_string(c.paise) + " paise shown as " + c.shown);
    }
}

void testOnlyRequestedAccount() {
    reset();
    fake::accounts = {makeAccount(100001, 111111), makeAccount(100002, 222222),
                      makeAccount(100003, 333333)};
    std::string output;
    bool ok = runEnquiry("100002\n1234\n", output);
    check(ok && contains(output, "account 100002: Rs. 2222.22")
          && !contains(output, "1111.11") && !contains(output, "3333.33"),
          "Only the requested account's balance is shown when several exist");
}

void testAuthenticationFailure() {
    reset();
    std::string output;
    bool ok = runEnquiry("100001\n9999\n", output);
    check(!ok && contains(output, "Access denied") && noBalanceShown(output)
          && fake::loadCalls == 0,
          "Wrong PIN: access denied, no balance shown, storage not read");

    reset();
    fake::forceAuthFailure = true;
    ok = runEnquiry("100001\n1234\n", output);
    check(!ok && contains(output, "Access denied") && noBalanceShown(output)
          && fake::loadCalls == 0,
          "Authentication failure for an existing account: no balance shown");

    reset();
    ok = runEnquiry("999999\n1234\n", output);
    check(!ok && contains(output, "Access denied") && noBalanceShown(output),
          "Unknown account at login: access denied, no balance shown");
}

void testInvalidAccountNumberInput() {
    const char* inputs[] = {"", "   ", "abc", "-5", "0", "+5", "12a", "1.5",
                            "2147483648", "99999999999"};
    for (const char* text : inputs) {
        reset();
        std::string output;
        bool ok = runEnquiry(std::string(text) + "\n1234\n", output);
        check(!ok && contains(output, "positive whole number") && !contains(output, "PIN:")
              && fake::authCalls == 0 && fake::loadCalls == 0 && noBalanceShown(output),
              std::string("Account number \"") + text
                  + "\" rejected before PIN; no auth, no balance");
    }

    reset();
    fake::acceptAnyPin = true;
    fake::accounts.push_back(makeAccount(INT_MAX, 500));
    std::string output;
    bool ok = runEnquiry("2147483647\n1234\n", output);
    check(ok && contains(output, "account 2147483647: Rs. 5.00"),
          "Largest int account number 2147483647 accepted");
}

void testAccountValidation() {
    reset();
    fake::acceptAnyPin = true;
    std::string output;
    bool ok = runEnquiry("100002\n1234\n", output);
    check(!ok && contains(output, "Account not found") && noBalanceShown(output),
          "Authenticated but no stored record: account not found, no balance");

    reset();
    fake::accounts[0].status = STATUS_CLOSED;
    ok = runEnquiry("100001\n1234\n", output);
    check(!ok && contains(output, "Account is closed") && noBalanceShown(output),
          "CLOSED account rejected even with correct PIN; no balance");

    reset();
    fake::accounts[0].status = "FROZEN";
    ok = runEnquiry("100001\n1234\n", output);
    check(!ok && contains(output, "Account is not active") && noBalanceShown(output),
          "Other inactive status (FROZEN) rejected; no balance");
}

void testStorageFailure() {
    reset();
    fake::failLoadAccounts = true;
    std::string output;
    bool ok = runEnquiry("100001\n1234\n", output);
    check(!ok && contains(output, "Storage error") && noBalanceShown(output)
          && fake::authCalls == 1 && fake::loadCalls == 1,
          "loadAccounts failure after login: storage error, no balance");
}

} // namespace

int main() {
    std::cout << "FR-5 balance enquiry tests. Customer login and storage are test doubles:\n"
                 "these results do NOT verify FR-11, FR-13 or persistence.\n\n";

    testSuccessfulEnquiry();
    testFormatting();
    testOnlyRequestedAccount();
    testAuthenticationFailure();
    testInvalidAccountNumberInput();
    testAccountValidation();
    testStorageFailure();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
