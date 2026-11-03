// NFR-6 persistence-across-restart tests (SRS NFR-6, FR-13; Test Plan TC-15).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr6.exe tests_nfr6.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp -lcrypto
//    or: cl /std:c++17 /EHsc /W4 tests_nfr6.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr6.exe
//
// NO test doubles: the bank.exe file list with this file in place of main.cpp.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+; on Linux also libssl-dev.
//
// Every step is a SEPARATE PROCESS: this program starts itself again with
//   --session <input file> <output file> [--add-employee]
// and that child runs runApplication(), the same function bank.exe's main() calls, with the
// input file as the keyboard. The child exits before the next step starts, so whatever a later
// session sees can only have come from the files on disk. Child processes refuse to run
// anywhere except inside the temporary folder this program creates and deletes.
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

#ifndef _WIN32
#include <sys/wait.h>   // WIFEXITED / WEXITSTATUS for std::system's result
#endif

#include "menu.h"
#include "models.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_nfr6_test_";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const fs::path EMP_FILE = fs::path("data") / "employees.txt";

const std::string LOGIN = "1\nstaff.one\nStaff-Pass-06\n";   // main menu 1, then credentials

int passed = 0;
int failed = 0;
fs::path selfPath;
int sessionCount = 0;

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

// ---- child side: one run of the real application, then exit ----
int childSession(const std::string& inputPath, const std::string& outputPath, bool addEmployee) {
    std::ifstream in(inputPath, std::ios::binary);
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!in.is_open() || !out.is_open()) return 90;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    std::string program = "bank.exe", option = "--add-employee";
    char* argv[] = {&program[0], &option[0], nullptr};
    int code = runApplication(addEmployee ? 2 : 1, argv);
    std::cout.flush();
    std::cin.rdbuf(oldIn);     // restore before the file streams are destroyed
    std::cout.rdbuf(oldOut);
    return code;
}

// ---- parent side ----
struct Session {
    int exitCode = -1;
    std::string output;
};

// Starts a new process that runs the application once with `input` typed in, and waits for it
// to exit.
Session runSession(const std::string& input, bool addEmployee = false) {
    ++sessionCount;
    std::string base = "session" + std::to_string(sessionCount);
    writeBytes(base + ".in", input);
    std::string command = "\"" + selfPath.string() + "\" --session " + base + ".in " + base + ".out"
                          + (addEmployee ? " --add-employee" : "");
#ifdef _WIN32
    command = "\"" + command + "\"";   // cmd.exe strips one pair of outer quotes
#endif
    std::cout.flush();
    Session s;
    int status = std::system(command.c_str());
#ifdef _WIN32
    s.exitCode = status;
#else
    s.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    s.output = readBytes(base + ".out");
    return s;
}

const Account* findAccount(const std::vector<Account>& accounts, int number) {
    for (const Account& a : accounts)
        if (a.accountNumber == number) return &a;
    return nullptr;
}

// ------------------------------------------------------------------ the restart scenario
void testRestarts() {
    // Process 1: first employee (bank.exe --add-employee).
    Session s = runSession("staff.one\nStaff-Pass-06\nStaff-Pass-06\n", true);
    check(s.exitCode == 0 && contains(s.output, "Employee staff.one added."),
          "Process 1 (--add-employee): first employee created, process exited with 0");

    // Process 2: log in and create an account, then exit.
    s = runSession(LOGIN + "1\nAsha Rao\n12 MG Road, Bengaluru\n9876543210\n1234\n1000\n0\n0\n");
    check(s.exitCode == 0 && contains(s.output, "Employee login successful.")
              && contains(s.output, "Account created successfully. Account number: 100001"),
          "Process 2: employee login (needs the employee saved by process 1) and account 100001 created");

    // Process 3: the account created by process 2 is found; then update it.
    s = runSession(LOGIN + "2\n100001\n3\n100001\nAsha R. Rao\n9 Lake View, Mysuru\n9123456780\n0\n0\n");
    check(contains(s.output, "Account number : 100001") && contains(s.output, "Name           : Asha Rao")
              && contains(s.output, "Balance        : Rs. 1000.00"),
          "TC-15 Process 3: account created in an earlier process is found after restart");
    check(contains(s.output, "Account details updated successfully."), "Process 3: account details updated");

    // Process 4: the update survived; a deposit and a withdrawal by the customer.
    s = runSession(LOGIN + "2\n100001\n0\n2\n100001\n1234\n500\n3\n100001\n1234\n200\n0\n");
    check(contains(s.output, "Name           : Asha R. Rao") && contains(s.output, "Address        : 9 Lake View, Mysuru")
              && contains(s.output, "Contact number : 9123456780"),
          "TC-15 Process 4: updated name, address and contact number retained after restart");
    check(contains(s.output, "Deposit successful.") && contains(s.output, "Withdrawal successful.")
              && contains(s.output, "Remaining balance: Rs. 1300.00"),
          "Process 4: customer deposit Rs. 500 and withdrawal Rs. 200 (PIN from process 2 accepted)");

    // Process 5: balance and history survived; create a second account and close it.
    s = runSession("4\n100001\n1234\n5\n100001\n1234\n" + LOGIN
                   + "1\nRavi Kumar\n5 Park Street\n9000000002\n5678\n250\n4\n100002\n0\n0\n");
    check(contains(s.output, "Current balance for account 100001: Rs. 1300.00"),
          "TC-15 Process 5: balance after deposit and withdrawal retained after restart");
    check(contains(s.output, "DEPOSIT") && contains(s.output, "500.00") && contains(s.output, "WITHDRAW")
              && contains(s.output, "200.00"),
          "TC-15 Process 5: transaction history (deposit and withdrawal) recovered after restart");
    check(contains(s.output, "Account number: 100002") && contains(s.output, "Account closed successfully."),
          "Process 5: account 100002 created and closed");

    // Process 6: the closure survived and is enforced.
    s = runSession(LOGIN + "2\n100002\n0\n2\n100002\n5678\n100\n0\n");
    check(contains(s.output, "Account number : 100002") && contains(s.output, "Status         : CLOSED"),
          "TC-15 Process 6: CLOSED status retained after restart");
    check(contains(s.output, "Access denied: invalid account number or PIN."),
          "Process 6: the restored CLOSED account still refuses customer operations");
}

// Failed operations in one process leave the files exactly as they were for the next one.
void testFailedOperationsAcrossRestart() {
    std::string accounts = readBytes(ACC_FILE), transactions = readBytes(TXN_FILE),
                employees = readBytes(EMP_FILE);
    std::error_code ec;
    auto oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
    for (const fs::path& p : {ACC_FILE, TXN_FILE, EMP_FILE}) fs::last_write_time(p, oldTime, ec);

    Session s = runSession(
        "3\n100001\n1234\n5000\n"                      // insufficient balance
        "2\n100001\n9999\n500\n"                       // wrong PIN
        "2\n100001\n1234\nabc\n"                       // invalid amount
        "1\nstaff.one\nwrong-password\n"               // wrong employee password
        + LOGIN
        + "1\nKiran Das\n4 Lake Road\n9123456780\n1111\n500\n"   // duplicate contact (100001)
          "3\n999999\n"                                 // update a non-existent account
          "4\n100002\n"                                 // close an already closed account
          "0\n0\n");
    check(contains(s.output, "Insufficient balance") && contains(s.output, "Access denied: invalid account number or PIN.")
              && contains(s.output, "Error: Deposit amount must be") && contains(s.output, "Invalid username or password.")
              && contains(s.output, "already exists") && contains(s.output, "Error: Account not found.")
              && contains(s.output, "Error: Account is already closed."),
          "Process 7: seven invalid operations, each rejected with its error");

    bool unchanged = readBytes(ACC_FILE) == accounts && readBytes(TXN_FILE) == transactions
                     && readBytes(EMP_FILE) == employees;
    for (const fs::path& p : {ACC_FILE, TXN_FILE, EMP_FILE})
        if (fs::last_write_time(p, ec) != oldTime || fs::exists(p.string() + ".tmp")) unchanged = false;
    check(unchanged, "After process 7 exits: all data files byte-for-byte unchanged and not rewritten");

    s = runSession("4\n100001\n1234\n0\n");
    check(contains(s.output, "Current balance for account 100001: Rs. 1300.00"),
          "Process 8: after restart the balance is still Rs. 1300.00");
}

// What the last process left on disk, read here (a different process) with the real storage.
void testFinalFiles() {
    std::vector<Account> accounts;
    std::vector<Transaction> txns;
    bool loaded = loadAccounts(accounts) && loadTransactions(txns);
    const Account* asha = findAccount(accounts, 100001);
    const Account* ravi = findAccount(accounts, 100002);
    check(loaded && accounts.size() == 2 && asha && asha->name == "Asha R. Rao"
              && asha->address == "9 Lake View, Mysuru" && asha->contactNumber == "9123456780"
              && asha->pin == "1234" && asha->balance == 130000 && asha->status == STATUS_ACTIVE,
          "On disk: account 100001 with every updated field and balance Rs. 1300.00");
    check(ravi && ravi->status == STATUS_CLOSED && ravi->balance == 25000,
          "On disk: account 100002 kept (not deleted) with status CLOSED");
    check(txns.size() == 2 && txns[0].transactionId == 1 && txns[0].accountNumber == 100001
              && txns[0].type == TXN_DEPOSIT && txns[0].amount == 50000 && txns[1].transactionId == 2
              && txns[1].type == TXN_WITHDRAWAL && txns[1].amount == 20000 && !txns[1].dateTime.empty(),
          "On disk: exactly the two successful transactions, in order, with IDs, types and amounts");
    check(contains(readBytes(EMP_FILE), "staff.one|pbkdf2-sha256$"), "On disk: the employee record");
}

} // namespace

int main(int argc, char* argv[]) {
    std::error_code ec;
    selfPath = fs::absolute(argv[0], ec);
    if (argc >= 4 && std::string(argv[1]) == "--session") {
        if (!inTestFolder()) {
            std::cout << "Refusing to run a session outside the test temp folder.\n";
            return 3;
        }
        return childSession(argv[2], argv[3], argc == 5 && std::string(argv[4]) == "--add-employee");
    }

    std::cout << "NFR-6 persistence tests: every step is a separate run of the real application\n"
                 "(no test doubles). Data lives only in a temporary folder.\n\n";

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
    if (ec || !inTestFolder()) {
        std::cout << "Could not switch to the temporary test folder; no tests run.\n";
        return 2;
    }
    std::cout << "Temporary folder: " << testRoot.string() << "\n\n";

    check(!fs::exists("data"), "Start: no data folder (first launch)");
    testRestarts();
    testFailedOperationsAcrossRestart();
    testFinalFiles();
    check(sessionCount == 8, "8 separate application processes were run");

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
