// FR-13 persistent storage tests, using the REAL storage.cpp and the real FR-1 to FR-6 code.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr13.exe tests_fr13.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_fr13.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp /Fe:tests_fr13.exe
//
// Build ONLY these files. Never link other test files or main.cpp / auth.cpp into this program.
//
// Isolation: the program creates a fresh folder under the system temp directory, switches
// into it and works only on ./data/ there, then deletes the folder. It never touches the
// data/ folder of the repository. Restart tests run this same program again as separate
// child processes (--restart-phase1 / --restart-phase2), which refuse to run anywhere
// except inside such a temp folder.
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "account.h"
#include "auth.h"
#include "balance_enquiry.h"
#include "mini_statement.h"
#include "storage.h"
#include "transactions.h"

namespace fs = std::filesystem;

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for interfaces whose implementations do not exist yet:
//   isEmployeeAuthenticated -> FR-10 (auth.cpp)
//   authenticateCustomer    -> FR-11 (auth.cpp); checks the PIN against the REAL stored accounts
// Defined only in this file; never linked into main.
namespace fake {
bool employeeLoggedIn = true;
}

bool isEmployeeAuthenticated() { return fake::employeeLoggedIn; }

bool authenticateCustomer(int accountNo, const std::string& pin) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return false;
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo && a.pin == pin) return true;
    return false;
}
// =================== END TEST-ONLY FAKES ===================

namespace {

const std::string TEMP_PREFIX = "bms_fr13_";
const std::string ACC_HEADER = "BMS-ACCOUNTS 1\n";
const std::string TXN_HEADER = "BMS-TRANSACTIONS 1\n";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";

int passed = 0;
int failed = 0;
std::string prefix;   // marks output from child processes

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << prefix << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << prefix << "[FAIL] " << name << "\n"; }
}

// True only inside a temp folder created by this program.
bool inTestFolder() {
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    return !ec && cwd.filename().string().rfind(TEMP_PREFIX, 0) == 0;
}

// Deletes ./data/ - only ever inside the test temp folder.
void resetData() {
    if (!inTestFolder()) {
        std::cout << "Refusing to delete data/ outside the test temp folder.\n";
        std::exit(3);
    }
    fs::remove_all("data");
}

std::string readBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeBytes(const fs::path& path, const std::string& text) {
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

Account makeAccount(int number, const std::string& name, long long balance,
                    const std::string& status = STATUS_ACTIVE) {
    Account a;
    a.accountNumber = number;
    a.name = name;
    a.address = "12 MG Road, Bengaluru";
    a.contactNumber = "9876543210";
    a.pin = "1234";
    a.balance = balance;
    a.status = status;
    return a;
}

Transaction makeTxn(int id, int account, const std::string& type, long long amount) {
    Transaction t;
    t.transactionId = id;
    t.accountNumber = account;
    t.type = type;
    t.amount = amount;
    t.dateTime = "2026-10-27 14:30:00";
    return t;
}

bool sameAccount(const Account& a, const Account& b) {
    return a.accountNumber == b.accountNumber && a.name == b.name && a.address == b.address
           && a.contactNumber == b.contactNumber && a.pin == b.pin && a.balance == b.balance
           && a.status == b.status;
}

bool sameTxn(const Transaction& a, const Transaction& b) {
    return a.transactionId == b.transactionId && a.accountNumber == b.accountNumber
           && a.type == b.type && a.amount == b.amount && a.dateTime == b.dateTime;
}

const Account* findAccount(const std::vector<Account>& accounts, int number) {
    for (const Account& a : accounts)
        if (a.accountNumber == number) return &a;
    return nullptr;
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool runConsole(const std::function<bool()>& flow, const std::string& input, std::string& output) {
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

// ---------------------------------------------------------------- storage unit tests

void testMissingFiles() {
    resetData();
    std::vector<Account> accounts = {makeAccount(42, "Sentinel", 1)};
    std::vector<Transaction> txns = {makeTxn(42, 42, "X", 1)};
    check(loadAccounts(accounts) && accounts.empty(), "Missing accounts file: true with empty list");
    check(loadTransactions(txns) && txns.empty(), "Missing transactions file: true with empty list");
    check(!fs::exists("data"), "Loading never creates data/ or any file");

    check(saveAccount(makeAccount(100001, "Asha Rao", 100000)) && fs::is_regular_file(ACC_FILE),
          "First save creates data/accounts.txt");
    check(readBytes(ACC_FILE).rfind(ACC_HEADER, 0) == 0, "Accounts file starts with its header line");
    check(!fs::exists(ACC_FILE.string() + ".tmp"), "No temp file left after a successful save");
}

void testAccountRoundTrip() {
    resetData();
    std::vector<Account> saved = {
        makeAccount(100001, "Asha Rao", 0),
        makeAccount(100002, "Dr. A. O'Brien", 1, STATUS_CLOSED),
        makeAccount(100003, "Pipe | Back\\slash", LLONG_MAX, "FROZEN"),
        makeAccount(100004, "Line\nBreak\rCR", -500),
    };
    saved[2].address = "Flat 4 | Block B\\C, 2nd Cross";
    bool allSaved = true;
    for (const Account& a : saved) allSaved = saveAccount(a) && allSaved;

    std::vector<Account> loaded;
    bool ok = loadAccounts(loaded);
    bool same = ok && loaded.size() == saved.size();
    for (size_t i = 0; same && i < saved.size(); ++i) same = sameAccount(saved[i], loaded[i]);
    check(allSaved && same,
          "All 7 account fields round-trip (spaces, '|', '\\', newline, 0 / 1 / LLONG_MAX / -500 paise)");
    check(contains(readBytes(ACC_FILE), "|Asha Rao|12 MG Road, Bengaluru|"),
          "Text with spaces is stored readably, unescaped");
    check(contains(readBytes(ACC_FILE), "|0|ACTIVE\n") && contains(readBytes(ACC_FILE), "|9223372036854775807|FROZEN\n"),
          "Money stored as integer paise text");
}

void testAccountUpdate() {
    resetData();
    saveAccount(makeAccount(100001, "First", 100));
    saveAccount(makeAccount(100002, "Second", 200));
    saveAccount(makeAccount(100003, "Third", 300));

    Account updated = makeAccount(100002, "Second", 999, STATUS_CLOSED);
    check(saveAccount(updated), "Updating an existing account succeeds");

    std::vector<Account> loaded;
    loadAccounts(loaded);
    check(loaded.size() == 3 && loaded[0].accountNumber == 100001 && loaded[1].accountNumber == 100002
          && loaded[2].accountNumber == 100003,
          "Update keeps record count and order");
    check(loaded.size() == 3 && sameAccount(loaded[1], updated) && loaded[0].balance == 100
          && loaded[2].balance == 300,
          "Update changes only the target record");

    saveAccount(makeAccount(100004, "Fourth", 400));
    loaded.clear();
    loadAccounts(loaded);
    const Account* closed = findAccount(loaded, 100002);
    check(loaded.size() == 4 && closed && closed->status == STATUS_CLOSED,
          "CLOSED account stays stored with CLOSED status after later saves");
}

void testAccountRejected() {
    resetData();
    check(!saveAccount(makeAccount(0, "Zero", 1)) && !saveAccount(makeAccount(-1, "Neg", 1))
          && !fs::exists(ACC_FILE),
          "Account number <= 0 rejected; nothing written");

    saveAccount(makeAccount(100001, "Asha Rao", 100));
    std::string before = readBytes(ACC_FILE);
    check(!saveAccount(makeAccount(0, "Zero", 1)) && readBytes(ACC_FILE) == before,
          "Rejected account leaves existing file unchanged");
}

void testTransactions() {
    resetData();
    std::vector<Transaction> saved = {makeTxn(1, 100001, TXN_DEPOSIT, 50000),
                                      makeTxn(2, 100001, TXN_WITHDRAWAL, 20000),
                                      makeTxn(3, 100002, "Type | with \\ odd\nchars", 1)};
    check(saveTransaction(saved[0]) && saveTransaction(saved[1]), "Saving transactions succeeds");
    std::string beforeThird = readBytes(TXN_FILE);
    check(saveTransaction(saved[2]), "Saving a transaction with odd characters succeeds");
    std::string afterThird = readBytes(TXN_FILE);
    check(afterThird.size() > beforeThird.size() && afterThird.compare(0, beforeThird.size(), beforeThird) == 0,
          "Appending leaves earlier transaction bytes unchanged");

    std::vector<Transaction> loaded;
    bool same = loadTransactions(loaded) && loaded.size() == 3;
    for (size_t i = 0; same && i < 3; ++i) same = sameTxn(saved[i], loaded[i]);
    check(same, "All transaction fields round-trip, oldest first");

    check(!saveTransaction(makeTxn(2, 100001, TXN_DEPOSIT, 5)) && readBytes(TXN_FILE) == afterThird,
          "Duplicate transaction ID rejected; file unchanged");
    check(!saveTransaction(makeTxn(0, 100001, TXN_DEPOSIT, 5))
          && !saveTransaction(makeTxn(9, 0, TXN_DEPOSIT, 5)) && readBytes(TXN_FILE) == afterThird,
          "Transaction ID or account number <= 0 rejected; file unchanged");
}

// Each malformed file: load returns false with the vector untouched, and a later save
// returns false without changing a single byte.
void expectMalformedAccounts(const std::string& label, const std::string& content) {
    resetData();
    writeBytes(ACC_FILE, content);
    std::vector<Account> accounts = {makeAccount(42, "Sentinel", 7)};
    bool loadOk = loadAccounts(accounts);
    bool untouched = accounts.size() == 1 && accounts[0].accountNumber == 42;
    bool saveOk = saveAccount(makeAccount(100009, "New", 1));
    check(!loadOk && untouched && !saveOk && readBytes(ACC_FILE) == content,
          "Malformed accounts (" + label + "): load false, vector untouched, save refused, file unchanged");
}

void expectMalformedTransactions(const std::string& label, const std::string& content) {
    resetData();
    writeBytes(TXN_FILE, content);
    std::vector<Transaction> txns = {makeTxn(42, 42, "Sentinel", 7)};
    bool loadOk = loadTransactions(txns);
    bool untouched = txns.size() == 1 && txns[0].transactionId == 42;
    bool saveOk = saveTransaction(makeTxn(100, 100001, TXN_DEPOSIT, 1));
    check(!loadOk && untouched && !saveOk && readBytes(TXN_FILE) == content,
          "Malformed transactions (" + label + "): load false, vector untouched, save refused, file unchanged");
}

void testMalformedFiles() {
    const std::string good = "100001|Asha Rao|12 MG Road|9876543210|1234|100000|ACTIVE\n";
    expectMalformedAccounts("empty file", "");
    expectMalformedAccounts("wrong header", "BMS-ACCOUNTS 2\n" + good);
    expectMalformedAccounts("no header", good);
    expectMalformedAccounts("6 fields", ACC_HEADER + "100001|Asha|Road|9876543210|1234|100\n");
    expectMalformedAccounts("8 fields", ACC_HEADER + "100001|Asha|Road|9876543210|1234|100|ACTIVE|x\n");
    expectMalformedAccounts("non-numeric balance", ACC_HEADER + "100001|Asha|Road|9876543210|1234|abc|ACTIVE\n");
    expectMalformedAccounts("decimal balance", ACC_HEADER + "100001|Asha|Road|9876543210|1234|1.50|ACTIVE\n");
    expectMalformedAccounts("balance overflow", ACC_HEADER + "100001|Asha|Road|9876543210|1234|9223372036854775808|ACTIVE\n");
    expectMalformedAccounts("account number overflow", ACC_HEADER + "2147483648|Asha|Road|9876543210|1234|1|ACTIVE\n");
    expectMalformedAccounts("account number 0", ACC_HEADER + "0|Asha|Road|9876543210|1234|1|ACTIVE\n");
    expectMalformedAccounts("account number +5", ACC_HEADER + "+5|Asha|Road|9876543210|1234|1|ACTIVE\n");
    expectMalformedAccounts("invalid escape", ACC_HEADER + "100001|Asha \\x Rao|Road|9876543210|1234|1|ACTIVE\n");
    expectMalformedAccounts("unfinished escape", ACC_HEADER + "100001|Asha|Road|9876543210|1234|1|ACTIVE\\\n");
    expectMalformedAccounts("duplicate account number", ACC_HEADER + good + good);
    expectMalformedAccounts("blank line", ACC_HEADER + good + "\n" + good);
    expectMalformedAccounts("one bad record among good ones",
                            ACC_HEADER + good + "100002|Ravi|Road|9123456780|5678|oops|ACTIVE\n");

    const std::string goodTxn = "1|100001|DEPOSIT|50000|2026-10-27 14:30:00\n";
    expectMalformedTransactions("accounts header", ACC_HEADER + goodTxn);
    expectMalformedTransactions("4 fields", TXN_HEADER + "1|100001|DEPOSIT|50000\n");
    expectMalformedTransactions("amount overflow", TXN_HEADER + "1|100001|DEPOSIT|99999999999999999999|2026-10-27 14:30:00\n");
    expectMalformedTransactions("non-numeric ID", TXN_HEADER + "abc|100001|DEPOSIT|1|2026-10-27 14:30:00\n");
    expectMalformedTransactions("ID 0", TXN_HEADER + "0|100001|DEPOSIT|1|2026-10-27 14:30:00\n");
    expectMalformedTransactions("duplicate ID", TXN_HEADER + goodTxn + goodTxn);
    expectMalformedTransactions("invalid escape", TXN_HEADER + "1|100001|DEP\\qOSIT|1|2026-10-27 14:30:00\n");
}

void testCrlfFiles() {
    resetData();
    writeBytes(ACC_FILE, "BMS-ACCOUNTS 1\r\n100001|Asha Rao|12 MG Road|9876543210|1234|100000|ACTIVE\r\n");
    std::vector<Account> accounts;
    check(loadAccounts(accounts) && accounts.size() == 1 && accounts[0].status == STATUS_ACTIVE
          && accounts[0].balance == 100000,
          "CRLF line endings load correctly (no trailing \\r in fields)");
}

void testUnreadableFiles() {
    resetData();
    fs::create_directories(ACC_FILE);   // a directory where the file should be
    std::vector<Account> accounts = {makeAccount(42, "Sentinel", 7)};
    check(!loadAccounts(accounts) && accounts.size() == 1 && !saveAccount(makeAccount(100001, "A", 1)),
          "accounts.txt is a directory: load false (vector untouched), save false");

    resetData();
    fs::create_directories(TXN_FILE);
    std::vector<Transaction> txns;
    check(!loadTransactions(txns) && !saveTransaction(makeTxn(1, 100001, TXN_DEPOSIT, 1)),
          "transactions.txt is a directory: load false, save false");
}

void testWriteFailures() {
    resetData();
    saveAccount(makeAccount(100001, "Asha Rao", 100000));
    std::string before = readBytes(ACC_FILE);
    fs::create_directories(ACC_FILE.string() + ".tmp");   // temp file cannot be created
    check(!saveAccount(makeAccount(100002, "Ravi", 1)) && readBytes(ACC_FILE) == before,
          "Temp file cannot be created: save false, original file unchanged");

    resetData();
    writeBytes("data", "not a folder");
    check(!saveAccount(makeAccount(100001, "Asha", 1)) && readBytes("data") == "not a folder",
          "data exists as a plain file: save false, file untouched");
    fs::remove("data");

#ifdef _WIN32
    // On Windows a file held open by another handle cannot be replaced.
    resetData();
    saveAccount(makeAccount(100001, "Asha Rao", 100000));
    before = readBytes(ACC_FILE);
    bool saveOk;
    {
        std::ifstream holder(ACC_FILE, std::ios::binary);
        saveOk = saveAccount(makeAccount(100001, "Asha Rao", 1));
    }
    check(!saveOk && readBytes(ACC_FILE) == before && !fs::exists(ACC_FILE.string() + ".tmp"),
          "Windows: file held open, replace fails: save false, original unchanged, temp removed");
#endif
}

// ---------------------------------------------------------------- FR-1..FR-6 with real storage

// The FR-3 / FR-4 rollback path when the transaction file cannot be written.
void testRollbackWithRealStorage() {
    resetData();
    CreateAccountResult created = createAccountWithDetails("Asha Rao", "12 MG Road", "9876543210",
                                                           "1234", 100000);
    check(created.success && created.accountNumber == 100001, "FR-1 creates account 100001 in real storage");

    fs::create_directories(TXN_FILE.string() + ".tmp");   // transaction save will fail
    DepositResult d = deposit(100001, 50000);
    std::vector<Account> accounts;
    loadAccounts(accounts);
    check(!d.success && contains(d.message, "balance unchanged") && accounts.size() == 1
          && accounts[0].balance == 100000 && !fs::exists(TXN_FILE),
          "FR-3 deposit: transaction save fails -> not reported as success, balance on disk restored");

    WithdrawalResult w = withdraw(100001, 20000);
    accounts.clear();
    loadAccounts(accounts);
    check(!w.success && contains(w.message, "balance restored") && accounts[0].balance == 100000
          && !fs::exists(TXN_FILE),
          "FR-4 withdrawal: transaction save fails -> not reported as success, balance on disk restored");

    fs::remove(TXN_FILE.string() + ".tmp");
    d = deposit(100001, 50000);
    accounts.clear();
    std::vector<Transaction> txns;
    loadAccounts(accounts);
    loadTransactions(txns);
    check(d.success && accounts[0].balance == 150000 && txns.size() == 1 && txns[0].transactionId == 1
          && txns[0].amount == 50000,
          "After the fault clears, deposit saves balance and transaction together");
}

// Phase 1 runs in its own process, then exits.
int restartPhase1() {
    prefix = "    (process 1) ";
    CreateAccountResult a = createAccountWithDetails("Asha Rao", "12 MG Road, Bengaluru",
                                                     "9876543210", "1234", 100000);
    CreateAccountResult b = createAccountWithDetails("Ravi Kumar", "5 Park Street", "9123456780",
                                                     "5678", 250000);
    check(a.success && a.accountNumber == 100001 && b.success && b.accountNumber == 100002,
          "FR-1/FR-2 create accounts 100001 and 100002");
    DepositResult d1 = deposit(100001, 50000);
    WithdrawalResult w1 = withdraw(100001, 20000);
    DepositResult d2 = deposit(100002, 1);
    check(d1.success && w1.success && d2.success && w1.newBalance == 130000,
          "FR-3/FR-4 deposit, withdraw, deposit");

    std::vector<Account> accounts;
    loadAccounts(accounts);
    Account closing = *findAccount(accounts, 100002);
    closing.status = STATUS_CLOSED;
    check(saveAccount(closing), "Account 100002 saved as CLOSED");
    return failed == 0 ? 0 : 1;
}

// Phase 2 runs in a new process and sees only what phase 1 left on disk.
int restartPhase2() {
    prefix = "    (process 2) ";
    std::vector<Account> accounts;
    std::vector<Transaction> txns;
    check(loadAccounts(accounts) && accounts.size() == 2, "Both accounts reloaded after restart");
    const Account* asha = findAccount(accounts, 100001);
    const Account* ravi = findAccount(accounts, 100002);
    check(asha && asha->name == "Asha Rao" && asha->address == "12 MG Road, Bengaluru"
          && asha->contactNumber == "9876543210" && asha->pin == "1234" && asha->balance == 130000
          && asha->status == STATUS_ACTIVE,
          "Account 100001: all fields and balance Rs. 1300.00 restored");
    check(ravi && ravi->status == STATUS_CLOSED && ravi->balance == 250001,
          "Account 100002: CLOSED status and balance restored");

    check(loadTransactions(txns) && txns.size() == 3 && txns[0].transactionId == 1
          && txns[0].type == TXN_DEPOSIT && txns[0].amount == 50000 && txns[1].transactionId == 2
          && txns[1].type == TXN_WITHDRAWAL && txns[1].amount == 20000 && txns[2].transactionId == 3
          && txns[2].accountNumber == 100002,
          "Transactions 1-3 restored with IDs, types and amounts");

    CreateAccountResult c = createAccountWithDetails("Meera Iyer", "7 Hill Road", "9000011111",
                                                     "4321", 10000);
    check(c.success && c.accountNumber == 100003, "FR-2 after restart: next number 100003 (CLOSED 100002 not reused)");
    DepositResult d = deposit(100003, 100);
    check(d.success && d.transactionId == 4, "Transaction IDs continue after restart (next ID 4)");
    check(!deposit(100002, 100).success, "Deposit to the restored CLOSED account is still rejected");

    std::string output;
    bool ok = runConsole(balanceEnquiry, "100001\n1234\n", output);
    check(ok && contains(output, "Rs. 1300.00"), "FR-5 balance enquiry shows the restored balance");
    ok = runConsole(miniStatement, "100001\n1234\n", output);
    check(ok && contains(output, "WITHDRAW") && contains(output, "200.00") && contains(output, "DEPOSIT")
          && contains(output, "500.00"),
          "FR-6 mini-statement lists the restored transactions");
    return failed == 0 ? 0 : 1;
}

fs::path selfPath;

int runChild(const std::string& phase) {
    std::string command = "\"" + selfPath.string() + "\" " + phase;
#ifdef _WIN32
    command = "\"" + command + "\"";   // cmd.exe strips one pair of outer quotes
#endif
    std::cout.flush();
    return std::system(command.c_str());
}

void testRestart() {
    resetData();
    int phase1 = runChild("--restart-phase1");
    check(phase1 == 0, "Restart phase 1 (separate process) created data and exited cleanly");
    int phase2 = runChild("--restart-phase2");
    check(phase2 == 0, "Restart phase 2 (new process) found everything phase 1 saved");

    std::vector<Account> accounts;
    std::vector<Transaction> txns;
    check(loadAccounts(accounts) && accounts.size() == 3 && loadTransactions(txns) && txns.size() == 4,
          "After both processes: 3 accounts and 4 transactions on disk");
}

} // namespace

int main(int argc, char* argv[]) {
    std::error_code ec;
    selfPath = fs::absolute(argv[0], ec);
    if (argc == 2) {
        std::string phase = argv[1];
        if (!inTestFolder()) {
            std::cout << "Refusing to run a restart phase outside the test temp folder.\n";
            return 3;
        }
        if (phase == "--restart-phase1") return restartPhase1();
        if (phase == "--restart-phase2") return restartPhase2();
        return 2;
    }

    std::cout << "FR-13 storage tests with the REAL storage.cpp and FR-1 to FR-6 code. Employee and\n"
                 "customer login are test doubles. Data lives only in a temporary folder.\n\n";

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

    testMissingFiles();
    testAccountRoundTrip();
    testAccountUpdate();
    testAccountRejected();
    testTransactions();
    testMalformedFiles();
    testCrlfFiles();
    testUnreadableFiles();
    testWriteFailures();
    testRollbackWithRealStorage();
    testRestart();

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
