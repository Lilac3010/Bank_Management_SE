// NFR-3 invalid-input tests (Test Plan TC-14): invalid input is rejected with a clear error and
// never changes the stored accounts or transactions.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr3.exe tests_nfr3.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_nfr3.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr3.exe
//
// Every case types its input into the REAL main menu (menu.cpp), so it goes through the same
// menus and console flows as bank.exe, with the REAL storage.cpp. Only login is faked
// (auth.cpp does not exist yet). Each case starts from the same saved data and then checks:
//   - the expected error message was shown and no success message was shown;
//   - data/accounts.txt and data/transactions.txt are byte-for-byte unchanged;
//   - no save happened: every save rewrites a whole file, so each file's timestamp (set to a
//     day ago before the case) must be unchanged, and no ".tmp" file may be left;
//   - the menu returned within 5 seconds (no endless loop) without throwing.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+.
// Data lives only in a temporary folder that this program creates and deletes.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "account.h"
#include "auth.h"
#include "menu.h"
#include "storage.h"
#include "transactions.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for auth.cpp (FR-10 / FR-11), which does not exist yet.
// Defined only in this file; never linked into main.

// Employee login always succeeds, so every Employee Menu flow can be reached.
bool loginEmployee() { return true; }
bool isEmployeeAuthenticated() { return true; }

// Accepts the stored PIN of a stored ACTIVE account, read through the real storage.
bool authenticateCustomer(int accountNo, const std::string& pin) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return false;
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo) return a.pin == pin && a.status == STATUS_ACTIVE;
    return false;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_nfr3_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const std::chrono::seconds MENU_TIME_LIMIT(5);

// Messages each flow shows for invalid input.
const std::string INVALID_CHOICE = "Invalid choice";
const std::string INVALID_ACCOUNT_NUMBER = "Error: Account number must be a positive whole number.";
const std::string ACCESS_DENIED = "Access denied: invalid account number or PIN.";
const std::string UPDATE_CLOSE_INVALID_NUMBER = "Invalid account number.";
const std::string NO_MESSAGE_NEEDED = "";   // end of input: nobody is left to read a message

// Text that appears only when an operation succeeds.
const char* const SUCCESS_TEXT[] = {"successful", "Current balance", "Recent transactions",
                                    "Account number : "};

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

// True when both files are byte-for-byte as saved, neither was rewritten, and no temp file
// exists (data/ holds exactly the two data files).
bool dataUnchanged() {
    std::error_code ec;
    int files = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator("data", ec)) {
        (void)entry;
        ++files;
    }
    return !ec && files == 2 && readBytes(ACC_FILE) == seedAccounts
           && readBytes(TXN_FILE) == seedTransactions
           && fs::last_write_time(ACC_FILE, ec) == oldTime
           && fs::last_write_time(TXN_FILE, ec) == oldTime;
}

// Runs the whole menu with `input` as the keyboard and returns what it printed. Stops the
// test program if the menu does not return in time, because the menu thread cannot be stopped.
std::string runMenu(const std::string& input, const std::string& label, bool& threw) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::future<void> done = std::async(std::launch::async, runMainMenu);
    bool finished = done.wait_for(MENU_TIME_LIMIT) == std::future_status::ready;
    if (!finished) {
        // The menu thread is still writing to std::cout (redirected to `out`), so leave it
        // there and report through C stdio after flushing what was already printed.
        oldOut->pubsync();
        std::string message = "[FAIL] " + label + ": menu did not return within 5 seconds"
                              " (endless loop)\n";
        std::fputs(message.c_str(), stdout);
        std::fflush(stdout);
        std::_Exit(1);
    }
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    threw = false;
    try {
        done.get();
    } catch (...) {
        threw = true;
    }
    return out.str();
}

// One invalid-input case: start from the saved data, type `input`, check the result.
void expectRejected(const std::string& label, const std::string& input,
                    const std::string& expectedMessage) {
    restoreSeed();
    bool threw = false;
    std::string output = runMenu(input, label, threw);

    bool noSuccess = true;
    for (const char* text : SUCCESS_TEXT)
        if (contains(output, text)) noSuccess = false;

    bool ok = !threw && contains(output, expectedMessage) && noSuccess && dataUnchanged();
    std::string expectation = expectedMessage.empty() ? "returns" : "error shown";
    check(ok, label + ": " + expectation + "; stored data unchanged; nothing saved");
    if (!ok) {
        std::cout << "      threw=" << threw << " expected message found="
                  << contains(output, expectedMessage) << " no success text=" << noSuccess
                  << " data unchanged=" << dataUnchanged() << "\n";
    }
}

std::string quoted(const std::string& text) {
    return "\"" + text + "\"";
}

// Account-number text that update and close must reject.
const char* const BAD_ACCOUNT_NUMBERS[] = {"abc", "", "100001abc", " 100001 x", "99999999999",
                                           "-5", "0", "1e5", "100 001"};

// Amount text that deposit and withdrawal must reject.
const char* const BAD_AMOUNTS[] = {"abc", "12a", "", "1,000", "500rs", "Rs. 500"};

void testMainMenu() {
    for (const char* choice : {"abc", "", "9", "-1", "1.5", " 2x", "zero"})
        expectRejected(std::string("Main menu choice ") + quoted(choice),
                       std::string(choice) + "\n", INVALID_CHOICE);
    expectRejected("Main menu: end of input", "", NO_MESSAGE_NEEDED);
}

void testEmployeeMenu() {
    for (const char* choice : {"abc", "", "7", "-2", "1x"})
        expectRejected(std::string("Employee Menu choice ") + quoted(choice),
                       std::string("1\n") + choice + "\n", INVALID_CHOICE);
    expectRejected("Employee Menu: end of input", "1\n", NO_MESSAGE_NEEDED);
}

void testCreateAccount() {
    const std::string start = "1\n1\n";   // Employee Login, then Create Account
    const std::string details = "Kiran Das\n4 Lake Road, Udupi\n9000000001\n4321\n";
    for (const char* amount : BAD_AMOUNTS)
        expectRejected("Create account: initial deposit " + quoted(amount),
                       start + details + amount + "\n", "Error: Initial deposit must be");
    expectRejected("Create account: every field an empty line", start + "\n\n\n\n\n", "Error");
    expectRejected("Create account: name of only spaces",
                   start + "   \n4 Lake Road, Udupi\n9000000001\n4321\n100\n",
                   "Error: Name cannot be empty");
    expectRejected("Create account: letters in the contact number",
                   start + "Kiran Das\n4 Lake Road, Udupi\n90000abc01\n4321\n100\n",
                   "Error: Contact number must be exactly 10 digits");
    expectRejected("Create account: end of input", start, "Error");
}

void testSearch() {
    const std::string start = "1\n2\n";   // Employee Login, then Search Account
    for (const char* number : {"abc", "", "100001abc", " 100001 x", "99999999999"})
        expectRejected("Search: account number " + quoted(number),
                       start + number + "\n", INVALID_ACCOUNT_NUMBER);
    expectRejected("Search: end of input", start, NO_MESSAGE_NEEDED);
}

void testUpdate() {
    const std::string start = "1\n3\n";   // Employee Login, then Update Account
    // Valid new details follow, so a wrongly accepted number would really change the record.
    const std::string newDetails = "Asha Changed\n9 New Road, Hubballi\n9000000009\n";
    for (const char* number : BAD_ACCOUNT_NUMBERS)
        expectRejected("Update: account number " + quoted(number),
                       start + number + "\n" + newDetails, UPDATE_CLOSE_INVALID_NUMBER);
    expectRejected("Update: end of input before the account number", start,
                   UPDATE_CLOSE_INVALID_NUMBER);

    const std::string account = start + "100001\n";
    expectRejected("Update: name of only spaces",
                   account + "   \n9 New Road, Hubballi\n9000000009\n", "Invalid name");
    expectRejected("Update: name with no letters",
                   account + "...\n9 New Road, Hubballi\n9000000009\n", "Invalid name");
    expectRejected("Update: name with digits",
                   account + "Asha 2\n9 New Road, Hubballi\n9000000009\n", "Invalid name");
    expectRejected("Update: address of only spaces",
                   account + "Asha Changed\n   \n9000000009\n", "Invalid address");
    expectRejected("Update: letters in the contact number",
                   account + "Asha Changed\n9 New Road, Hubballi\n90000abc09\n",
                   "Contact number must contain exactly 10 digits");
    expectRejected("Update: every new detail an empty line", account + "\n\n\n", "Invalid name");
    expectRejected("Update: end of input after the account number", account, "Invalid name");
}

void testClose() {
    const std::string start = "1\n4\n";   // Employee Login, then Close Account
    for (const char* number : BAD_ACCOUNT_NUMBERS)
        expectRejected("Close: account number " + quoted(number),
                       start + number + "\n", UPDATE_CLOSE_INVALID_NUMBER);
    expectRejected("Close: end of input", start, UPDATE_CLOSE_INVALID_NUMBER);
}

// Deposit (main menu 2) and withdrawal (main menu 3) share the same input rules.
void testMoneyFlow(const std::string& name, const std::string& choice,
                   const std::string& amountError) {
    const std::string start = choice + "\n";
    for (const char* number : {"abc", "", "100001abc", " 100001 x"})
        expectRejected(name + ": account number " + quoted(number),
                       start + number + "\n1234\n500\n", ACCESS_DENIED);
    for (const char* amount : BAD_AMOUNTS)
        expectRejected(name + ": amount " + quoted(amount),
                       start + "100001\n1234\n" + amount + "\n", amountError);
    expectRejected(name + ": end of input before the account number", start, ACCESS_DENIED);
    expectRejected(name + ": end of input before the amount", start + "100001\n1234\n",
                   amountError);
}

void testBalanceAndStatement() {
    for (const char* number : {"abc", "", "100001abc", " 100001 x"}) {
        expectRejected("Balance enquiry: account number " + quoted(number),
                       std::string("4\n") + number + "\n1234\n", INVALID_ACCOUNT_NUMBER);
        expectRejected("Mini-statement: account number " + quoted(number),
                       std::string("5\n") + number + "\n1234\n", INVALID_ACCOUNT_NUMBER);
    }
    expectRejected("Balance enquiry: end of input", "4\n", INVALID_ACCOUNT_NUMBER);
    expectRejected("Mini-statement: end of input", "5\n", NO_MESSAGE_NEEDED);
}

// Saves two accounts and one transaction with the real FR-1 / FR-3 code and remembers the files.
bool createSeedData() {
    CreateAccountResult first = createAccountWithDetails("Asha Rao", "12 MG Road, Bengaluru",
                                                         "9876543210", "1234", 100000);
    CreateAccountResult second = createAccountWithDetails("Ravi Kumar", "7 Park Street, Mysuru",
                                                          "9123456780", "5678", 50000);
    DepositResult dep = deposit(100001, 50000);
    if (!first.success || first.accountNumber != 100001 || !second.success || !dep.success)
        return false;
    seedAccounts = readBytes(ACC_FILE);
    seedTransactions = readBytes(TXN_FILE);
    oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    return true;
}

} // namespace

int main() {
    std::cout << "NFR-3 invalid-input tests through the REAL menus and console flows with the REAL\n"
                 "storage.cpp. Login is a test double. Data lives only in a temporary folder.\n\n";

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
    check(seeded, "Seed data saved: accounts 100001 and 100002, one deposit");
    if (seeded) {
        restoreSeed();
        check(dataUnchanged(), "Check works: restored data counts as unchanged");
        Account changed;
        changed.accountNumber = 100002;
        changed.name = "Ravi Kumar";
        changed.address = "7 Park Street, Mysuru";
        changed.contactNumber = "9123456780";
        changed.pin = "5678";
        changed.balance = 50000;
        saveAccount(changed);   // identical record: same bytes, but the file is rewritten
        check(!dataUnchanged(), "Check works: a save of identical data is still detected");

        testMainMenu();
        testEmployeeMenu();
        testCreateAccount();
        testSearch();
        testUpdate();
        testClose();
        testMoneyFlow("Deposit", "2", "Error: Deposit amount must be");
        testMoneyFlow("Withdrawal", "3", "Error: Withdrawal amount must be");
        testBalanceAndStatement();
    }

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
