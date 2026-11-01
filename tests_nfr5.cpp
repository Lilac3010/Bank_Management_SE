// NFR-5 whole-program link and smoke test, using the REAL code of every module bank.exe needs.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr5.exe tests_nfr5.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_nfr5.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr5.exe
//
// This is the bank.exe file list without main.cpp (this file has its own main(); main.cpp only
// calls runMainMenu() in menu.cpp) and without
// auth.cpp (FR-10 / FR-11, not written yet). If this program links, bank.exe will link once
// auth.cpp provides the three functions faked below.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+.
// Data lives only in a temporary folder that this program creates and deletes.
#include <chrono>
#include <filesystem>
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

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for auth.cpp (FR-10 / FR-11), the only bank.exe module that does not exist yet.
// Defined only in this file; never linked into main.
namespace fake {
bool employeeLoggedIn = true;
}

bool loginEmployee() { return fake::employeeLoggedIn; }

bool isEmployeeAuthenticated() { return fake::employeeLoggedIn; }

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

const std::string TEMP_PREFIX = "bms_nfr5_test_";

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
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

// runMainMenu (called by main.cpp) and every function the menus call. Taking their addresses
// makes this program fail to link if any module bank.exe needs (other than auth.cpp) is
// missing or defines one of them twice.
void testEveryMenuFunctionLinks() {
    using ConsoleFlow = bool (*)();
    const ConsoleFlow flows[] = {loginEmployee, createAccount, accountSearch, accountUpdate,
                                 accountClosure, depositMoney, withdrawMoney, balanceEnquiry,
                                 miniStatement};
    void (*const mainMenu)() = runMainMenu;
    bool allLinked = mainMenu != nullptr;
    for (ConsoleFlow flow : flows)
        if (flow == nullptr) allLinked = false;
    check(allLinked, "NFR-5 runMainMenu and all 9 functions the menus call are linked");
}

// One pass through FR-1 to FR-9 with the real modules, then FR-13 checks what reached disk.
void testEndToEndWithRealStorage() {
    CreateAccountResult created = createAccountWithDetails("Asha Rao", "12 MG Road, Bengaluru",
                                                           "9876543210", "1234", 100000);
    check(created.success && created.accountNumber == 100001,
          "FR-1/FR-2 account created as 100001 with Rs. 1000.00");
    const int no = created.accountNumber;

    DepositResult dep = deposit(no, 50000);
    check(dep.success && dep.newBalance == 150000, "FR-3 deposit Rs. 500.00 gives Rs. 1500.00");

    WithdrawalResult wd = withdraw(no, 20000);
    check(wd.success && wd.newBalance == 130000 && wd.transactionId == 2,
          "FR-4 withdrawal Rs. 200.00 gives Rs. 1300.00 (transaction 2)");

    std::string output;
    bool ok = runConsole(balanceEnquiry, "100001\n1234\n", output);
    check(ok && contains(output, "Rs. 1300.00"), "FR-5 balance enquiry shows Rs. 1300.00");

    ok = runConsole(miniStatement, "100001\n1234\n", output);
    check(ok && contains(output, "DEPOSIT") && contains(output, "WITHDRAW"),
          "FR-6 mini-statement lists the deposit and the withdrawal");

    SearchAccountResult found = searchAccount(no);
    check(found.success && found.account.balance == 130000 && found.account.pin.empty(),
          "FR-7 search finds the account without its PIN");

    UpdateAccountResult updated = updateAccountDetails(no, "Asha R. Rao", "7 Park Street, Mysuru",
                                                       "9123456780");
    check(updated.success, "FR-8 customer details updated");

    AccountClosureResult closed = closeAccount(no);
    check(closed.success, "FR-9 account closed");
    check(!deposit(no, 100).success, "FR-9 closed account rejects a deposit");

    std::vector<Account> accounts;
    std::vector<Transaction> txns;
    bool loaded = loadAccounts(accounts) && loadTransactions(txns);
    check(loaded && accounts.size() == 1 && accounts[0].name == "Asha R. Rao"
              && accounts[0].contactNumber == "9123456780" && accounts[0].balance == 130000
              && accounts[0].status == STATUS_CLOSED && txns.size() == 2,
          "FR-13 files on disk hold the final state (1 CLOSED account, 2 transactions)");
}

} // namespace

int main() {
    std::cout << "NFR-5 link and smoke tests with the REAL code of every bank.exe module except\n"
                 "main.cpp and auth.cpp. Login is a test double. Data lives only in a temporary folder.\n\n";

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

    testEveryMenuFunctionLinks();
    testEndToEndWithRealStorage();

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
