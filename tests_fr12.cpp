// FR-12 error-handling tests (SRS FR-12; Test Plan TC-02, TC-04, TC-06, TC-14).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr12.exe tests_fr12.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp -lcrypto
//    or: cl /std:c++17 /EHsc /W4 tests_fr12.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_fr12.exe
//
// NO test doubles: the real menus, features, login and storage, so this must never be linked
// with a file that fakes any of them. Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+;
// on Linux also libssl-dev (auth.cpp links password_hash.cpp).
// Data lives only in a temporary folder that this program creates and deletes.
//
// FR-12 names five kinds of failure. For each, the operation must be rejected with an
// understandable message and must change nothing: every data file byte-for-byte unchanged and
// not even rewritten (timestamps set to a day ago must stay), and no .tmp file left behind.
// Deeper input cases are in tests_nfr3.cpp (TC-14), credentials in tests_fr10.cpp /
// tests_fr11.cpp / tests_sec3.cpp, amounts in tests_sec4.cpp; this suite covers each category
// once through the real program.
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

#include "account.h"
#include "account_closure.h"
#include "account_search.h"
#include "account_update.h"
#include "auth.h"
#include "balance_enquiry.h"
#include "menu.h"
#include "mini_statement.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_fr12_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const fs::path EMP_FILE = fs::path("data") / "employees.txt";

const std::string EMPLOYEE_USER = "staff.one";
const std::string EMPLOYEE_PASS = "Staff-Pass-12";
const std::string CUSTOMER_DENIED = "Access denied: invalid account number or PIN.";
const std::string NOT_FOUND = "Error: Account not found.";
const std::string BAD_NUMBER = "Error: Account number must be a positive whole number.";

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

// Puts back the seed data, dated a day ago. Leaves the employee session as it is.
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
}

// "Without unintended changes": nothing rewritten, nothing left half-written.
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

void employeeLogin() {
    std::string output;
    runFlow(loginEmployee, EMPLOYEE_USER + "\n" + EMPLOYEE_PASS + "\n", output);
}

Account makeAccount(int number, const std::string& contact, const std::string& pin,
                    long long balance, const std::string& status) {
    Account a;
    a.accountNumber = number;
    a.name = "Customer " + std::string(1, static_cast<char>('A' + number % 26));
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = contact;
    a.pin = pin;
    a.balance = balance;
    a.status = status;
    return a;
}

// 100001 ACTIVE 9876543210 PIN 1234 Rs. 1000.00; 100002 CLOSED 9000000002 PIN 5678 Rs. 500.00;
// 100003 ACTIVE 9000000003 PIN 4321 Rs. 200.00; one deposit; one employee.
bool createSeedData() {
    Transaction txn;
    txn.transactionId = 1;
    txn.accountNumber = 100001;
    txn.type = TXN_DEPOSIT;
    txn.amount = 10000;
    txn.dateTime = "2026-11-03 10:00:00";
    bool ok = saveAccount(makeAccount(100001, "9876543210", "1234", 100000, STATUS_ACTIVE))
              && saveAccount(makeAccount(100002, "9000000002", "5678", 50000, STATUS_CLOSED))
              && saveAccount(makeAccount(100003, "9000000003", "4321", 20000, STATUS_ACTIVE))
              && saveTransaction(txn)
              && createFirstEmployee(EMPLOYEE_USER, EMPLOYEE_PASS).success;
    seedAccounts = readBytes(ACC_FILE);
    seedTransactions = readBytes(TXN_FILE);
    seedEmployees = readBytes(EMP_FILE);
    oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    return ok;
}

// ------------------------------------------------------------------ 1. invalid input
void testInvalidInput() {
    employeeLogin();
    struct Create { const char* label; const char* name; const char* contact; const char* pin;
                    const char* deposit; const char* message; };
    const Create creates[] = {
        {"empty name", "", "9111111111", "1111", "500", "Error: Name cannot be empty."},
        {"name with digits", "R2D2", "9111111111", "1111", "500", "Error: Name may contain only"},
        {"contact of 9 digits", "Kiran Das", "911111111", "1111", "500", "Error: Contact number must be exactly 10 digits."},
        {"contact with letters", "Kiran Das", "91111abcde", "1111", "500", "Error: Contact number must be exactly 10 digits."},
        {"PIN of 3 digits", "Kiran Das", "9111111111", "111", "500", "Error: PIN must be exactly 4 digits."},
        {"PIN with a letter", "Kiran Das", "9111111111", "11a1", "500", "Error: PIN must be exactly 4 digits."},
        {"deposit 0", "Kiran Das", "9111111111", "1111", "0", "Error: Initial deposit must be a positive amount."},
        {"deposit with letters", "Kiran Das", "9111111111", "1111", "abc", "Error: Initial deposit must be an amount like"},
        {"negative deposit", "Kiran Das", "9111111111", "1111", "-500", "Error: Initial deposit must be an amount like"},
        {"deposit in exponent form", "Kiran Das", "9111111111", "1111", "1e3", "Error: Initial deposit must be an amount like"},
        {"deposit with 3 decimals", "Kiran Das", "9111111111", "1111", "10.555", "Error: Initial deposit must be an amount like"},
    };
    for (const Create& c : creates) {
        restoreSeed();
        std::string output;
        bool ok = runFlow(createAccount,
                          std::string(c.name) + "\n4 Lake Road\n" + c.contact + "\n" + c.pin + "\n"
                              + c.deposit + "\n",
                          output);
        check(!ok && contains(output, c.message) && !contains(output, "created successfully")
                  && dataUnchanged(),
              std::string("Invalid input, create account with ") + c.label
                  + ": clear error, no account created, nothing changed");
    }

    for (const char* amount : {"abc", "", "-100", "0", "0.00", "1e3", "1,000", "10.555", "nan"}) {
        restoreSeed();
        std::string output;
        bool deposited = runFlow(depositMoney, std::string("100001\n1234\n") + amount + "\n", output);
        bool depositOk = !deposited && contains(output, "Error: Deposit amount must be")
                         && dataUnchanged();
        restoreSeed();
        bool withdrawn = runFlow(withdrawMoney, std::string("100001\n1234\n") + amount + "\n", output);
        check(depositOk && !withdrawn && contains(output, "Error: Withdrawal amount must be")
                  && dataUnchanged(),
              std::string("Invalid input, amount \"") + amount
                  + "\": deposit and withdrawal rejected with an error, balance and history unchanged");
    }

    struct Screen { const char* name; bool (*flow)(); };
    const Screen screens[] = {{"Search", accountSearch}, {"Update", accountUpdate},
                              {"Close", accountClosure}, {"Balance enquiry", balanceEnquiry},
                              {"Mini-statement", miniStatement}};
    for (const Screen& s : screens) {
        for (const char* number : {"abc", "0", "-5", "100001x", "2147483648"}) {
            restoreSeed();
            std::string output;
            bool ok = runFlow(s.flow, std::string(number) + "\n", output);
            check(!ok && contains(output, BAD_NUMBER) && dataUnchanged(),
                  std::string("Invalid input, ") + s.name + " with account number \"" + number
                      + "\": \"" + BAD_NUMBER + "\", nothing changed");
        }
    }

    restoreSeed();
    std::string output;
    bool ok = runFlow(accountUpdate, "100003\nR2D2\n4 Lake Road\n9000000003\n", output);
    check(!ok && contains(output, "Error: Invalid name.") && dataUnchanged(),
          "Invalid input, update with a bad name: clear error, record unchanged");

    restoreSeed();
    output = runMenu("9\nabc\n0\n");
    check(contains(output, "Invalid choice. Please try again.") && dataUnchanged(),
          "Invalid input, main-menu choices 9 and abc: \"Invalid choice\", nothing run");
    logoutEmployee();
}

// ------------------------------------------------------------------ 2. invalid credentials
void testInvalidCredentials() {
    restoreSeed();
    std::string output = runMenu("1\n" + EMPLOYEE_USER + "\nwrong-password\n");
    check(contains(output, "Invalid username or password.") && !contains(output, "=== Employee Menu ===")
              && !isEmployeeAuthenticated() && dataUnchanged(),
          "Invalid credentials, employee login: clear error, Employee Menu not shown, nothing changed");

    struct Customer { const char* name; const char* key; const char* afterPin; };
    const Customer flows[] = {{"Deposit", "2", "500\n"}, {"Withdrawal", "3", "200\n"},
                              {"Balance enquiry", "4", ""}, {"Mini-statement", "5", ""}};
    for (const Customer& f : flows) {
        restoreSeed();
        output = runMenu(std::string(f.key) + "\n100001\n9999\n" + f.afterPin);
        check(contains(output, CUSTOMER_DENIED) && !contains(output, "successful")
                  && !contains(output, "Current balance") && !contains(output, "Recent transactions")
                  && dataUnchanged(),
              std::string("Invalid credentials, ") + f.name
                  + " with a wrong PIN: \"Access denied\", nothing shown or changed");
    }
}

// ------------------------------------------------------------------ 3. insufficient balance
void testInsufficientBalance() {
    restoreSeed();
    std::string output;
    bool ok = runFlow(withdrawMoney, "100001\n1234\n1000.01\n", output);
    check(!ok && contains(output, "Error: Insufficient balance. Withdrawal not made.") && dataUnchanged(),
          "Insufficient balance, withdraw Rs. 1000.01 from Rs. 1000.00: clear error, nothing changed");

    restoreSeed();
    ok = runFlow(withdrawMoney, "100003\n4321\n1500\n", output);
    check(!ok && contains(output, "Insufficient balance") && dataUnchanged(),
          "Insufficient balance, TC-06 (Rs. 1500 from Rs. 200): rejected, nothing changed");

    restoreSeed();
    WithdrawalResult r = withdraw(100001, 100001);
    check(!r.success && contains(r.message, "Insufficient balance") && r.newBalance == 0
              && r.transactionId == 0 && dataUnchanged(),
          "Insufficient balance, withdraw() directly: rejected, no balance or transaction ID returned");

    restoreSeed();
    ok = runFlow(withdrawMoney, "100001\n1234\n1000\n", output);
    std::vector<Account> accounts;
    loadAccounts(accounts);
    check(ok && contains(output, "Withdrawal successful") && accounts[0].balance == 0,
          "Control: withdrawing exactly the whole balance is allowed");
}

// ------------------------------------------------------------------ 4. duplicate account data
void testDuplicateAccountData() {
    employeeLogin();
    for (const char* contact : {"9876543210", " 9876543210 "}) {
        restoreSeed();
        CreateAccountResult r = createAccountWithDetails("Kiran Das", "4 Lake Road", contact, "1111", 50000);
        check(!r.success && r.accountNumber == 0
                  && r.message == "Error: An active account with this contact number already exists."
                  && dataUnchanged(),
              std::string("Duplicate account data, create with the contact of an ACTIVE account (\"")
                  + contact + "\"): rejected, no account created, nothing changed");
    }

    restoreSeed();
    std::string output;
    bool ok = runFlow(createAccount, "Kiran Das\n4 Lake Road\n9876543210\n1111\n500\n", output);
    check(!ok && contains(output, "already exists") && dataUnchanged(),
          "Duplicate account data, Create Account screen: clear error, nothing changed");

    restoreSeed();
    UpdateAccountResult u = updateAccountDetails(100003, "Customer D", "4 Lake Road", "9876543210");
    check(!u.success && u.message == "Error: Contact number is already used by another active account."
              && dataUnchanged(),
          "Duplicate account data, update 100003 to the contact of ACTIVE 100001: rejected, unchanged");

    restoreSeed();
    CreateAccountResult r = createAccountWithDetails("Kiran Das", "4 Lake Road", "9000000002", "1111", 50000);
    check(r.success && r.accountNumber > 100003,
          "Control: the contact of a CLOSED account may be used again (FR-1 rule)");
    logoutEmployee();
}

// ------------------------------------------------------------------ 5. non-existent accounts
void testNonExistentAccounts() {
    employeeLogin();
    restoreSeed();
    SearchAccountResult s = searchAccount(999999);
    UpdateAccountResult u = updateAccountDetails(999999, "Kiran Das", "4 Lake Road", "9111111111");
    AccountClosureResult c = closeAccount(999999);
    check(!s.success && s.message == NOT_FOUND && s.account.accountNumber == 0,
          "Non-existent account, searchAccount(999999): \"Error: Account not found.\", no data");
    check(!u.success && u.message == NOT_FOUND, "Non-existent account, updateAccountDetails: not found");
    check(!c.success && c.message == NOT_FOUND, "Non-existent account, closeAccount: not found");
    check(dataUnchanged(), "... and none of them changed anything");

    struct Screen { const char* name; bool (*flow)(); };
    const Screen screens[] = {{"Search", accountSearch}, {"Update", accountUpdate},
                              {"Close", accountClosure}};
    for (const Screen& sc : screens) {
        restoreSeed();
        std::string output;
        bool ok = runFlow(sc.flow, "999999\nKiran Das\n4 Lake Road\n9111111111\n", output);
        check(!ok && contains(output, NOT_FOUND) && dataUnchanged(),
              std::string("Non-existent account, ") + sc.name
                  + " screen: \"Error: Account not found.\", nothing changed");
    }
    logoutEmployee();

    restoreSeed();
    DepositResult d = deposit(999999, 50000);
    WithdrawalResult w = withdraw(999999, 50000);
    check(!d.success && d.message == NOT_FOUND && !w.success && w.message == NOT_FOUND && dataUnchanged(),
          "Non-existent account, deposit() / withdraw() directly: \"Error: Account not found.\", nothing changed");

    // At the customer screens an unknown account is denied like a wrong PIN, so the message
    // does not reveal which account numbers exist (FR-11).
    for (bool (*flow)() : {depositMoney, withdrawMoney, balanceEnquiry, miniStatement}) {
        restoreSeed();
        std::string output;
        bool ok = runFlow(flow, "999999\n1234\n500\n", output);
        check(!ok && contains(output, CUSTOMER_DENIED) && dataUnchanged(),
              "Non-existent account at a customer screen: \"Access denied\", nothing changed");
    }

    restoreSeed();
    d = deposit(100002, 50000);
    check(!d.success && d.message == "Error: Account is closed." && dataUnchanged(),
          "Closed account, deposit(): \"Error: Account is closed.\", nothing changed");
}

} // namespace

int main() {
    std::cout << "FR-12 error-handling tests with the REAL menus, features, login and storage\n"
                 "(no test doubles). Data lives only in a temporary folder.\n\n";

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
    check(seeded, "Seed data saved (3 accounts, 1 deposit, 1 employee)");
    if (seeded) {
        testInvalidInput();
        testInvalidCredentials();
        testInsufficientBalance();
        testDuplicateAccountData();
        testNonExistentAccounts();
    }

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
