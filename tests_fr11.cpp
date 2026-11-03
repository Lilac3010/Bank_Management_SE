// FR-11 customer authentication tests (Test Plan TC-13; SRS FR-11, SEC-2, SEC-3).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr11.exe tests_fr11.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp -lcrypto
//    or: cl /std:c++17 /EHsc /W4 tests_fr11.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_fr11.exe
//
// NO test doubles: the real auth.cpp, storage.cpp, menus and the four self-service flows, so this
// must never be linked with a file that fakes login, storage or a feature.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+. On Linux, needs libssl-dev (auth.cpp
// links password_hash.cpp for FR-10).
// Data lives only in a temporary folder that this program creates and deletes.
//
// Customer authentication is per operation: there is no customer session. Each flow asks for
// the account number and PIN and calls authenticateCustomer() before it shows data or asks for
// an amount. These tests check that, and that nothing carries over between operations.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "account_closure.h"
#include "auth.h"
#include "balance_enquiry.h"
#include "menu.h"
#include "mini_statement.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_fr11_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const fs::path EMP_FILE = fs::path("data") / "employees.txt";

const std::string ACCESS_DENIED = "Access denied: invalid account number or PIN.";
const std::string OLD_AUTH_LINE = "Invalid account number or PIN.";   // removed in FR-11
const std::string BAD_NUMBER = "Error: Account number must be a positive whole number.";
const std::string EMPLOYEE_USER = "staff.one";
const std::string EMPLOYEE_PASS = "Staff-Pass-11";

int passed = 0;
int failed = 0;

std::string seedAccounts, seedTransactions, seedEmployees;
fs::file_time_type oldTime;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

size_t countOf(const std::string& text, const std::string& part) {
    size_t n = 0;
    for (size_t pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + 1)) ++n;
    return n;
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

bool inTestFolder() {
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    return !ec && cwd.filename().string().rfind(TEMP_PREFIX, 0) == 0;
}

// Puts back the seed data, dated a day ago, and ends any employee session.
void restoreSeed() {
    if (!inTestFolder()) {
        std::cout << "Refusing to touch data/ outside the test temp folder.\n";
        std::exit(3);
    }
    std::error_code ec;
    fs::remove_all("data", ec);
    fs::create_directories("data", ec);
    writeBytes(ACC_FILE, seedAccounts);
    writeBytes(TXN_FILE, seedTransactions);
    writeBytes(EMP_FILE, seedEmployees);
    for (const fs::path& p : {ACC_FILE, TXN_FILE, EMP_FILE}) fs::last_write_time(p, oldTime, ec);
    logoutEmployee();
}

// Every data file byte-for-byte unchanged and not even rewritten (timestamps untouched).
bool dataUnchanged() {
    std::error_code ec;
    for (const fs::path& p : {ACC_FILE, TXN_FILE, EMP_FILE})
        if (fs::last_write_time(p, ec) != oldTime || fs::exists(p.string() + ".tmp")) return false;
    return readBytes(ACC_FILE) == seedAccounts && readBytes(TXN_FILE) == seedTransactions
           && readBytes(EMP_FILE) == seedEmployees;
}

bool runFlow(bool (*flow)(), const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = flow();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    output = out.str();
    return ok;
}

std::string runMenu(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    runMainMenu();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    return out.str();
}

// authenticateCustomer with its output captured, to check that it prints nothing.
bool authenticate(int accountNo, const std::string& pin, std::string& printed) {
    std::ostringstream out;
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = authenticateCustomer(accountNo, pin);
    std::cout.rdbuf(oldOut);
    printed = out.str();
    return ok;
}

long long storedBalance(int accountNo) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return -1;
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo) return a.balance;
    return -1;
}

// The four self-service flows, with the main-menu key, the line typed after the PIN, and
// text that appears only once access was granted.
struct Flow {
    const char* name;
    bool (*run)();
    const char* menuKey;
    const char* afterPin;
    const char* grantedText;
};
const Flow FLOWS[] = {
    {"Deposit", depositMoney, "2", "500\n", "Deposit amount (Rs.)"},
    {"Withdrawal", withdrawMoney, "3", "200\n", "Withdrawal amount (Rs.)"},
    {"Balance enquiry", balanceEnquiry, "4", "", "Current balance"},
    {"Mini-statement", miniStatement, "5", "", "Recent transactions"},
};

// Text that shows account information or a completed operation.
bool noAccountDataShown(const std::string& output) {
    for (const char* text : {"Deposit amount", "Withdrawal amount", "Current balance",
                             "Recent transactions", "No transactions found", "successful", "Rs. "})
        if (contains(output, text)) return false;
    return true;
}

Account makeAccount(int number, const std::string& pin, long long balance, const std::string& status) {
    Account a;
    a.accountNumber = number;
    a.name = "Customer " + std::to_string(number);
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = std::to_string(9000000000LL + number);
    a.pin = pin;
    a.balance = balance;
    a.status = status;
    return a;
}

// 100001 ACTIVE PIN 1234 Rs. 1000; 100002 CLOSED PIN 5678; 100003 ACTIVE PIN 0420 Rs. 200;
// 100004 FROZEN PIN 1111; one deposit on 100001; one employee. Saved with the real storage.
bool createSeedData() {
    Transaction txn;
    txn.transactionId = 1;
    txn.accountNumber = 100001;
    txn.type = TXN_DEPOSIT;
    txn.amount = 10000;
    txn.dateTime = "2026-11-03 10:00:00";
    bool ok = saveAccount(makeAccount(100001, "1234", 100000, STATUS_ACTIVE))
              && saveAccount(makeAccount(100002, "5678", 50000, STATUS_CLOSED))
              && saveAccount(makeAccount(100003, "0420", 20000, STATUS_ACTIVE))
              && saveAccount(makeAccount(100004, "1111", 30000, "FROZEN"))
              && saveTransaction(txn)
              && createFirstEmployee(EMPLOYEE_USER, EMPLOYEE_PASS).success;
    seedAccounts = readBytes(ACC_FILE);
    seedTransactions = readBytes(TXN_FILE);
    seedEmployees = readBytes(EMP_FILE);
    oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    return ok;
}

// ------------------------------------------------- authenticateCustomer() itself
void testAuthenticateCustomer() {
    restoreSeed();
    std::string printed;
    check(authenticate(100001, "1234", printed) && printed.empty(),
          "Correct account number and PIN: authenticated, nothing printed");
    check(authenticate(100003, "0420", printed), "PIN with a leading zero (0420) is matched as text");

    struct Bad { const char* label; int account; std::string pin; };
    const Bad bad[] = {
        {"wrong PIN", 100001, "9999"},
        {"another account's PIN", 100001, "0420"},
        {"PIN of a different account used on that account", 100003, "1234"},
        {"leading zero dropped (420)", 100003, "420"},
        {"unknown account number", 100099, "1234"},
        {"CLOSED account with its correct PIN", 100002, "5678"},
        {"non-ACTIVE (FROZEN) account with its correct PIN", 100004, "1111"},
        {"empty PIN", 100001, ""},
        {"PIN with surrounding spaces (callers trim; the check does not)", 100001, " 1234 "},
        {"PIN with an extra digit", 100001, "12345"},
        {"PIN missing a digit", 100001, "123"},
        {"letters as PIN", 100001, "abcd"},
        {"account number 0", 0, "1234"},
        {"negative account number", -100001, "1234"},
    };
    for (const Bad& b : bad)
        check(!authenticate(b.account, b.pin, printed) && printed.empty(),
              std::string("Denied: ") + b.label + "; nothing printed");
    check(dataUnchanged(), "authenticateCustomer never saves or rewrites any file");

    restoreSeed();
    writeBytes(ACC_FILE, "BMS-ACCOUNTS 1\nnot a record\n");
    check(!authenticate(100001, "1234", printed) && readBytes(ACC_FILE) == "BMS-ACCOUNTS 1\nnot a record\n",
          "Unreadable accounts file: denied, file left as it was");

    std::error_code ec;
    fs::remove(ACC_FILE, ec);
    check(!authenticate(100001, "1234", printed) && !fs::exists(ACC_FILE),
          "No accounts file: denied, nothing created");
}

// ------------------------------------------------- the four flows, from the main menu
void testFlowsRequireAuthentication() {
    for (const Flow& f : FLOWS) {
        restoreSeed();
        std::string output = runMenu(std::string(f.menuKey) + "\n100001\n1234\n" + f.afterPin);
        check(contains(output, f.grantedText) && !contains(output, ACCESS_DENIED),
              std::string(f.name) + " from the main menu, correct account and PIN: access granted");

        struct Bad { const char* label; const char* account; const char* pin; };
        const Bad bad[] = {{"wrong PIN", "100001", "9999"},
                           {"unknown account", "100099", "1234"},
                           {"another account's PIN", "100003", "1234"},
                           {"CLOSED account", "100002", "5678"},
                           {"FROZEN account", "100004", "1111"},
                           {"empty PIN", "100001", ""}};
        for (const Bad& b : bad) {
            restoreSeed();
            output = runMenu(std::string(f.menuKey) + "\n" + b.account + "\n" + b.pin + "\n"
                             + f.afterPin);
            check(countOf(output, ACCESS_DENIED) == 1 && !contains(output, OLD_AUTH_LINE + "\n")
                      && noAccountDataShown(output) && dataUnchanged(),
                  std::string(f.name) + ", " + b.label
                      + ": one \"Access denied\" message, no data shown, no amount asked, nothing saved");
        }

        for (const char* number : {"", "abc", "0", "-5", "+100001", "100001x", "1e5", "2147483648"}) {
            restoreSeed();
            output = runMenu(std::string(f.menuKey) + "\n" + number + "\n1234\n" + f.afterPin);
            bool rejected = contains(output, ACCESS_DENIED) || contains(output, BAD_NUMBER);
            check(rejected && noAccountDataShown(output) && dataUnchanged(),
                  std::string(f.name) + ", account number \"" + number
                      + "\": rejected, no data shown, nothing saved");
        }

        restoreSeed();
        std::string partial;
        bool ok = runFlow(f.run, "100001\n", partial);   // input ends before the PIN
        check(!ok && noAccountDataShown(partial) && dataUnchanged(),
              std::string(f.name) + ", input ends before the PIN: nothing shown or saved");
    }
}

void testGrantedOperationsUseOnlyThatAccount() {
    restoreSeed();
    std::string output;
    bool ok = runFlow(depositMoney, "100003\n0420\n50\n", output);
    check(ok && storedBalance(100003) == 25000 && storedBalance(100001) == 100000,
          "Deposit after authenticating 100003 changes only 100003");

    restoreSeed();
    ok = runFlow(withdrawMoney, " 100001 \n 1234 \n100\n", output);
    check(ok && storedBalance(100001) == 90000,
          "Spaces around the account number and PIN are ignored by the flows");

    restoreSeed();
    ok = runFlow(balanceEnquiry, "100003\n0420\n", output);
    check(ok && contains(output, "account 100003: Rs. 200.00") && !contains(output, "1000.00"),
          "Balance enquiry shows only the authenticated account");
}

// ------------------------------------------------- no state carried between operations
void testNoCustomerSession() {
    restoreSeed();
    std::string output;
    bool ok = runFlow(balanceEnquiry, "100001\n1234\n", output);
    check(ok, "(setup) customer 100001 authenticated once");
    ok = runFlow(miniStatement, "100001\n9999\n", output);
    check(!ok && contains(output, ACCESS_DENIED) && noAccountDataShown(output),
          "A later operation with a wrong PIN is denied: an earlier success is not remembered");
    ok = runFlow(depositMoney, "100003\n1234\n500\n", output);
    check(!ok && contains(output, ACCESS_DENIED) && storedBalance(100003) == 20000,
          "An earlier success for 100001 gives no access to another account");

    output = runMenu("4\n100001\n1234\n4\n100001\n9999\n");
    check(countOf(output, "Current balance") == 1 && countOf(output, ACCESS_DENIED) == 1,
          "Two enquiries in one menu session: each asks for the PIN; the second, wrong, is denied");

    check(!isEmployeeAuthenticated(), "Customer authentication never logs in an employee");
    output = runMenu("4\n100001\n1234\n1\n\n\n");
    check(contains(output, "=== Employee Login ===") && !contains(output, "=== Employee Menu ==="),
          "After a customer operation, the Employee Menu still needs an employee login");
}

void testEmployeeLoginDoesNotBypassCustomerPin() {
    restoreSeed();
    std::string output;
    runFlow(loginEmployee, EMPLOYEE_USER + "\n" + EMPLOYEE_PASS + "\n", output);
    check(isEmployeeAuthenticated(), "(setup) employee logged in");
    for (const Flow& f : FLOWS) {
        bool ok = runFlow(f.run, std::string("100001\n9999\n") + f.afterPin, output);
        check(!ok && contains(output, ACCESS_DENIED) && noAccountDataShown(output),
              std::string(f.name) + " while an employee is logged in: a wrong PIN is still denied");
    }
    check(dataUnchanged(), "... and nothing was saved");
    logoutEmployee();
}

void testClosedAccountLosesAccess() {
    restoreSeed();
    std::string output;
    runFlow(loginEmployee, EMPLOYEE_USER + "\n" + EMPLOYEE_PASS + "\n", output);
    bool closed = closeAccount(100003).success;
    logoutEmployee();
    std::string printed;
    check(closed && !authenticate(100003, "0420", printed),
          "After FR-9 closes 100003, its correct PIN no longer authenticates");
    bool ok = runFlow(withdrawMoney, "100003\n0420\n50\n", output);
    check(!ok && contains(output, ACCESS_DENIED) && storedBalance(100003) == 20000,
          "... and withdrawal from it is denied");
}

} // namespace

int main() {
    std::cout << "FR-11 customer authentication tests with the REAL auth.cpp, storage.cpp, menus and\n"
                 "self-service flows (no test doubles). Data lives only in a temporary folder.\n\n";

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
    check(seeded && !isEmployeeAuthenticated(),
          "Seed data saved (4 accounts, 1 deposit, 1 employee); nobody logged in at start");
    if (seeded) {
        testAuthenticateCustomer();
        testFlowsRequireAuthentication();
        testGrantedOperationsUseOnlyThatAccount();
        testNoCustomerSession();
        testEmployeeLoginDoesNotBypassCustomerPin();
        testClosedAccountLosesAccess();
    }

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
