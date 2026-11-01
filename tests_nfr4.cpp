// NFR-4 modular-structure tests (Test Plan TC-18, module part; SRS RTM: code review).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr4.exe tests_nfr4.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_nfr4.cpp /Fe:tests_nfr4.exe
// Run it from the repository root (build_and_test.ps1 does), because it reads the source files.
//
// Build ONLY this file. It links no module: it only includes the headers.
//   1. Interfaces: every public module function has the signature the code relies on, checked
//      at compile time with static_assert (nothing is called, so nothing needs to be linked).
//   2. Module map: each major feature has its own header and source file, which declare and
//      define its functions.
//   3. Dependencies: the #include "..." lines of every production .h / .cpp follow the SAD
//      layers (Presentation -> Service/Business -> Persistence). Feature modules never include
//      another feature, so each one can be changed on its own (SRS NFR-4).
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "account.h"
#include "account_closure.h"
#include "account_search.h"
#include "account_update.h"
#include "auth.h"
#include "balance_enquiry.h"
#include "menu.h"
#include "mini_statement.h"
#include "models.h"
#include "password_hash.h"
#include "storage.h"
#include "transactions.h"

// ---- 1. Interfaces (compile time). A failing check stops the build with its message. ----
using Text = const std::string&;

// Presentation (Console UI)
static_assert(std::is_same<decltype(&runMainMenu), void (*)()>::value, "runMainMenu()");
static_assert(std::is_same<decltype(&runApplication), int (*)(int, char**)>::value,
              "runApplication(argc, argv)");
// Authentication (FR-10 / FR-11)
static_assert(std::is_same<decltype(&loginEmployee), bool (*)()>::value, "loginEmployee()");
static_assert(std::is_same<decltype(&isEmployeeAuthenticated), bool (*)()>::value,
              "isEmployeeAuthenticated()");
static_assert(std::is_same<decltype(&authenticateCustomer), bool (*)(int, Text)>::value,
              "authenticateCustomer(accountNo, pin)");
static_assert(std::is_same<decltype(&logoutEmployee), void (*)()>::value, "logoutEmployee()");
static_assert(std::is_same<decltype(&createFirstEmployee),
                           EmployeeSetupResult (*)(Text, Text)>::value,
              "createFirstEmployee(username, password)");
static_assert(std::is_same<decltype(&addEmployee),
                           EmployeeSetupResult (*)(Text, Text, Text, Text)>::value,
              "addEmployee(existingUsername, existingPassword, username, password)");
static_assert(std::is_same<decltype(&employeeSetup), bool (*)()>::value, "employeeSetup()");
// Password hashing (FR-10)
static_assert(std::is_same<decltype(&pbkdf2HmacSha256),
                           bool (*)(Text, Text, int, size_t, std::string&)>::value,
              "pbkdf2HmacSha256(password, salt, iterations, length, out)");
static_assert(std::is_same<decltype(&hashPassword), bool (*)(Text, std::string&)>::value,
              "hashPassword(password, stored)");
static_assert(std::is_same<decltype(&verifyPassword), bool (*)(Text, Text)>::value,
              "verifyPassword(password, stored)");
// Account Management (FR-1, FR-2, FR-7, FR-8, FR-9)
static_assert(std::is_same<decltype(&createAccount), bool (*)()>::value, "createAccount()");
static_assert(std::is_same<decltype(&createAccountWithDetails),
                           CreateAccountResult (*)(Text, Text, Text, Text, long long)>::value,
              "createAccountWithDetails(name, address, contact, pin, paise)");
static_assert(std::is_same<decltype(&generateAccountNumber),
                           int (*)(const std::vector<Account>&)>::value,
              "generateAccountNumber(existing)");
static_assert(std::is_same<decltype(&searchAccount), SearchAccountResult (*)(int)>::value,
              "searchAccount(accountNo)");
static_assert(std::is_same<decltype(&accountSearch), bool (*)()>::value, "accountSearch()");
static_assert(std::is_same<decltype(&updateAccountDetails),
                           UpdateAccountResult (*)(int, Text, Text, Text)>::value,
              "updateAccountDetails(accountNo, name, address, contact)");
static_assert(std::is_same<decltype(&accountUpdate), bool (*)()>::value, "accountUpdate()");
static_assert(std::is_same<decltype(&closeAccount), AccountClosureResult (*)(int)>::value,
              "closeAccount(accountNo)");
static_assert(std::is_same<decltype(&accountClosure), bool (*)()>::value, "accountClosure()");
// Transaction Management (FR-3 to FR-6)
static_assert(std::is_same<decltype(&deposit), DepositResult (*)(int, long long)>::value,
              "deposit(accountNo, paise)");
static_assert(std::is_same<decltype(&depositMoney), bool (*)()>::value, "depositMoney()");
static_assert(std::is_same<decltype(&withdraw), WithdrawalResult (*)(int, long long)>::value,
              "withdraw(accountNo, paise)");
static_assert(std::is_same<decltype(&withdrawMoney), bool (*)()>::value, "withdrawMoney()");
static_assert(std::is_same<decltype(&balanceEnquiry), bool (*)()>::value, "balanceEnquiry()");
static_assert(std::is_same<decltype(&miniStatement), bool (*)()>::value, "miniStatement()");
// File Storage / Transaction Log (FR-13)
static_assert(std::is_same<decltype(&loadAccounts), bool (*)(std::vector<Account>&)>::value,
              "loadAccounts(accounts)");
static_assert(std::is_same<decltype(&saveAccount), bool (*)(const Account&)>::value,
              "saveAccount(account)");
static_assert(std::is_same<decltype(&loadTransactions),
                           bool (*)(std::vector<Transaction>&)>::value,
              "loadTransactions(transactions)");
static_assert(std::is_same<decltype(&saveTransaction), bool (*)(const Transaction&)>::value,
              "saveTransaction(txn)");
static_assert(std::is_same<decltype(&loadEmployees), bool (*)(std::vector<Employee>&)>::value,
              "loadEmployees(employees)");
static_assert(std::is_same<decltype(&saveEmployee), bool (*)(const Employee&)>::value,
              "saveEmployee(employee)");

namespace {

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

bool readFile(const std::string& path, std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
}

// Project headers named in #include "..." lines (system headers use <...> and are ignored).
std::set<std::string> projectIncludes(const std::string& text) {
    std::set<std::string> includes;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line.compare(start, 10, "#include \"") != 0) continue;
        size_t open = start + 9;
        size_t close = line.find('"', open + 1);
        if (close != std::string::npos) includes.insert(line.substr(open + 1, close - open - 1));
    }
    return includes;
}

// ---- 2. Module map: one header and one source per major feature. ----
struct Module {
    const char* requirement;
    const char* sadComponent;
    const char* header;
    const char* source;
    std::vector<std::string> functions;   // declared in the header, defined in the source
};

const std::vector<Module> MODULES = {
    {"Console UI (NFR-2)", "Console UI", "menu.h", "menu.cpp", {"runMainMenu", "runApplication"}},
    {"FR-10 / FR-11 login", "Authentication", "auth.h", "auth.cpp",
     {"loginEmployee", "isEmployeeAuthenticated", "authenticateCustomer", "logoutEmployee",
      "createFirstEmployee", "addEmployee", "employeeSetup"}},
    {"FR-10 password hashing", "Authentication", "password_hash.h", "password_hash.cpp",
     {"pbkdf2HmacSha256", "hashPassword", "verifyPassword"}},
    {"FR-1 create account", "Account Management", "account.h", "account.cpp",
     {"createAccountWithDetails", "createAccount"}},
    {"FR-2 account numbers", "Account Management", "account.h", "account_number.cpp",
     {"generateAccountNumber"}},
    {"FR-3 / FR-4 deposit, withdrawal", "Transaction Management", "transactions.h",
     "transactions.cpp", {"deposit", "depositMoney", "withdraw", "withdrawMoney"}},
    {"FR-5 balance enquiry", "Transaction Management", "balance_enquiry.h",
     "balance_enquiry.cpp", {"balanceEnquiry"}},
    {"FR-6 mini-statement", "Transaction Log", "mini_statement.h", "mini_statement.cpp",
     {"miniStatement"}},
    {"FR-7 search account", "Account Management", "account_search.h", "account_search.cpp",
     {"searchAccount", "accountSearch"}},
    {"FR-8 update account", "Account Management", "account_update.h", "account_update.cpp",
     {"updateAccountDetails", "accountUpdate"}},
    {"FR-9 close account", "Account Management", "account_closure.h", "account_closure.cpp",
     {"closeAccount", "accountClosure"}},
    {"FR-13 storage", "File Storage", "storage.h", "storage.cpp",
     {"loadAccounts", "saveAccount", "loadTransactions", "saveTransaction", "loadEmployees",
      "saveEmployee"}},
};

// True if `text` has `name(` not preceded by a letter, digit or '_' (so "deposit(" does not
// match inside "depositMoney(").
bool mentionsFunction(const std::string& text, const std::string& name) {
    for (size_t pos = text.find(name + "("); pos != std::string::npos;
         pos = text.find(name + "(", pos + 1)) {
        if (pos == 0) return true;
        char before = text[pos - 1];
        if (!(std::isalnum(static_cast<unsigned char>(before)) || before == '_')) return true;
    }
    return false;
}

void testModuleMap() {
    for (const Module& m : MODULES) {
        std::string header, source;
        bool found = readFile(m.header, header) && readFile(m.source, source);
        bool allThere = found;
        for (const std::string& f : m.functions)
            if (!mentionsFunction(header, f) || !mentionsFunction(source, f)) allThere = false;
        check(allThere, std::string(m.requirement) + ": " + m.source + " / " + m.header
                            + " (" + m.sadComponent + ") hold its functions");
    }
}

// ---- 3. Dependency rules between project files. ----
// Every production file and the project headers it may include. Only files listed here are
// checked, so add every new module here; its dependencies then become part of the review.
const std::map<std::string, std::set<std::string>> ALLOWED = {
    // Data structures
    {"models.h", {}},
    // Interfaces: only the shared data structures
    {"auth.h", {}},
    {"menu.h", {}},
    {"password_hash.h", {}},
    {"storage.h", {"models.h"}},
    {"account.h", {"models.h"}},
    {"account_search.h", {"models.h"}},
    {"account_update.h", {"models.h"}},
    {"account_closure.h", {"models.h"}},
    {"transactions.h", {"models.h"}},
    {"balance_enquiry.h", {"models.h"}},
    {"mini_statement.h", {"models.h"}},
    // Presentation: main only starts the menus; the menus call features and login, never storage
    {"main.cpp", {"menu.h"}},
    {"menu.cpp", {"menu.h", "auth.h", "account.h", "account_search.h", "account_update.h",
                  "account_closure.h", "transactions.h", "balance_enquiry.h",
                  "mini_statement.h", "models.h"}},
    // Service / Business: own header, data structures, login and storage; never another feature
    {"auth.cpp", {"auth.h", "models.h", "storage.h", "password_hash.h"}},
    {"password_hash.cpp", {"password_hash.h"}},
    {"account.cpp", {"account.h", "models.h", "auth.h", "storage.h"}},
    {"account_number.cpp", {"account.h", "models.h"}},
    {"account_search.cpp", {"account_search.h", "models.h", "auth.h", "storage.h"}},
    {"account_update.cpp", {"account_update.h", "models.h", "auth.h", "storage.h"}},
    {"account_closure.cpp", {"account_closure.h", "models.h", "auth.h", "storage.h"}},
    {"transactions.cpp", {"transactions.h", "models.h", "auth.h", "storage.h"}},
    {"balance_enquiry.cpp", {"balance_enquiry.h", "models.h", "auth.h", "storage.h"}},
    {"mini_statement.cpp", {"mini_statement.h", "models.h", "auth.h", "storage.h"}},
    // Persistence: depends on nothing above it. password_hash.h only for the header-only
    // stored-hash format check (FR-10); no hashing code is linked into storage.
    {"storage.cpp", {"storage.h", "models.h", "password_hash.h"}},
};

void testDependencies() {
    for (const auto& entry : ALLOWED) {
        const std::string& file = entry.first;
        std::string text;
        if (!readFile(file, text)) {
            check(false, file + ": file found (run from the repository root)");
            continue;
        }
        std::string notAllowed;
        for (const std::string& inc : projectIncludes(text))
            if (entry.second.count(inc) == 0) notAllowed += " " + inc;
        check(notAllowed.empty(),
              file + ": includes only what its layer allows"
                  + (notAllowed.empty() ? "" : " (not allowed:" + notAllowed + ")"));
    }

    // No production file includes a .cpp file or a test file.
    bool clean = true;
    for (const auto& entry : ALLOWED) {
        std::string text;
        if (!readFile(entry.first, text)) continue;
        for (const std::string& inc : projectIncludes(text)) {
            bool isCpp = inc.size() > 4 && inc.compare(inc.size() - 4, 4, ".cpp") == 0;
            if (isCpp || inc.rfind("tests", 0) == 0) clean = false;
        }
    }
    check(clean, "No production file includes a .cpp file or a test file");
}

} // namespace

int main() {
    std::cout << "NFR-4 modular-structure tests. Links no module; reads the source files from the\n"
                 "current folder (run from the repository root).\n\n";

    // Reaching this line means all 33 static_asserts above compiled.
    check(true, "Interfaces: 33 public functions match their expected signatures "
                "(static_assert at compile time)");
    testModuleMap();
    testDependencies();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
