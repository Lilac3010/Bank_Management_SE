// FR-3 deposit tests.
//
// PARKED: this suite is not part of any documented build or test run, and has never been
// executed. The built test program was blocked by Windows Smart App Control. See README
// ("Verification status") before re-enabling it.
//
// When re-enabled, build ONLY these two files: tests_fr3.cpp transactions.cpp. Never link tests.cpp / tests_fr2.cpp (they define main()
// and the same fakes) or main.cpp / auth.cpp / storage.cpp into this program.
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
// Stand-ins for interfaces whose implementations do not exist yet:
//   authenticateCustomer                  -> FR-11 (auth.cpp)
//   loadAccounts/saveAccount              -> FR-13 (storage.cpp)
//   loadTransactions/saveTransaction      -> FR-13 (storage.cpp)
// Defined only in this file; never linked into main.
namespace fake {
std::vector<Account> accounts;
std::vector<Transaction> transactions;
bool failLoadAccounts = false;
bool failLoadTransactions = false;
int failSaveAccountOnCall = 0;   // 1-based call number that fails; 0 = never
bool failSaveTransaction = false;
int saveAccountCalls = 0;
int saveTransactionCalls = 0;
int authCalls = 0;
}

// Accepts the stored PIN of a stored account, whatever its status.
bool authenticateCustomer(int accountNo, const std::string& pin) {
    ++fake::authCalls;
    for (const Account& a : fake::accounts)
        if (a.accountNumber == accountNo && a.pin == pin) return true;
    return false;
}

bool loadAccounts(std::vector<Account>& accounts) {
    if (fake::failLoadAccounts) return false;
    accounts = fake::accounts;
    return true;
}

bool saveAccount(const Account& account) {
    ++fake::saveAccountCalls;
    if (fake::saveAccountCalls == fake::failSaveAccountOnCall) return false;
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
    if (fake::failLoadTransactions) return false;
    transactions = fake::transactions;
    return true;
}

bool saveTransaction(const Transaction& txn) {
    ++fake::saveTransactionCalls;
    if (fake::failSaveTransaction) return false;
    fake::transactions.push_back(txn);
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

// One ACTIVE account 100001 with Rs. 1000.00 (TC-03 precondition).
void reset() {
    fake::accounts = {makeAccount(100001, 100000)};
    fake::transactions.clear();
    fake::failLoadAccounts = false;
    fake::failLoadTransactions = false;
    fake::failSaveAccountOnCall = 0;
    fake::failSaveTransaction = false;
    fake::saveAccountCalls = 0;
    fake::saveTransactionCalls = 0;
    fake::authCalls = 0;
}

long long balanceOf(int accountNo) {
    for (const Account& a : fake::accounts)
        if (a.accountNumber == accountNo) return a.balance;
    return -1;
}

bool nothingChanged(long long expectedBalance) {
    return balanceOf(100001) == expectedBalance && fake::transactions.empty()
           && fake::saveAccountCalls == 0 && fake::saveTransactionCalls == 0;
}

bool looksLikeDateTime(const std::string& s) {
    if (s.size() != 19) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (i == 4 || i == 7) { if (c != '-') return false; }
        else if (i == 10)     { if (c != ' ') return false; }
        else if (i == 13 || i == 16) { if (c != ':') return false; }
        else if (c < '0' || c > '9') return false;
    }
    return true;
}

bool runConsole(const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = depositMoney();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    output = out.str();
    return ok;
}

// TC-03
void testValidDeposit() {
    reset();
    DepositResult r = deposit(100001, 50000);
    check(r.success && r.newBalance == 150000 && balanceOf(100001) == 150000,
          "TC-03 Rs. 1000 + Rs. 500 deposit gives balance Rs. 1500");
    check(r.message.find("New balance: Rs. 1500.00") != std::string::npos,
          "TC-03 success message shows new balance");

    check(fake::transactions.size() == 1, "TC-03 exactly one transaction recorded");
    if (fake::transactions.size() != 1) return;
    const Transaction& t = fake::transactions[0];
    check(t.transactionId == 1 && r.transactionId == 1, "TC-03 first transaction ID is 1");
    check(t.accountNumber == 100001 && t.type == TXN_DEPOSIT && t.type == "DEPOSIT"
          && t.amount == 50000,
          "TC-03 transaction has account 100001, type DEPOSIT, amount 50000 paise");
    check(looksLikeDateTime(t.dateTime), "TC-03 transaction date/time is YYYY-MM-DD HH:MM:SS");
}

void testMultipleDeposits() {
    reset();
    DepositResult a = deposit(100001, 10000);
    DepositResult b = deposit(100001, 25050);
    DepositResult c = deposit(100001, 1);
    check(a.success && b.success && c.success && balanceOf(100001) == 135051
          && c.newBalance == 135051,
          "Three deposits (Rs. 100, Rs. 250.50, Rs. 0.01) give Rs. 1350.51");
    check(fake::transactions.size() == 3 && fake::transactions[0].transactionId == 1
          && fake::transactions[1].transactionId == 2 && fake::transactions[2].transactionId == 3,
          "Three deposits get transaction IDs 1, 2, 3");

    reset();
    Transaction older;
    older.transactionId = 7;
    older.accountNumber = 100001;
    older.type = TXN_DEPOSIT;
    older.amount = 100;
    fake::transactions = {older};
    older.transactionId = 3;
    fake::transactions.push_back(older);
    DepositResult r = deposit(100001, 100);
    check(r.success && r.transactionId == 8 && fake::transactions.back().transactionId == 8,
          "Transaction ID continues after the highest existing ID (7 -> 8)");
}

void testOnlyTargetAccountChanges() {
    reset();
    fake::accounts = {makeAccount(100001, 100000), makeAccount(100002, 200000),
                      makeAccount(100003, 300000)};
    DepositResult r = deposit(100002, 5000);
    check(r.success && balanceOf(100001) == 100000 && balanceOf(100002) == 205000
          && balanceOf(100003) == 300000,
          "Deposit changes only the specified account when several exist");
    check(fake::transactions.size() == 1 && fake::transactions[0].accountNumber == 100002,
          "Transaction recorded against the specified account only");
}

void testRejectedDeposits() {
    reset();
    DepositResult r = deposit(999999, 50000);
    check(!r.success && r.message.find("not found") != std::string::npos && nothingChanged(100000),
          "Unknown account rejected; nothing saved");

    reset();
    fake::accounts[0].status = STATUS_CLOSED;
    r = deposit(100001, 50000);
    check(!r.success && r.message.find("closed") != std::string::npos && nothingChanged(100000),
          "CLOSED account rejected; balance unchanged, no transaction");

    reset();
    fake::accounts[0].status = "FROZEN";
    r = deposit(100001, 50000);
    check(!r.success && r.message.find("not active") != std::string::npos && nothingChanged(100000),
          "Account with a non-ACTIVE status rejected");

    reset();
    r = deposit(100001, 0);
    check(!r.success && r.message.find("positive") != std::string::npos && nothingChanged(100000),
          "TC-04 zero deposit rejected; balance unchanged, no transaction");

    reset();
    r = deposit(100001, -50000);
    check(!r.success && nothingChanged(100000),
          "TC-04 negative deposit rejected; balance unchanged, no transaction");

    reset();
    fake::accounts[0].balance = LLONG_MAX - 10;
    r = deposit(100001, 11);
    check(!r.success && nothingChanged(LLONG_MAX - 10), "Deposit that would overflow the balance rejected");
}

void testStorageFailures() {
    reset();
    fake::failLoadAccounts = true;
    DepositResult r = deposit(100001, 50000);
    check(!r.success && r.message.find("Storage error") != std::string::npos && nothingChanged(100000),
          "loadAccounts failure: no success, nothing saved");

    reset();
    fake::failLoadTransactions = true;
    r = deposit(100001, 50000);
    check(!r.success && r.message.find("Storage error") != std::string::npos && nothingChanged(100000),
          "loadTransactions failure: no success, nothing saved");

    reset();
    Transaction last;
    last.transactionId = INT_MAX;
    fake::transactions = {last};
    r = deposit(100001, 50000);
    check(!r.success && balanceOf(100001) == 100000 && fake::transactions.size() == 1
          && fake::saveAccountCalls == 0 && fake::saveTransactionCalls == 0,
          "No transaction ID available: no success, nothing saved");

    reset();
    fake::failSaveAccountOnCall = 1;
    r = deposit(100001, 50000);
    check(!r.success && r.newBalance == 0 && balanceOf(100001) == 100000
          && fake::saveTransactionCalls == 0 && fake::transactions.empty(),
          "saveAccount failure: no success, no transaction saved, balance unchanged");

    reset();
    fake::failSaveTransaction = true;
    r = deposit(100001, 50000);
    check(!r.success && balanceOf(100001) == 100000 && fake::transactions.empty()
          && fake::saveAccountCalls == 2
          && r.message.find("balance unchanged") != std::string::npos,
          "saveTransaction failure: no success, balance rolled back to Rs. 1000");

    reset();
    fake::failSaveTransaction = true;
    fake::failSaveAccountOnCall = 2;
    r = deposit(100001, 50000);
    check(!r.success && fake::transactions.empty()
          && r.message.find("may be inconsistent") != std::string::npos,
          "saveTransaction and rollback both fail: no success, inconsistency reported");
}

// SEC-2 / SEC-3 and console amount parsing
void testConsole() {
    reset();
    std::string output;
    bool ok = runConsole("100001\n9999\n500\n", output);
    check(!ok && output.find("Access denied") != std::string::npos
          && output.find("Deposit amount") == std::string::npos && nothingChanged(100000),
          "SEC-3 wrong PIN: access denied, amount never asked, nothing saved");

    reset();
    ok = runConsole("999999\n1234\n500\n", output);
    check(!ok && output.find("Access denied") != std::string::npos && nothingChanged(100000),
          "SEC-3 unknown account at login: access denied, nothing saved");

    reset();
    ok = runConsole("abc\n1234\n500\n", output);
    check(!ok && output.find("Access denied") != std::string::npos && fake::authCalls == 0
          && nothingChanged(100000),
          "Non-numeric account number denied without calling authenticateCustomer");

    reset();
    ok = runConsole("100001\n1234\n500\n", output);
    check(ok && fake::authCalls == 1 && balanceOf(100001) == 150000 && fake::transactions.size() == 1
          && output.find("New balance: Rs. 1500.00") != std::string::npos,
          "SEC-2 correct PIN: Rs. 500 deposited and new balance shown");

    reset();
    ok = runConsole("100001\n1234\n0.01\n", output);
    check(ok && balanceOf(100001) == 100001, "Console deposit \"0.01\" adds 1 paisa");

    const char* badAmounts[] = {"nan", "NaN", "inf", "-inf", "infinity", "-500", "0", "1e3",
                                "0x10", "abc", "", "12.345", "1,000"};
    for (const char* amount : badAmounts) {
        reset();
        ok = runConsole(std::string("100001\n1234\n") + amount + "\n", output);
        check(!ok && output.find("Error") != std::string::npos && nothingChanged(100000),
              std::string("Console amount \"") + amount + "\" rejected; balance unchanged, no transaction");
    }

    reset();
    fake::accounts[0].status = STATUS_CLOSED;
    ok = runConsole("100001\n1234\n500\n", output);
    check(!ok && output.find("closed") != std::string::npos && nothingChanged(100000),
          "CLOSED account rejected by deposit() even with the correct PIN");
}

} // namespace

int main() {
    std::cout << "FR-3 deposit tests. Customer login and storage are test doubles: these\n"
                 "results do NOT verify FR-11, FR-13, persistence or atomic saving.\n\n";

    testValidDeposit();
    testMultipleDeposits();
    testOnlyTargetAccountChanges();
    testRejectedDeposits();
    testStorageFailures();
    testConsole();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
