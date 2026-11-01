// SEC-2 customer access-control tests (Test Plan TC-13): deposit, withdrawal, balance enquiry and
// mini-statement need a valid account number and PIN.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_sec2.exe tests_sec2.cpp auth.cpp storage.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_sec2.cpp auth.cpp storage.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp /Fe:tests_sec2.exe
//
// NO test doubles: uses the REAL auth.cpp (FR-11), storage.cpp and the four customer flows.
// Each case starts from the same saved data and types its input into the real console flow.
// "Denied" means: the access-denied message is shown, no success output, the flow never asks for
// an amount, and both data files are byte-for-byte unchanged with their timestamps untouched
// (every save rewrites a whole file, so nothing was saved).
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+.
// Data lives only in a temporary folder that this program creates and deletes.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "auth.h"
#include "balance_enquiry.h"
#include "mini_statement.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_sec2_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const std::string ACCESS_DENIED = "Access denied: invalid account number or PIN.";

int passed = 0;
int failed = 0;

std::string seedAccounts;
std::string seedTransactions;
fs::file_time_type oldTime;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string readBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeBytes(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// Puts back the saved data and dates both files a day ago.
void restoreSeed() {
    std::error_code ec;
    fs::remove_all("data", ec);
    fs::create_directories("data", ec);
    writeBytes(ACC_FILE, seedAccounts);
    writeBytes(TXN_FILE, seedTransactions);
    fs::last_write_time(ACC_FILE, oldTime, ec);
    fs::last_write_time(TXN_FILE, oldTime, ec);
}

bool dataUnchanged() {
    std::error_code ec;
    return readBytes(ACC_FILE) == seedAccounts && readBytes(TXN_FILE) == seedTransactions
           && fs::last_write_time(ACC_FILE, ec) == oldTime
           && fs::last_write_time(TXN_FILE, ec) == oldTime
           && !fs::exists(ACC_FILE.string() + ".tmp") && !fs::exists(TXN_FILE.string() + ".tmp");
}

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

// authenticateCustomer prints its own message on failure; keep it out of the test report.
bool quietAuthenticate(int accountNo, const std::string& pin) {
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = authenticateCustomer(accountNo, pin);
    std::cout.rdbuf(oldOut);
    return ok;
}

long long storedBalance(int accountNo) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return -1;
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo) return a.balance;
    return -1;
}

size_t storedTransactionCount() {
    std::vector<Transaction> txns;
    return loadTransactions(txns) ? txns.size() : 0;
}

struct Flow {
    const char* name;
    bool (*run)();
    const char* amountLine;      // typed after the PIN ("" for read-only flows)
    const char* amountPrompt;    // must never appear when access is denied
    const char* successText;
};

const Flow FLOWS[] = {
    {"Deposit", depositMoney, "500\n", "Deposit amount", "Deposit successful"},
    {"Withdrawal", withdrawMoney, "200\n", "Withdrawal amount", "Withdrawal successful"},
    {"Balance enquiry", balanceEnquiry, "", "Deposit amount", "Current balance"},
    {"Mini-statement", miniStatement, "", "Deposit amount", "Recent transactions"},
};

// Credentials that must be denied, with the reason shown in the test name.
struct BadLogin { const char* label; const char* account; const char* pin; };
const BadLogin BAD_LOGINS[] = {
    {"wrong PIN", "100001", "9999"},
    {"another account's PIN", "100001", "4321"},
    {"unknown account", "100099", "1234"},
    {"CLOSED account with its correct PIN", "100002", "5678"},
    {"empty PIN", "100001", ""},
    {"PIN of only spaces", "100001", "   "},
};

void testDenied() {
    for (const Flow& flow : FLOWS) {
        for (const BadLogin& bad : BAD_LOGINS) {
            restoreSeed();
            std::string output;
            std::string input = std::string(bad.account) + "\n" + bad.pin + "\n" + flow.amountLine;
            bool ok = runConsole(flow.run, input, output);
            check(!ok && contains(output, ACCESS_DENIED) && !contains(output, flow.successText)
                      && !contains(output, flow.amountPrompt) && dataUnchanged(),
                  std::string("TC-13 ") + flow.name + ", " + bad.label
                      + ": access denied, operation not run, nothing saved");
        }

        // Input ends before the PIN. As in tests_nfr3.cpp, a flow may return without a message
        // at end of input (nobody is left to read it), but it must not run the operation.
        restoreSeed();
        std::string output;
        bool ok = runConsole(flow.run, "100001\n", output);
        check(!ok && !contains(output, flow.successText) && !contains(output, flow.amountPrompt)
                  && dataUnchanged(),
              std::string("TC-13 ") + flow.name
                  + ", input ends before the PIN: operation not run, nothing saved");
    }
}

void testAllowed() {
    std::string output;

    restoreSeed();
    size_t before = storedTransactionCount();
    bool ok = runConsole(depositMoney, "100001\n1234\n500\n", output);
    check(ok && contains(output, "Deposit successful") && storedBalance(100001) == 150000
              && storedTransactionCount() == before + 1,
          "TC-13 Deposit, correct account and PIN: Rs. 500.00 deposited and recorded");

    restoreSeed();
    ok = runConsole(withdrawMoney, "100001\n1234\n200\n", output);
    check(ok && contains(output, "Withdrawal successful") && storedBalance(100001) == 80000
              && storedTransactionCount() == before + 1,
          "TC-13 Withdrawal, correct account and PIN: Rs. 200.00 withdrawn and recorded");

    restoreSeed();
    ok = runConsole(balanceEnquiry, "100001\n1234\n", output);
    check(ok && contains(output, "Current balance for account 100001: Rs. 1000.00") && dataUnchanged(),
          "TC-13 Balance enquiry, correct account and PIN: balance shown, nothing saved");

    restoreSeed();
    ok = runConsole(balanceEnquiry, "100003\n4321\n", output);
    check(ok && contains(output, "Current balance for account 100003: Rs. 200.00"),
          "TC-13 Balance enquiry, a second account with its own PIN: its balance shown");

    restoreSeed();
    ok = runConsole(miniStatement, "100001\n1234\n", output);
    check(ok && contains(output, "Recent transactions for account 100001")
              && contains(output, "100.00") && !contains(output, "50.00") && dataUnchanged(),
          "TC-13 Mini-statement, correct account and PIN: only that account's transactions");

    restoreSeed();
    ok = runConsole(balanceEnquiry, " 100001 \n 1234 \n", output);
    check(ok && contains(output, "Rs. 1000.00"),
          "Spaces around the account number and PIN are ignored (flows trim them)");
}

void testAuthenticateCustomerDirectly() {
    restoreSeed();
    check(quietAuthenticate(100001, "1234"), "authenticateCustomer: correct account and PIN -> true");
    check(!quietAuthenticate(100001, "9999"), "authenticateCustomer: wrong PIN -> false");
    check(!quietAuthenticate(100001, "4321"), "authenticateCustomer: another account's PIN -> false");
    check(!quietAuthenticate(100099, "1234"), "authenticateCustomer: unknown account -> false");
    check(!quietAuthenticate(100002, "5678"), "authenticateCustomer: CLOSED account -> false");
    check(!quietAuthenticate(100001, ""), "authenticateCustomer: empty PIN -> false");
    check(!quietAuthenticate(0, "1234") && !quietAuthenticate(-1, "1234"),
          "authenticateCustomer: account number 0 / -1 -> false");
    check(dataUnchanged(), "authenticateCustomer never saves anything");
}

Account makeAccount(int number, const std::string& pin, long long balance, const std::string& status) {
    Account a;
    a.accountNumber = number;
    a.name = "Customer " + std::to_string(number);
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = std::to_string(9000000000LL + number);   // 10 digits, one per account
    a.pin = pin;
    a.balance = balance;
    a.status = status;
    return a;
}

Transaction makeTxn(int id, int accountNo, long long amount) {
    Transaction t;
    t.transactionId = id;
    t.accountNumber = accountNo;
    t.type = TXN_DEPOSIT;
    t.amount = amount;
    t.dateTime = "2026-10-30 10:00:00";
    return t;
}

// 100001 ACTIVE (PIN 1234, Rs. 1000), 100002 CLOSED (PIN 5678), 100003 ACTIVE (PIN 4321,
// Rs. 200), one deposit each (Rs. 100.00, 50.00, 20.00), saved with the real storage.
bool createSeedData() {
    bool ok = saveAccount(makeAccount(100001, "1234", 100000, STATUS_ACTIVE))
              && saveAccount(makeAccount(100002, "5678", 50000, STATUS_CLOSED))
              && saveAccount(makeAccount(100003, "4321", 20000, STATUS_ACTIVE))
              && saveTransaction(makeTxn(1, 100001, 10000))
              && saveTransaction(makeTxn(2, 100002, 5000))
              && saveTransaction(makeTxn(3, 100003, 2000));
    seedAccounts = readBytes(ACC_FILE);
    seedTransactions = readBytes(TXN_FILE);
    oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    return ok;
}

} // namespace

int main() {
    std::cout << "SEC-2 customer access-control tests with the REAL auth.cpp, storage.cpp and\n"
                 "customer flows (no test doubles). Data lives only in a temporary folder.\n\n";

    std::error_code ec;
    std::random_device rd;
    std::string folderName = TEMP_PREFIX
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_"
        + std::to_string(rd());
    fs::path original = fs::current_path(ec);
    fs::path testRoot = fs::temp_directory_path(ec) / folderName;
    if (ec || !fs::create_directory(testRoot, ec) || ec) {
        std::cout << "Could not create a temporary test folder; no tests run.\n";
        return 2;
    }
    fs::current_path(testRoot, ec);
    if (ec) {
        std::cout << "Could not switch to the temporary test folder; no tests run.\n";
        return 2;
    }
    std::cout << "Temporary folder: " << testRoot.string() << "\n\n";

    bool seeded = createSeedData();
    check(seeded, "Seed data saved: 100001 ACTIVE, 100002 CLOSED, 100003 ACTIVE, 3 deposits");
    if (seeded) {
        testDenied();
        testAllowed();
        testAuthenticateCustomerDirectly();
    }

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
