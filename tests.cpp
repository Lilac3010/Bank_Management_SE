// FR-1 unit tests.
// Build: g++ -std=c++17 -Wall -Wextra -o tests.exe tests.cpp account.cpp
//    or: cl /std:c++17 /EHsc /W4 tests.cpp account.cpp /Fe:tests.exe
//
// Build ONLY tests.cpp + account.cpp. Never build every .cpp together:
//   - tests.cpp and main.cpp both define main().
//   - The fakes below clash with the real auth.cpp / storage.cpp (LNK2005 / multiple definition).
//
// Money is in paise: Rs. 1000.00 = 100000.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "account.h"
#include "auth.h"
#include "storage.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces whose implementations do not exist yet:
//   isEmployeeAuthenticated  -> FR-10 (auth.cpp)
//   generateAccountNumber    -> FR-2  (account.cpp)
//   loadAccounts/saveAccount -> FR-13 (storage.cpp)
// Defined only in this file; never linked into main.
// When FR-2 is implemented in account.cpp, delete the fake generateAccountNumber
// (account.cpp is always part of this build) and rewrite the tests that use
// fake::generateCalls / fake::forceNumber.
namespace fake {
bool employeeLoggedIn = false;
std::vector<Account> store;
bool failLoad = false;      // when true, loadAccounts reports a read failure
bool failNextSave = false;
int saveCalls = 0;
int generateCalls = 0;
bool forceNumber = false;   // when true, the generator returns forcedNumber
int forcedNumber = 0;
}

bool isEmployeeAuthenticated() { return fake::employeeLoggedIn; }

bool loadAccounts(std::vector<Account>& accounts) {
    if (fake::failLoad) return false;
    accounts = fake::store;
    return true;
}

bool saveAccount(const Account& account) {
    ++fake::saveCalls;
    if (fake::failNextSave) {
        fake::failNextSave = false;
        return false;
    }
    for (Account& a : fake::store) {
        if (a.accountNumber == account.accountNumber) {
            a = account;
            return true;
        }
    }
    fake::store.push_back(account);
    return true;
}

int generateAccountNumber(const std::vector<Account>& existing) {
    ++fake::generateCalls;
    if (fake::forceNumber) return fake::forcedNumber;
    int highest = 100000;
    for (const Account& a : existing)
        if (a.accountNumber > highest) highest = a.accountNumber;
    return highest + 1;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

void reset(bool loggedIn = true) {
    fake::employeeLoggedIn = loggedIn;
    fake::store.clear();
    fake::failLoad = false;
    fake::failNextSave = false;
    fake::saveCalls = 0;
    fake::generateCalls = 0;
    fake::forceNumber = false;
    fake::forcedNumber = 0;
}

Account makeAccount(int number, const std::string& name, const std::string& contact) {
    Account a;
    a.accountNumber = number;
    a.name = name;
    a.address = "12 MG Road";
    a.contactNumber = contact;
    a.pin = "1234";
    a.balance = 100000;
    return a;
}

void expectRejected(const std::string& label, const std::string& name,
                    const std::string& address, const std::string& contact,
                    const std::string& pin, long long depositPaise) {
    reset();
    CreateAccountResult r = createAccountWithDetails(name, address, contact, pin, depositPaise);
    check(!r.success && r.accountNumber == 0 && !r.message.empty()
          && fake::store.empty() && fake::saveCalls == 0, label);
}

bool runConsole(const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = createAccount();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    output = out.str();
    return ok;
}

std::string consoleInputWithDeposit(const std::string& deposit) {
    return "Asha Rao\n12 MG Road\n9876543210\n1234\n" + deposit + "\n";
}

// TC-01 (FR-1 part)
void testValidCreation() {
    reset();
    CreateAccountResult r = createAccountWithDetails("Asha Rao", "12 MG Road, Bengaluru",
                                                     "9876543210", "1234", 100000);
    check(r.success, "TC-01 valid details create an account");
    check(fake::store.size() == 1, "TC-01 exactly one account saved");
    if (fake::store.size() != 1) return;
    const Account& a = fake::store[0];
    check(a.accountNumber == r.accountNumber && r.accountNumber != 0,
          "TC-01 returned account number matches saved record");
    check(a.balance == 100000, "TC-01 balance equals initial deposit (100000 paise)");
    check(a.status == STATUS_ACTIVE, "TC-01 new account is ACTIVE");
    check(a.name == "Asha Rao" && a.address == "12 MG Road, Bengaluru"
          && a.contactNumber == "9876543210" && a.pin == "1234",
          "TC-01 customer details stored as entered");
    check(r.message.find("Account number: " + std::to_string(r.accountNumber)) != std::string::npos,
          "TC-01 success message shows account number");
    check(r.message.find("Opening balance: Rs. 1000.00") != std::string::npos,
          "TC-01 success message shows opening balance");
}

// Accepted edge values (assumptions A4, A7, deposit rules)
void testAcceptedEdgeCases() {
    reset();
    CreateAccountResult r = createAccountWithDetails("  Asha Rao  ", "  12 MG Road  ",
                                                     " 9876543210 ", " 1234 ", 100000);
    check(r.success && fake::store.size() == 1 && fake::store[0].name == "Asha Rao"
          && fake::store[0].address == "12 MG Road" && fake::store[0].contactNumber == "9876543210"
          && fake::store[0].pin == "1234",
          "A7 leading/trailing spaces trimmed before storing");

    reset();
    r = createAccountWithDetails("Dr. A. O'Brien", "12 MG Road", "9876543210", "1234", 100000);
    check(r.success, "A4 name with '.' and apostrophe accepted");

    reset();
    r = createAccountWithDetails("Asha Rao", "12 MG Road", "9876543210", "1234", 1);
    check(r.success && fake::store.size() == 1 && fake::store[0].balance == 1
          && r.message.find("Opening balance: Rs. 0.01") != std::string::npos,
          "Smallest deposit (1 paisa = Rs. 0.01) accepted");

    reset();
    r = createAccountWithDetails("Asha Rao", "12 MG Road", "9876543210", "1234", 100050);
    check(r.success && r.message.find("Opening balance: Rs. 1000.50") != std::string::npos,
          "Deposit of 100050 paise shown as Rs. 1000.50");
}

// FR-1 use of the FR-2 interface. Uses the fake generator; does NOT verify FR-2.
void testUsesGenerator() {
    reset();
    CreateAccountResult first = createAccountWithDetails("Asha Rao", "12 MG Road",
                                                         "9876543210", "1234", 100000);
    CreateAccountResult second = createAccountWithDetails("Ravi Kumar", "5 Park Street",
                                                          "9123456780", "5678", 250000);
    check(first.success && second.success, "FR-1 two valid accounts created");
    check(first.accountNumber != second.accountNumber,
          "FR-1 two accounts get different numbers (fake generator)");
    check(fake::generateCalls == 2, "FR-1 calls generateAccountNumber once per account (fake)");
}

// FR-1 guard against a bad generated number.
void testGeneratedNumberChecked() {
    reset();
    fake::store.push_back(makeAccount(100001, "Asha Rao", "9876543210"));
    fake::forceNumber = true;
    fake::forcedNumber = 100001;
    CreateAccountResult r = createAccountWithDetails("Ravi Kumar", "5 Park Street",
                                                     "9123456780", "5678", 10000);
    check(!r.success && r.accountNumber == 0 && fake::saveCalls == 0
          && fake::store.size() == 1 && fake::store[0].name == "Asha Rao"
          && fake::store[0].balance == 100000,
          "FR-1 rejects a generated number already in use; existing account untouched (fake)");

    reset();
    fake::forceNumber = true;
    fake::forcedNumber = 0;
    r = createAccountWithDetails("Ravi Kumar", "5 Park Street", "9123456780", "5678", 10000);
    check(!r.success && fake::saveCalls == 0, "FR-1 rejects generated number 0 (fake)");

    reset();
    fake::forceNumber = true;
    fake::forcedNumber = -5;
    r = createAccountWithDetails("Ravi Kumar", "5 Park Street", "9123456780", "5678", 10000);
    check(!r.success && fake::saveCalls == 0, "FR-1 rejects a negative generated number (fake)");
}

// TC-02 (FR-1, FR-12) invalid or missing details
void testInvalidDetails() {
    expectRejected("TC-02 empty name rejected", "", "12 MG Road", "9876543210", "1234", 100000);
    expectRejected("TC-02 whitespace-only name rejected", "   ", "12 MG Road", "9876543210", "1234", 100000);
    expectRejected("TC-02 name with digits rejected", "Asha 2", "12 MG Road", "9876543210", "1234", 100000);
    expectRejected("TC-02 name of only punctuation rejected", ". '", "12 MG Road", "9876543210", "1234", 100000);
    expectRejected("TC-02 empty address rejected", "Asha Rao", "", "9876543210", "1234", 100000);
    expectRejected("TC-02 whitespace-only address rejected", "Asha Rao", "   ", "9876543210", "1234", 100000);
    expectRejected("TC-02 address with '|' rejected", "Asha Rao", "12 | MG Road", "9876543210", "1234", 100000);
    expectRejected("TC-02 empty contact rejected", "Asha Rao", "12 MG Road", "", "1234", 100000);
    expectRejected("TC-02 whitespace-only contact rejected", "Asha Rao", "12 MG Road", "   ", "1234", 100000);
    expectRejected("TC-02 9-digit contact rejected", "Asha Rao", "12 MG Road", "987654321", "1234", 100000);
    expectRejected("TC-02 11-digit contact rejected", "Asha Rao", "12 MG Road", "98765432101", "1234", 100000);
    expectRejected("TC-02 non-numeric contact rejected", "Asha Rao", "12 MG Road", "98765abcde", "1234", 100000);
    expectRejected("TC-02 contact with spaces inside rejected", "Asha Rao", "12 MG Road", "98765 43210", "1234", 100000);
    expectRejected("TC-02 empty PIN rejected", "Asha Rao", "12 MG Road", "9876543210", "", 100000);
    expectRejected("TC-02 3-digit PIN rejected", "Asha Rao", "12 MG Road", "9876543210", "123", 100000);
    expectRejected("TC-02 5-digit PIN rejected", "Asha Rao", "12 MG Road", "9876543210", "12345", 100000);
    expectRejected("TC-02 non-numeric PIN rejected", "Asha Rao", "12 MG Road", "9876543210", "12a4", 100000);
}

// Initial deposit (assumption A2)
void testInvalidDeposit() {
    expectRejected("Deposit of 0 paise rejected", "Asha Rao", "12 MG Road", "9876543210", "1234", 0);
    expectRejected("Negative deposit rejected", "Asha Rao", "12 MG Road", "9876543210", "1234", -50000);
}

// TC-02 duplicate data (assumption A6)
void testDuplicateContact() {
    reset();
    fake::store.push_back(makeAccount(100001, "Asha Rao", "9876543210"));

    CreateAccountResult r = createAccountWithDetails("Someone Else", "7 Hill Road",
                                                     "9876543210", "4321", 50000);
    check(!r.success && fake::store.size() == 1 && fake::saveCalls == 0,
          "TC-02 duplicate active contact rejected, nothing saved");

    fake::store[0].status = STATUS_CLOSED;
    r = createAccountWithDetails("Someone Else", "7 Hill Road", "9876543210", "4321", 50000);
    check(r.success && fake::store.size() == 2, "Contact of a CLOSED account may be reused");
}

// SEC-1, SEC-3: FR-1's own login guard, using the fake auth flag. Real TC-12 needs auth.cpp.
void testUnauthenticated() {
    reset(false);
    CreateAccountResult r = createAccountWithDetails("Asha Rao", "12 MG Road",
                                                     "9876543210", "1234", 100000);
    check(!r.success && r.message.find("Access denied") != std::string::npos,
          "SEC-1 FR-1 guard denies unauthenticated employee (fake auth)");
    check(fake::store.empty() && fake::saveCalls == 0 && fake::generateCalls == 0,
          "SEC-1 no account generated or saved when denied");

    std::string output;
    bool ok = runConsole(consoleInputWithDeposit("1000"), output);
    check(!ok && fake::store.empty() && output.find("Access denied") != std::string::npos,
          "SEC-1 console createAccount denied without login (fake auth)");
}

// SAD §5 read/write failures: FR-1's handling of a false return. Does NOT verify FR-13.
void testStorageFailure() {
    reset();
    fake::store.push_back(makeAccount(100001, "Asha Rao", "9876543210"));
    fake::failLoad = true;
    CreateAccountResult r = createAccountWithDetails("Ravi Kumar", "5 Park Street",
                                                     "9123456780", "5678", 10000);
    check(!r.success && r.accountNumber == 0 && fake::generateCalls == 0 && fake::saveCalls == 0
          && fake::store.size() == 1 && fake::store[0].name == "Asha Rao"
          && r.message.find("Storage error") != std::string::npos,
          "FR-1 stops on loadAccounts failure; nothing generated or saved (fake storage)");

    reset();
    fake::failNextSave = true;
    r = createAccountWithDetails("Asha Rao", "12 MG Road", "9876543210", "1234", 100000);
    check(!r.success && r.accountNumber == 0 && fake::store.empty()
          && r.message.find("Storage error") != std::string::npos,
          "FR-1 reports saveAccount failure as an error (fake storage)");
}

// TC-14 / console flow and rupee-to-paise parsing
void testConsole() {
    const char* badDeposits[] = {"abc", "", "-500", "0x10", "1e3", "+500", "inf", "nan",
                                 "12.345", "1,000", "1000.", ".50", "1.2.3",
                                 "1234567890123456"};
    for (const char* deposit : badDeposits) {
        reset();
        std::string output;
        bool ok = runConsole(consoleInputWithDeposit(deposit), output);
        check(!ok && fake::store.empty() && fake::saveCalls == 0
              && output.find("Error") != std::string::npos,
              std::string("TC-14 console deposit \"") + deposit + "\" rejected, nothing saved");
    }

    struct { const char* text; long long paise; } goodDeposits[] = {
        {"1000", 100000}, {"1000.5", 100050}, {"1000.50", 100050},
        {"0.01", 1}, {"0.10", 10}, {"007", 700}, {"123456789012345", 12345678901234500LL}};
    for (const auto& deposit : goodDeposits) {
        reset();
        std::string output;
        bool ok = runConsole(consoleInputWithDeposit(deposit.text), output);
        check(ok && fake::store.size() == 1 && fake::store[0].balance == deposit.paise,
              std::string("Console deposit \"") + deposit.text + "\" stored as "
                  + std::to_string(deposit.paise) + " paise");
    }

    reset();
    std::string output;
    bool ok = runConsole(consoleInputWithDeposit("0"), output);
    check(!ok && fake::store.empty() && output.find("positive") != std::string::npos,
          "Console deposit \"0\" rejected as not positive");

    reset();
    ok = runConsole(consoleInputWithDeposit("1000"), output);
    check(ok && fake::store.size() == 1
          && output.find("Account number: " + std::to_string(fake::store[0].accountNumber))
             != std::string::npos
          && output.find("Opening balance: Rs. 1000.00") != std::string::npos,
          "TC-01 console flow creates account and shows number and balance");
}

} // namespace

int main() {
    std::cout << "FR-1 unit tests. Employee login, account-number generation and storage are\n"
                 "test doubles: these results do NOT verify FR-2, FR-10, FR-13 or persistence.\n\n";

    testValidCreation();
    testAcceptedEdgeCases();
    testUsesGenerator();
    testGeneratedNumberChecked();
    testInvalidDetails();
    testInvalidDeposit();
    testDuplicateContact();
    testUnauthenticated();
    testStorageFailure();
    testConsole();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
