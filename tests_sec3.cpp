// SEC-3 tests (Test Plan TC-12, TC-13): a failed employee or customer login shows a clear error
// and performs NO operation; the Employee Menu cannot be reached without a successful login.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_sec3.exe tests_sec3.cpp menu.cpp auth.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_sec3.cpp menu.cpp auth.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_sec3.exe
//
// NO test doubles: this is the bank.exe file list with this file in place of main.cpp. Every
// menu case types its input into the REAL main menu (runMainMenu), as a user would.
// "No operation" means: no success output, both data files byte-for-byte unchanged (balances,
// account records and the transaction log), and no save at all (each file's timestamp, set to a
// day ago before the case, is untouched; every save rewrites a whole file).
// No lockout or attempt limit is tested, because the SRS does not require one.
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

#include "account.h"
#include "account_closure.h"
#include "account_search.h"
#include "account_update.h"
#include "auth.h"
#include "menu.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

// Must match EMPLOYEE_USERNAME / EMPLOYEE_PASSWORD in auth.cpp (private to that file).
const std::string USERNAME = "admin";
const std::string PASSWORD = "admin123";

const std::string TEMP_PREFIX = "bms_sec3_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";

const std::string EMPLOYEE_MENU = "=== Employee Menu ===";
const std::string LOGIN_FAILED = "Invalid username or password.";
const std::string EMPLOYEE_DENIED = "Access denied: employee login required.";
const std::string CUSTOMER_DENIED = "Access denied: invalid account number or PIN.";

// Text that appears only when an operation succeeds.
const char* const SUCCESS_TEXT[] = {"successful", "Current balance", "Recent transactions",
                                    "Account number : ", "Current customer details"};

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

bool noSuccessShown(const std::string& output) {
    for (const char* text : SUCCESS_TEXT)
        if (contains(output, text)) return false;
    return true;
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

std::string runMenu(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    runMainMenu();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    return out.str();
}

bool runFlow(bool (*flow)(), const std::string& input, std::string& output) {
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

// Logs in (or fails to) through the real menu with nothing else typed, so the login state
// auth.cpp keeps is set the way a user would set it.
std::string login(const std::string& username, const std::string& password) {
    return runMenu("1\n" + username + "\n" + password + "\n");
}

std::string accountStatus(int accountNo) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return "";
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo) return a.status;
    return "";
}

// TC-12: wrong employee credentials show a clear error and never open the Employee Menu.
void testFailedEmployeeLogin() {
    struct BadLogin { const char* label; const char* username; const char* password; };
    const BadLogin bad[] = {
        {"wrong password", "admin", "wrong"},
        {"wrong username", "manager", "admin123"},
        {"username in other case", "ADMIN", "admin123"},
        {"password in other case", "admin", "ADMIN123"},
        {"empty username and password", "", ""},
        {"username with a space", " admin", "admin123"},
    };
    for (const BadLogin& b : bad) {
        restoreSeed();
        std::string output = login(b.username, b.password);
        check(contains(output, LOGIN_FAILED) && !contains(output, EMPLOYEE_MENU)
                  && !isEmployeeAuthenticated() && dataUnchanged(),
              std::string("TC-12 employee login, ") + b.label
                  + ": clear error, Employee Menu not shown, not logged in, nothing saved");
    }

    restoreSeed();
    std::string output = runMenu("1\nadmin\n");   // input ends before the password
    check(!contains(output, EMPLOYEE_MENU) && !isEmployeeAuthenticated() && dataUnchanged(),
          "TC-12 employee login, input ends before the password: Employee Menu not shown");
}

// Keys an employee would type after a successful login are not run after a failed one.
void testAdminKeysAfterFailedLogin() {
    // Wrong password, then "4" (Close Account in the Employee Menu) and the account number.
    // After the failure these lines reach the main menu instead, where 4 is Balance Enquiry,
    // which then asks for a PIN that is never given.
    restoreSeed();
    std::string output = runMenu("1\nadmin\nwrong\n4\n100001\n");
    check(contains(output, LOGIN_FAILED) && !contains(output, EMPLOYEE_MENU)
              && !contains(output, "--- Close Account ---") && accountStatus(100001) == STATUS_ACTIVE
              && noSuccessShown(output) && dataUnchanged(),
          "TC-12 failed login then the Close Account keys: account 100001 stays ACTIVE, nothing saved");

    // Wrong password, then "1" (Create Account) and full new-account details.
    restoreSeed();
    output = runMenu("1\nadmin\nwrong\n1\nKiran Das\n4 Lake Road\n9000000009\n4321\n500\n");
    check(!contains(output, EMPLOYEE_MENU) && !contains(output, "--- Create Customer Account ---")
              && noSuccessShown(output) && dataUnchanged(),
          "TC-12 failed login then the Create Account keys: no account created, nothing saved");
}

// After a failed login every protected employee operation refuses to run, even when called
// directly instead of through the menu.
void testEmployeeOperationsDeniedAfterFailedLogin() {
    restoreSeed();
    login(USERNAME, "wrong");

    CreateAccountResult created = createAccountWithDetails("Kiran Das", "4 Lake Road",
                                                           "9000000009", "4321", 50000);
    check(!created.success && created.message == EMPLOYEE_DENIED && dataUnchanged(),
          "SEC-3 createAccountWithDetails after a failed login: access denied, nothing saved");

    SearchAccountResult found = searchAccount(100001);
    check(!found.success && found.message == EMPLOYEE_DENIED && found.account.accountNumber == 0,
          "SEC-3 searchAccount after a failed login: access denied, no account data returned");

    UpdateAccountResult updated = updateAccountDetails(100001, "Asha Changed", "9 New Road",
                                                       "9000000008");
    check(!updated.success && updated.message == EMPLOYEE_DENIED && dataUnchanged(),
          "SEC-3 updateAccountDetails after a failed login: access denied, nothing saved");

    AccountClosureResult closed = closeAccount(100001);
    check(!closed.success && closed.message == EMPLOYEE_DENIED
              && accountStatus(100001) == STATUS_ACTIVE && dataUnchanged(),
          "SEC-3 closeAccount after a failed login: access denied, account stays ACTIVE");

    struct Screen { const char* name; bool (*flow)(); const char* input; };
    const Screen screens[] = {
        {"Create Account", createAccount, "Kiran Das\n4 Lake Road\n9000000009\n4321\n500\n"},
        {"Search Account", accountSearch, "100001\n"},
        {"Update Account", accountUpdate, "100001\nAsha Changed\n9 New Road\n9000000008\n"},
        {"Close Account", accountClosure, "100001\n"},
    };
    for (const Screen& s : screens) {
        std::string output;
        bool ok = runFlow(s.flow, s.input, output);
        check(!ok && contains(output, EMPLOYEE_DENIED) && !contains(output, "--- ")
                  && noSuccessShown(output) && dataUnchanged(),
              std::string("SEC-3 ") + s.name
                  + " screen after a failed login: access denied before any prompt, nothing saved");
    }
}

// TC-12 positive case, and a failed login after a successful one ends the employee session.
void testSuccessfulThenFailedLogin() {
    restoreSeed();
    std::string output = login(USERNAME, PASSWORD);
    check(contains(output, "Employee login successful.") && contains(output, EMPLOYEE_MENU)
              && isEmployeeAuthenticated() && dataUnchanged(),
          "TC-12 correct employee credentials: Employee Menu shown");

    output = login(USERNAME, "wrong");
    check(contains(output, LOGIN_FAILED) && !contains(output, EMPLOYEE_MENU)
              && !isEmployeeAuthenticated(),
          "SEC-3 a failed login after a successful one logs the employee out");
    check(!closeAccount(100001).success && accountStatus(100001) == STATUS_ACTIVE,
          "SEC-3 ... and protected operations are denied again");
}

// No lockout: several failed attempts do not block a later correct login (SRS has no limit).
void testNoLockout() {
    restoreSeed();
    for (int i = 0; i < 5; ++i) login(USERNAME, "wrong" + std::to_string(i));
    std::string output = login(USERNAME, PASSWORD);
    check(contains(output, EMPLOYEE_MENU) && isEmployeeAuthenticated(),
          "No lockout: after 5 failed attempts the correct credentials still work");
    login(USERNAME, "wrong");   // leave the session logged out for the next tests
}

// TC-13: a failed customer login shows a clear error and changes no balance, record or log.
void testFailedCustomerLogin() {
    struct Operation { const char* choice; const char* name; const char* amount; };
    const Operation operations[] = {{"2", "Deposit", "500\n"}, {"3", "Withdrawal", "200\n"},
                                    {"4", "Balance enquiry", ""}, {"5", "Mini-statement", ""}};
    struct BadLogin { const char* label; const char* account; const char* pin; };
    const BadLogin bad[] = {{"wrong PIN", "100001", "9999"},
                            {"unknown account", "100099", "1234"},
                            {"CLOSED account", "100002", "5678"}};
    for (const Operation& op : operations) {
        for (const BadLogin& b : bad) {
            restoreSeed();
            std::string output = runMenu(std::string(op.choice) + "\n" + b.account + "\n" + b.pin
                                         + "\n" + op.amount);
            check(contains(output, CUSTOMER_DENIED) && noSuccessShown(output)
                      && !contains(output, "amount (Rs.)") && dataUnchanged(),
                  std::string("TC-13 ") + op.name + " from the main menu, " + b.label
                      + ": clear error; balance, records and transaction log unchanged; no save");
        }
    }
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

// 100001 ACTIVE (PIN 1234, Rs. 1000), 100002 CLOSED (PIN 5678), one deposit, saved with the
// real storage.
bool createSeedData() {
    Transaction txn;
    txn.transactionId = 1;
    txn.accountNumber = 100001;
    txn.type = TXN_DEPOSIT;
    txn.amount = 10000;
    txn.dateTime = "2026-10-30 10:00:00";
    bool ok = saveAccount(makeAccount(100001, "1234", 100000, STATUS_ACTIVE))
              && saveAccount(makeAccount(100002, "5678", 50000, STATUS_CLOSED))
              && saveTransaction(txn);
    seedAccounts = readBytes(ACC_FILE);
    seedTransactions = readBytes(TXN_FILE);
    oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    return ok;
}

} // namespace

int main() {
    std::cout << "SEC-3 failed-authentication tests through the REAL menus, auth.cpp, features and\n"
                 "storage (no test doubles). Data lives only in a temporary folder.\n\n";

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
          "Seed data saved (100001 ACTIVE, 100002 CLOSED, 1 deposit); nobody logged in at start");
    if (seeded) {
        testFailedEmployeeLogin();
        testAdminKeysAfterFailedLogin();
        testEmployeeOperationsDeniedAfterFailedLogin();
        testSuccessfulThenFailedLogin();
        testNoLockout();
        testFailedCustomerLogin();
    }

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
