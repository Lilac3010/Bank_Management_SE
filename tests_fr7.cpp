// FR-7 employee account search tests.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr7.exe tests_fr7.cpp account_search.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_fr7.cpp account_search.cpp /Fe:tests_fr7.exe
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

#include "account_search.h"
#include "auth.h"
#include "storage.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces this suite does not link:
//   isEmployeeAuthenticated -> FR-10 (auth.cpp, not implemented yet)
//   loadAccounts            -> FR-13 (storage.cpp; real storage is tested in tests_fr13.cpp)
// Defined only in this file; never linked into main.
namespace fake {
bool employeeLoggedIn = true;
std::vector<Account> accounts;
bool failLoadAccounts = false;
int authCalls = 0;
int loadCalls = 0;
}

bool isEmployeeAuthenticated() {
    ++fake::authCalls;
    return fake::employeeLoggedIn;
}

bool loadAccounts(std::vector<Account>& accounts) {
    ++fake::loadCalls;
    if (fake::failLoadAccounts) return false;
    accounts = fake::accounts;
    return true;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

// Distinctive PIN so any leak into the output can be detected.
const std::string SECRET_PIN = "4729";

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
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = "98765" + std::to_string(number).substr(1);
    a.pin = SECRET_PIN;
    a.balance = balance;
    a.status = status;
    return a;
}

// Employee logged in; one ACTIVE account 100001 with Rs. 1250.50.
void reset() {
    fake::employeeLoggedIn = true;
    fake::accounts = {makeAccount(100001, 125050)};
    fake::accounts[0].name = "Asha Rao";
    fake::accounts[0].contactNumber = "9876543210";
    fake::failLoadAccounts = false;
    fake::authCalls = 0;
    fake::loadCalls = 0;
}

bool runSearch(const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = accountSearch();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    output = out.str();
    return ok;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool noDetailsShown(const std::string& output) {
    return !contains(output, "Name") && !contains(output, "Rs.") && !contains(output, "Status");
}

bool noPinShown(const std::string& output) {
    return !contains(output, SECRET_PIN) && !contains(output, "PIN");
}

void testSuccessfulSearch() {
    reset();
    std::string output;
    bool ok = runSearch("100001\n", output);
    check(ok, "Existing account 100001 found");
    check(contains(output, "Account number : 100001\n")
          && contains(output, "Name           : Asha Rao\n")
          && contains(output, "Address        : 12 MG Road, Bengaluru\n")
          && contains(output, "Contact number : 9876543210\n")
          && contains(output, "Balance        : Rs. 1250.50\n")
          && contains(output, "Status         : ACTIVE\n"),
          "All account details shown: number, name, address, contact, balance, status");
    check(noPinShown(output), "PIN is not shown for a found account");
    check(fake::loadCalls == 1, "loadAccounts called exactly once");

    reset();
    ok = runSearch("  100001 \t\n", output);
    check(ok && contains(output, "Account number : 100001"),
          "Spaces around the account number are trimmed");
}

void testOnlyRequestedAccount() {
    reset();
    fake::accounts = {makeAccount(100001, 111111), makeAccount(100002, 222222),
                      makeAccount(100003, 333333)};
    std::string output;
    bool ok = runSearch("100002\n", output);
    check(ok && contains(output, "Account number : 100002") && contains(output, "Rs. 2222.22")
          && !contains(output, "100001") && !contains(output, "100003")
          && !contains(output, "1111.11") && !contains(output, "3333.33"),
          "Only the requested account is shown when several exist");
}

void testAccountStatus() {
    reset();
    fake::accounts[0].status = STATUS_CLOSED;
    std::string output;
    bool ok = runSearch("100001\n", output);
    check(ok && contains(output, "Status         : CLOSED\n")
          && contains(output, "Account number : 100001") && noPinShown(output),
          "CLOSED account is found and shown with status CLOSED");

    reset();
    fake::accounts[0].status = "FROZEN";
    ok = runSearch("100001\n", output);
    check(ok && contains(output, "Status         : FROZEN\n"),
          "Other status (FROZEN) is found and shown as stored");

    reset();
    fake::accounts = {makeAccount(100001, 500, STATUS_CLOSED), makeAccount(100002, 700)};
    ok = runSearch("100002\n", output);
    check(ok && contains(output, "Account number : 100002")
          && contains(output, "Status         : ACTIVE"),
          "ACTIVE account found next to a CLOSED one");
}

void testAccountNotFound() {
    reset();
    std::string output;
    bool ok = runSearch("100002\n", output);
    check(!ok && contains(output, "Error: Account not found.") && noDetailsShown(output),
          "Nonexistent account: 'Account not found', no details");

    reset();
    fake::accounts.clear();
    ok = runSearch("100001\n", output);
    check(!ok && contains(output, "Error: Account not found.") && noDetailsShown(output),
          "No stored accounts at all: 'Account not found'");

    reset();
    fake::accounts = {makeAccount(1000010, 100), makeAccount(10000, 100)};
    ok = runSearch("100001\n", output);
    check(!ok && contains(output, "Account not found"),
          "Only an exact number matches (no prefix or substring match)");
}

void testInvalidAccountNumberInput() {
    const char* inputs[] = {"", "   ", "abc", "-5", "0", "+5", "12a", "1.5", "1e5",
                            "2147483648", "99999999999", "100 001"};
    for (const char* text : inputs) {
        reset();
        std::string output;
        bool ok = runSearch(std::string(text) + "\n", output);
        check(!ok && contains(output, "positive whole number") && fake::loadCalls == 0
              && noDetailsShown(output),
              std::string("Account number \"") + text + "\" rejected; storage not read");
    }

    reset();
    fake::accounts.push_back(makeAccount(INT_MAX, 500));
    std::string output;
    bool ok = runSearch("2147483647\n", output);
    check(ok && contains(output, "Account number : 2147483647"),
          "Largest int account number 2147483647 accepted");

    reset();
    ok = runSearch("", output);
    check(!ok && fake::loadCalls == 0 && noDetailsShown(output),
          "End of input before the account number: returns false, storage not read");
}

void testAuthenticationRequired() {
    reset();
    fake::employeeLoggedIn = false;
    std::string output;
    bool ok = runSearch("100001\n", output);
    check(!ok && contains(output, "Access denied: employee login required.")
          && !contains(output, "Account number:") && fake::loadCalls == 0
          && noDetailsShown(output),
          "Not logged in: console flow denies access before prompting; storage not read");

    reset();
    fake::employeeLoggedIn = false;
    SearchAccountResult result = searchAccount(100001);
    check(!result.success && result.message == "Access denied: employee login required."
          && fake::loadCalls == 0 && result.account.accountNumber == 0
          && result.account.name.empty(),
          "Not logged in: searchAccount() denies access and returns no account data");

    reset();
    runSearch("100001\n", output);
    check(fake::authCalls >= 1, "Employee authentication is checked on every search");
}

void testStorageFailure() {
    reset();
    fake::failLoadAccounts = true;
    std::string output;
    bool ok = runSearch("100001\n", output);
    check(!ok && contains(output, "Storage error: could not read account data.")
          && noDetailsShown(output),
          "loadAccounts failure: storage error, no details");

    reset();
    fake::failLoadAccounts = true;
    SearchAccountResult result = searchAccount(100001);
    check(!result.success && contains(result.message, "Storage error"),
          "loadAccounts failure: searchAccount() reports a storage error");
}

void testPinNeverExposed() {
    reset();
    SearchAccountResult result = searchAccount(100001);
    check(result.success && result.account.pin.empty(),
          "searchAccount() returns the account with the PIN cleared");
    check(fake::accounts[0].pin == SECRET_PIN,
          "Stored account's PIN is not changed by the search");

    const char* inputs[] = {"100001\n", "100002\n", "abc\n"};
    bool leaked = false;
    for (const char* text : inputs) {
        reset();
        std::string output;
        runSearch(text, output);
        if (!noPinShown(output)) leaked = true;
    }
    reset();
    fake::accounts[0].status = STATUS_CLOSED;
    std::string output;
    runSearch("100001\n", output);
    if (!noPinShown(output)) leaked = true;
    check(!leaked, "PIN never appears in output (found, closed, not found, invalid input)");
}

void testFormatting() {
    struct { long long paise; const char* shown; } cases[] = {
        {125050, "Rs. 1250.50"}, {1, "Rs. 0.01"}, {0, "Rs. 0.00"}, {10, "Rs. 0.10"},
        {100000, "Rs. 1000.00"}, {-500, "Rs. -5.00"}, {LLONG_MAX, "Rs. 92233720368547758.07"}};
    for (const auto& c : cases) {
        reset();
        fake::accounts[0].balance = c.paise;
        std::string output;
        bool ok = runSearch("100001\n", output);
        check(ok && contains(output, std::string("Balance        : ") + c.shown + "\n"),
              std::to_string(c.paise) + " paise shown as " + c.shown);
    }
}

void testDirectApi() {
    reset();
    SearchAccountResult result = searchAccount(100001);
    const Account& stored = fake::accounts[0];
    check(result.success && result.message.empty()
          && result.account.accountNumber == stored.accountNumber
          && result.account.name == stored.name && result.account.address == stored.address
          && result.account.contactNumber == stored.contactNumber
          && result.account.balance == stored.balance && result.account.status == stored.status,
          "searchAccount() returns success and copies every field except the PIN");

    reset();
    result = searchAccount(999999);
    check(!result.success && result.message == "Error: Account not found."
          && result.account.accountNumber == 0,
          "searchAccount() on a missing number: not found, no account data");

    for (int bad : {0, -1, INT_MIN}) {
        reset();
        result = searchAccount(bad);
        check(!result.success && contains(result.message, "positive whole number")
              && fake::loadCalls == 0,
              "searchAccount(" + std::to_string(bad) + ") rejected; storage not read");
    }
}

} // namespace

int main() {
    std::cout << "FR-7 account search tests. Employee login and storage are test doubles:\n"
                 "these results do NOT verify FR-10, FR-13 or persistence.\n\n";

    testSuccessfulSearch();
    testOnlyRequestedAccount();
    testAccountStatus();
    testAccountNotFound();
    testInvalidAccountNumberInput();
    testAuthenticationRequired();
    testStorageFailure();
    testPinNeverExposed();
    testFormatting();
    testDirectApi();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
