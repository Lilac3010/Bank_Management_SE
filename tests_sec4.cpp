// SEC-4 tests: zero and negative deposit / withdrawal amounts are rejected (Test Plan TC-04,
// TC-06 as mapped in the SRS RTM, and Test Plan section 5.1 step 6).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_sec4.exe tests_sec4.cpp transactions.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_sec4.cpp transactions.cpp /Fe:tests_sec4.exe
//
// Build ONLY these two files. Never link other test files (they define main() and the
// same fakes) or main.cpp / auth.cpp / storage.cpp into this program.
//
// Money is in paise: Rs. 1000.00 = 100000.
#include <climits>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "auth.h"
#include "storage.h"
#include "transactions.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces this suite does not test:
//   authenticateCustomer                  -> FR-11 (auth.cpp)
//   loadAccounts/saveAccount              -> FR-13 (storage.cpp)
//   loadTransactions/saveTransaction      -> FR-13 (storage.cpp)
// Defined only in this file; never linked into main.
namespace fake {
std::vector<Account> accounts;
std::vector<Transaction> transactions;
int saveAccountCalls = 0;
int saveTransactionCalls = 0;
}

// Accepts the stored PIN of a stored account.
bool authenticateCustomer(int accountNo, const std::string& pin) {
    for (const Account& a : fake::accounts)
        if (a.accountNumber == accountNo && a.pin == pin) return true;
    return false;
}

bool loadAccounts(std::vector<Account>& accounts) {
    accounts = fake::accounts;
    return true;
}

bool saveAccount(const Account& account) {
    ++fake::saveAccountCalls;
    for (Account& a : fake::accounts) {
        if (a.accountNumber == account.accountNumber) {
            a = account;
            return true;
        }
    }
    fake::accounts.push_back(account);
    return true;
}

bool loadTransactions(std::vector<Transaction>& transactions) {
    transactions = fake::transactions;
    return true;
}

bool saveTransaction(const Transaction& txn) {
    ++fake::saveTransactionCalls;
    fake::transactions.push_back(txn);
    return true;
}

// Not used by transactions.cpp; defined so every auth.h function exists.
bool loginEmployee() { return false; }
bool isEmployeeAuthenticated() { return false; }
// =================== END TEST-ONLY FAKES ===================

namespace {

const int ACCOUNT_NO = 100001;
const long long START_BALANCE = 100000;   // Rs. 1000.00

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

Account startAccount() {
    Account a;
    a.accountNumber = ACCOUNT_NO;
    a.name = "Asha Rao";
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = "9876543210";
    a.pin = "1234";
    a.balance = START_BALANCE;
    a.status = STATUS_ACTIVE;
    return a;
}

void reset() {
    fake::accounts = {startAccount()};
    fake::transactions.clear();
    fake::saveAccountCalls = 0;
    fake::saveTransactionCalls = 0;
}

bool sameAccount(const Account& a, const Account& b) {
    return a.accountNumber == b.accountNumber && a.name == b.name && a.address == b.address
           && a.contactNumber == b.contactNumber && a.pin == b.pin && a.balance == b.balance
           && a.status == b.status;
}

// Balance and the whole stored record unchanged, nothing saved, no transaction recorded.
bool nothingChanged() {
    return fake::accounts.size() == 1 && sameAccount(fake::accounts[0], startAccount())
           && fake::saveAccountCalls == 0 && fake::saveTransactionCalls == 0
           && fake::transactions.empty();
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Runs a console flow with `input` as the keyboard and captures what it prints.
bool runConsole(bool (*flow)(), const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = flow();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    output = out.str();
    return ok;
}

// Amounts in paise. 0 is what "0", "0.00" and "-0" mean; -1 is Rs. -0.01.
const long long BAD_AMOUNTS[] = {0, -1, -100, -50000, LLONG_MIN};

// Text typed at the amount prompt.
const char* const BAD_TEXT[] = {"0", "0.00", "0.0", "00", " 0 ", "-0", "-0.00", "-0.01",
                                "-1", "-500"};

void testDirectCalls() {
    for (long long amount : BAD_AMOUNTS) {
        const std::string label = std::to_string(amount) + " paise";

        reset();
        DepositResult dep = deposit(ACCOUNT_NO, amount);
        check(!dep.success && contains(dep.message, "Error") && nothingChanged(),
              "TC-04 deposit(" + label + ") rejected with an error; balance, record unchanged; "
              "nothing saved; no transaction");

        reset();
        WithdrawalResult wd = withdraw(ACCOUNT_NO, amount);
        check(!wd.success && contains(wd.message, "Error") && nothingChanged(),
              "SEC-4 withdraw(" + label + ") rejected with an error; balance, record unchanged; "
              "nothing saved; no transaction");
    }
}

void testConsole() {
    for (const char* text : BAD_TEXT) {
        const std::string input = std::to_string(ACCOUNT_NO) + "\n1234\n" + text + "\n";
        const std::string label = std::string("\"") + text + "\"";
        std::string output;

        reset();
        bool ok = runConsole(depositMoney, input, output);
        check(!ok && contains(output, "Error") && !contains(output, "successful") && nothingChanged(),
              "TC-04 console deposit " + label + " rejected with an error; nothing changed");

        reset();
        ok = runConsole(withdrawMoney, input, output);
        check(!ok && contains(output, "Error") && !contains(output, "successful") && nothingChanged(),
              "SEC-4 console withdrawal " + label + " rejected with an error; nothing changed");
    }
}

// The smallest positive amount is accepted, so the checks above can see a real change.
void testSmallestPositiveAccepted() {
    reset();
    DepositResult dep = deposit(ACCOUNT_NO, 1);
    check(dep.success && fake::accounts[0].balance == START_BALANCE + 1
              && fake::transactions.size() == 1,
          "Control: deposit(1 paise) accepted and recorded");

    reset();
    WithdrawalResult wd = withdraw(ACCOUNT_NO, 1);
    check(wd.success && fake::accounts[0].balance == START_BALANCE - 1
              && fake::transactions.size() == 1,
          "Control: withdraw(1 paise) accepted and recorded");

    const std::string input = std::to_string(ACCOUNT_NO) + "\n1234\n0.01\n";
    std::string output;
    reset();
    check(runConsole(depositMoney, input, output) && fake::accounts[0].balance == START_BALANCE + 1,
          "Control: console deposit \"0.01\" accepted");
    reset();
    check(runConsole(withdrawMoney, input, output) && fake::accounts[0].balance == START_BALANCE - 1,
          "Control: console withdrawal \"0.01\" accepted");
}

} // namespace

int main() {
    std::cout << "SEC-4 tests: zero and negative transaction amounts. Customer login and storage\n"
                 "are test doubles: these results do NOT verify FR-11, FR-13 or persistence.\n\n";

    testDirectCalls();
    testConsole();
    testSmallestPositiveAccepted();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
