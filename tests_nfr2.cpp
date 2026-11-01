// NFR-2 menu and message-style tests (Test Plan TC-17): numbered menus with titles, clear
// handling of invalid choices, 0 to go back / exit, each option reaching the right feature,
// and the same title / message style on the FR-8 (update) and FR-9 (close) screens.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr2.exe tests_nfr2.cpp menu.cpp account_update.cpp account_closure.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_nfr2.cpp menu.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr2.exe
//
// Build ONLY these four files. Never link other test files (they define main() and the
// same fakes) or main.cpp / auth.cpp / storage.cpp / the other feature .cpp files.
//
// Uses the REAL menus (menu.cpp) and the REAL FR-8 / FR-9 console screens. The other six
// features, login and storage are fakes, so each menu option's route can be counted.
#include <iostream>
#include <sstream>
#include <string>
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
// Stand-ins for the features the menus call (except FR-8 / FR-9, which are real), for
// auth.cpp (FR-10 / FR-11) and for storage.cpp (FR-13).
// Defined only in this file; never linked into main.
namespace fake {
bool loginSucceeds = true;
bool employeeLoggedIn = true;
bool failLoadAccounts = false;
bool failSaveAccount = false;
std::vector<Account> accounts;
int saveCalls = 0;
int loginCalls = 0;
int createCalls = 0;
int searchCalls = 0;
int depositCalls = 0;
int withdrawCalls = 0;
int balanceCalls = 0;
int statementCalls = 0;
}

bool loginEmployee() { ++fake::loginCalls; return fake::loginSucceeds; }
bool isEmployeeAuthenticated() { return fake::employeeLoggedIn; }
bool authenticateCustomer(int, const std::string&) { return false; }
void logoutEmployee() {}            // FR-10; menu.cpp calls it on leaving the Employee Menu
bool employeeSetup() { return false; }   // FR-10; reached only through bank.exe --add-employee

bool createAccount()  { ++fake::createCalls;    return true; }
bool accountSearch()  { ++fake::searchCalls;    return true; }
bool depositMoney()   { ++fake::depositCalls;   return true; }
bool withdrawMoney()  { ++fake::withdrawCalls;  return true; }
bool balanceEnquiry() { ++fake::balanceCalls;   return true; }
bool miniStatement()  { ++fake::statementCalls; return true; }

bool loadAccounts(std::vector<Account>& accounts) {
    if (fake::failLoadAccounts) return false;
    accounts = fake::accounts;
    return true;
}

bool saveAccount(const Account& account) {
    ++fake::saveCalls;
    if (fake::failSaveAccount) return false;
    for (Account& a : fake::accounts) {
        if (a.accountNumber == account.accountNumber) {
            a = account;
            return true;
        }
    }
    fake::accounts.push_back(account);
    return true;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

const std::string MAIN_TITLE = "=== Bank Management System ===";
const std::string EMPLOYEE_TITLE = "=== Employee Menu ===";
const std::string UPDATE_TITLE = "--- Update Account ---";
const std::string CLOSE_TITLE = "--- Close Account ---";
const std::string PROMPT = "Enter choice: ";
const std::string INVALID_CHOICE = "Invalid choice. Please try again.";

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

Account makeAccount(int number, const std::string& contact, const std::string& status) {
    Account a;
    a.accountNumber = number;
    a.name = "Asha Rao";
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = contact;
    a.pin = "1234";
    a.balance = 100000;
    a.status = status;
    return a;
}

void reset() {
    fake::loginSucceeds = true;
    fake::employeeLoggedIn = true;
    fake::failLoadAccounts = false;
    fake::failSaveAccount = false;
    fake::accounts = {makeAccount(100001, "9876543210", STATUS_ACTIVE),
                      makeAccount(100002, "9123456780", STATUS_CLOSED)};
    fake::saveCalls = 0;
    fake::loginCalls = 0;
    fake::createCalls = 0;
    fake::searchCalls = 0;
    fake::depositCalls = 0;
    fake::withdrawCalls = 0;
    fake::balanceCalls = 0;
    fake::statementCalls = 0;
}

// Calls of the six faked features, in menu order.
int featureCalls() {
    return fake::createCalls + fake::searchCalls + fake::depositCalls + fake::withdrawCalls
           + fake::balanceCalls + fake::statementCalls;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

int countOf(const std::string& text, const std::string& part) {
    int count = 0;
    for (size_t pos = text.find(part); pos != std::string::npos; pos = text.find(part, pos + 1))
        ++count;
    return count;
}

std::string runWithInput(void (*menu)(), const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    menu();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    return out.str();
}

std::string runMenu(const std::string& input) {
    return runWithInput(runMainMenu, input);
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

// The option lines printed between `title` and the next "Enter choice: " prompt.
std::vector<std::string> menuOptions(const std::string& output, const std::string& title) {
    std::vector<std::string> options;
    size_t start = output.find(title);
    if (start == std::string::npos) return options;
    std::istringstream lines(output.substr(start + title.size()));
    std::string line;
    std::getline(lines, line);   // rest of the title line
    while (std::getline(lines, line) && line.rfind(PROMPT, 0) != 0)
        options.push_back(line);
    return options;
}

// Every option is "<number>. <text>", numbered 1, 2, 3 ... with 0 last.
bool numberedOneToNThenZero(const std::vector<std::string>& options) {
    if (options.size() < 2) return false;
    for (size_t i = 0; i < options.size(); ++i) {
        const std::string expected = (i + 1 == options.size()) ? "0" : std::to_string(i + 1);
        const std::string& line = options[i];
        if (line.rfind(expected + ". ", 0) != 0 || line.size() <= expected.size() + 2)
            return false;
    }
    return true;
}

void testMenuLayout() {
    reset();
    std::string output = runMenu("1\n");
    const std::vector<std::string> mainExpected = {"1. Employee Login", "2. Deposit Money",
        "3. Withdraw Money", "4. Balance Enquiry", "5. Mini-Statement", "0. Exit"};
    const std::vector<std::string> employeeExpected = {"1. Create Account", "2. Search Account",
        "3. Update Account", "4. Close Account", "0. Back to Main Menu"};
    std::vector<std::string> mainOptions = menuOptions(output, MAIN_TITLE);
    std::vector<std::string> employeeOptions = menuOptions(output, EMPLOYEE_TITLE);

    check(contains(output, MAIN_TITLE), "Main menu shows its title");
    check(numberedOneToNThenZero(mainOptions), "Main menu options numbered 1-5 then 0");
    check(mainOptions == mainExpected, "Main menu keeps the same 6 options and numbers");
    check(contains(output, EMPLOYEE_TITLE), "Employee Menu shows its title");
    check(numberedOneToNThenZero(employeeOptions), "Employee Menu options numbered 1-4 then 0");
    check(employeeOptions == employeeExpected, "Employee Menu keeps the same 5 options and numbers");
    // Main menu, Employee Menu, then the main menu again after end of input ends the Employee
    // Menu: every menu shown is followed by exactly one prompt.
    check(countOf(output, PROMPT) == countOf(output, MAIN_TITLE) + countOf(output, EMPLOYEE_TITLE)
              && countOf(output, PROMPT) == 3,
          "Each menu ends with the prompt \"Enter choice: \"");
}

void testInvalidChoices() {
    for (const char* choice : {"abc", "9", "", "-1", " 1", "1.0", "00", "6"}) {
        reset();
        std::string output = runMenu(std::string(choice) + "\n");
        check(contains(output, INVALID_CHOICE) && countOf(output, MAIN_TITLE) == 2
                  && featureCalls() == 0 && fake::loginCalls == 0,
              std::string("Main menu choice \"") + choice
                  + "\": \"Invalid choice\", menu shown again, nothing run");
    }
    for (const char* choice : {"abc", "5", "", "-1", "x1", "9"}) {
        reset();
        std::string output = runMenu(std::string("1\n") + choice + "\n");
        check(contains(output, INVALID_CHOICE) && countOf(output, EMPLOYEE_TITLE) == 2
                  && featureCalls() == 0 && fake::saveCalls == 0,
              std::string("Employee Menu choice \"") + choice
                  + "\": \"Invalid choice\", menu shown again, nothing run");
    }
}

void testZeroAndEndOfInput() {
    reset();
    std::string output = runMenu("0\n2\n");
    check(countOf(output, MAIN_TITLE) == 1 && fake::depositCalls == 0,
          "Main menu 0 exits (input after it is not read)");

    reset();
    // Main menu (1), Employee Menu (0), main menu again (5), main menu once more (end).
    output = runMenu("1\n0\n5\n");
    check(countOf(output, EMPLOYEE_TITLE) == 1 && countOf(output, MAIN_TITLE) == 3
              && fake::statementCalls == 1,
          "Employee Menu 0 goes back to the main menu");

    reset();
    output = runMenu("");
    check(countOf(output, MAIN_TITLE) == 1 && featureCalls() == 0,
          "End of input at the main menu: returns");

    reset();
    // The Employee Menu returns, the main menu is shown once more, then also returns.
    output = runMenu("1\n");
    check(countOf(output, EMPLOYEE_TITLE) == 1 && countOf(output, MAIN_TITLE) == 2,
          "End of input at the Employee Menu: returns, then the main menu returns");
}

void testMainMenuRouting() {
    struct Route { const char* choice; const char* name; const int* calls; };
    const Route routes[] = {{"2", "Deposit Money", &fake::depositCalls},
                            {"3", "Withdraw Money", &fake::withdrawCalls},
                            {"4", "Balance Enquiry", &fake::balanceCalls},
                            {"5", "Mini-Statement", &fake::statementCalls}};
    for (const Route& r : routes) {
        reset();
        std::string output = runMenu(std::string(r.choice) + "\n");
        check(*r.calls == 1 && featureCalls() == 1 && countOf(output, MAIN_TITLE) == 2,
              std::string("Main menu ") + r.choice + " runs " + r.name
                  + " only, then shows the menu again");
    }

    reset();
    std::string output = runMenu("1\n");
    check(fake::loginCalls == 1 && contains(output, EMPLOYEE_TITLE) && featureCalls() == 0,
          "Main menu 1 runs Employee Login, then shows the Employee Menu");

    reset();
    fake::loginSucceeds = false;
    output = runMenu("1\n");
    check(fake::loginCalls == 1 && !contains(output, EMPLOYEE_TITLE)
              && contains(output, "Returning to main menu.") && countOf(output, MAIN_TITLE) == 2,
          "Failed Employee Login: Employee Menu not shown, back to the main menu");
}

void testEmployeeMenuRouting() {
    reset();
    std::string output = runMenu("1\n1\n");
    check(fake::createCalls == 1 && featureCalls() == 1 && countOf(output, EMPLOYEE_TITLE) == 2,
          "Employee Menu 1 runs Create Account only, then shows the menu again");

    reset();
    output = runMenu("1\n2\n");
    check(fake::searchCalls == 1 && featureCalls() == 1 && countOf(output, EMPLOYEE_TITLE) == 2,
          "Employee Menu 2 runs Search Account only, then shows the menu again");

    reset();
    output = runMenu("1\n3\n");
    check(contains(output, UPDATE_TITLE) && !contains(output, CLOSE_TITLE) && featureCalls() == 0
              && countOf(output, EMPLOYEE_TITLE) == 2,
          "Employee Menu 3 opens the Update Account screen, then shows the menu again");

    reset();
    output = runMenu("1\n4\n");
    check(contains(output, CLOSE_TITLE) && !contains(output, UPDATE_TITLE) && featureCalls() == 0
              && countOf(output, EMPLOYEE_TITLE) == 2,
          "Employee Menu 4 opens the Close Account screen, then shows the menu again");
}

// FR-8 / FR-9 screens follow the same style as the other features: a "--- Title ---" line
// first, "Account number: " prompt, and messages starting "Error: ", "Access denied: " or
// "Storage error: " (successes unchanged).
void expectScreen(const std::string& label, bool (*flow)(), const std::string& title,
                  const std::string& input, const std::string& message, bool expectTitle) {
    std::string output;
    runFlow(flow, input, output);
    bool titleOk = expectTitle ? output.rfind("\n" + title + "\n", 0) == 0
                                     && contains(output, "Account number: ")
                               : !contains(output, title);
    check(titleOk && contains(output, message + "\n"), label + ": \"" + message + "\"");
}

void testUpdateScreen() {
    const std::string newDetails = "Asha Changed\n9 New Road, Hubballi\n9000000009\n";

    reset();
    fake::employeeLoggedIn = false;
    expectScreen("Update, not logged in", accountUpdate, UPDATE_TITLE, "100001\n",
                 "Access denied: employee login required.", false);
    reset();
    expectScreen("Update, bad account number", accountUpdate, UPDATE_TITLE, "12a\n",
                 "Error: Account number must be a positive whole number.", true);
    reset();
    expectScreen("Update, unknown account", accountUpdate, UPDATE_TITLE, "100009\n",
                 "Error: Account not found.", true);
    reset();
    expectScreen("Update, closed account", accountUpdate, UPDATE_TITLE, "100002\n",
                 "Error: Cannot update a closed account.", true);
    reset();
    fake::failLoadAccounts = true;
    expectScreen("Update, storage cannot be read", accountUpdate, UPDATE_TITLE, "100001\n",
                 "Storage error: could not read account data.", true);
    reset();
    expectScreen("Update, invalid name", accountUpdate, UPDATE_TITLE,
                 "100001\nAsha 2\n9 New Road, Hubballi\n9000000009\n",
                 "Error: Invalid name. Use letters, spaces, dots or apostrophes.", true);
    reset();
    expectScreen("Update, invalid address", accountUpdate, UPDATE_TITLE,
                 "100001\nAsha Changed\n   \n9000000009\n", "Error: Invalid address.", true);
    reset();
    expectScreen("Update, invalid contact number", accountUpdate, UPDATE_TITLE,
                 "100001\nAsha Changed\n9 New Road, Hubballi\n12345\n",
                 "Error: Contact number must contain exactly 10 digits.", true);
    reset();
    fake::accounts.push_back(makeAccount(100003, "9000000009", STATUS_ACTIVE));
    expectScreen("Update, contact number of another account", accountUpdate, UPDATE_TITLE,
                 "100001\n" + newDetails,
                 "Error: Contact number is already used by another active account.", true);
    reset();
    fake::failSaveAccount = true;
    expectScreen("Update, save fails", accountUpdate, UPDATE_TITLE, "100001\n" + newDetails,
                 "Storage error: account details were not updated.", true);
    reset();
    expectScreen("Update, success (message unchanged)", accountUpdate, UPDATE_TITLE,
                 "100001\n" + newDetails, "Account details updated successfully.", true);
}

void testCloseScreen() {
    reset();
    fake::employeeLoggedIn = false;
    expectScreen("Close, not logged in", accountClosure, CLOSE_TITLE, "100001\n",
                 "Access denied: employee login required.", false);
    reset();
    expectScreen("Close, bad account number", accountClosure, CLOSE_TITLE, "12a\n",
                 "Error: Account number must be a positive whole number.", true);
    reset();
    expectScreen("Close, unknown account", accountClosure, CLOSE_TITLE, "100009\n",
                 "Error: Account not found.", true);
    reset();
    expectScreen("Close, already closed", accountClosure, CLOSE_TITLE, "100002\n",
                 "Error: Account is already closed.", true);
    reset();
    fake::accounts[0].status = "FROZEN";
    expectScreen("Close, other status", accountClosure, CLOSE_TITLE, "100001\n",
                 "Error: Account has an invalid status.", true);
    reset();
    fake::failLoadAccounts = true;
    expectScreen("Close, storage cannot be read", accountClosure, CLOSE_TITLE, "100001\n",
                 "Storage error: could not read account data.", true);
    reset();
    fake::failSaveAccount = true;
    expectScreen("Close, save fails", accountClosure, CLOSE_TITLE, "100001\n",
                 "Storage error: account was not closed.", true);
    reset();
    expectScreen("Close, success (message unchanged)", accountClosure, CLOSE_TITLE, "100001\n",
                 "Account closed successfully.", true);
}

// The direct-call API returns the same wording as the screens.
void testApiMessages() {
    reset();
    check(updateAccountDetails(0, "Asha", "Road", "9000000009").message
              == "Error: Account number must be a positive whole number.",
          "updateAccountDetails(0, ...) uses the shared invalid-number message");
    check(closeAccount(-1).message == "Error: Account number must be a positive whole number.",
          "closeAccount(-1) uses the shared invalid-number message");
    check(closeAccount(100009).message == "Error: Account not found.",
          "closeAccount on an unknown account uses the shared not-found message");
}

} // namespace

int main() {
    std::cout << "NFR-2 menu and message-style tests. Real menus and real FR-8 / FR-9 screens;\n"
                 "other features, login and storage are test doubles.\n\n";

    testMenuLayout();
    testInvalidChoices();
    testZeroAndEndOfInput();
    testMainMenuRouting();
    testEmployeeMenuRouting();
    testUpdateScreen();
    testCloseScreen();
    testApiMessages();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
