// NFR-1 response-time tests (Test Plan TC-16): deposit, withdrawal and balance enquiry must each
// complete within 2 seconds in normal local use.
// Build: g++ -std=c++17 -Wall -Wextra -o tests_nfr1.exe tests_nfr1.cpp storage.cpp transactions.cpp balance_enquiry.cpp
//    or: cl /std:c++17 /EHsc /W4 tests_nfr1.cpp storage.cpp transactions.cpp balance_enquiry.cpp /Fe:tests_nfr1.exe
//
// Uses the REAL storage.cpp, transactions.cpp and balance_enquiry.cpp; only customer login is a
// fake (auth.cpp does not exist yet). Each timed run is a whole console flow, from the typed
// input to the printed result, so it includes reading and rewriting the data files.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+.
// Data lives only in a temporary folder that this program creates and deletes.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "auth.h"
#include "balance_enquiry.h"
#include "storage.h"
#include "transactions.h"

// ===================== TEST-ONLY FAKES =====================
// Stand-ins for auth.cpp (FR-10 / FR-11), which does not exist yet.
// Defined only in this file; never linked into main.

// Accepts the stored PIN of a stored ACTIVE account, read through the real storage, so the
// timed flows pay the same file-reading cost a real login would.
bool authenticateCustomer(int accountNo, const std::string& pin) {
    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) return false;
    for (const Account& a : accounts)
        if (a.accountNumber == accountNo) return a.pin == pin && a.status == STATUS_ACTIVE;
    return false;
}

bool loginEmployee() { return false; }
bool isEmployeeAuthenticated() { return false; }
// =================== END TEST-ONLY FAKES ===================

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

const std::string TEMP_PREFIX = "bms_nfr1_test_";
const double LIMIT_MS = 2000.0;             // SRS NFR-1: 2 seconds
const int ACCOUNT_COUNT = 500;              // accounts 100001 .. 100500
const int TXNS_PER_ACCOUNT = 20;            // 10000 stored transactions
const int RUNS = 50;                        // timed runs per operation
const long long START_BALANCE = 1000000;    // Rs. 10000.00 per account

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Writes the data files directly in the FR-13 format (see README "Data storage"). Saving
// 10000 transactions one by one would rewrite the file 10000 times just to set up the test.
void writeSeedData() {
    fs::create_directories("data");
    std::ofstream acc(fs::path("data") / "accounts.txt", std::ios::binary | std::ios::trunc);
    acc << "BMS-ACCOUNTS 1\n";
    for (int i = 1; i <= ACCOUNT_COUNT; ++i) {
        acc << (100000 + i) << "|Customer " << i << "|" << i << " MG Road, Bengaluru|"
            << (9000000000LL + i) << "|1234|" << START_BALANCE << "|ACTIVE\n";
    }

    std::ofstream txn(fs::path("data") / "transactions.txt", std::ios::binary | std::ios::trunc);
    txn << "BMS-TRANSACTIONS 1\n";
    int id = 0;
    for (int round = 0; round < TXNS_PER_ACCOUNT; ++round) {
        for (int i = 1; i <= ACCOUNT_COUNT; ++i) {
            const char* type = (round % 2 == 0) ? "DEPOSIT" : "WITHDRAW";
            txn << ++id << "|" << (100000 + i) << "|" << type << "|10000|2026-10-01 10:00:00\n";
        }
    }
}

// Runs a console flow with `input` as the keyboard; returns its result and elapsed time.
bool timeConsole(bool (*flow)(), const std::string& input, std::string& output, double& ms) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    Clock::time_point start = Clock::now();
    bool ok = flow();
    Clock::time_point end = Clock::now();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    output = out.str();
    ms = std::chrono::duration<double, std::milli>(end - start).count();
    return ok;
}

std::string formatMs(double ms) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(2) << ms << " ms";
    return s.str();
}

// Times RUNS console runs of one operation, spread over different accounts.
void timeOperation(const std::string& name, bool (*flow)(), const std::string& amountLine,
                   const std::string& successText) {
    std::vector<double> times;
    bool allOk = true;
    for (int run = 0; run < RUNS; ++run) {
        int accountNo = 100001 + (run * 37) % ACCOUNT_COUNT;
        std::string input = std::to_string(accountNo) + "\n1234\n" + amountLine;
        std::string output;
        double ms = 0;
        bool ok = timeConsole(flow, input, output, ms);
        if (!ok || !contains(output, successText)) allOk = false;
        times.push_back(ms);
    }
    std::sort(times.begin(), times.end());
    double median = (times[RUNS / 2 - 1] + times[RUNS / 2]) / 2;
    double worst = times.back();

    std::cout << "  " << std::left << std::setw(17) << name << " " << RUNS << " runs: median "
              << formatMs(median) << ", worst " << formatMs(worst) << "\n";
    check(allOk, "TC-16 " + name + ": all " + std::to_string(RUNS) + " runs succeeded");
    check(worst < LIMIT_MS, "TC-16 " + name + ": worst " + formatMs(worst) + " is under 2 seconds");
}

} // namespace

int main() {
    std::cout << "NFR-1 response-time tests with the REAL storage.cpp, transactions.cpp and\n"
                 "balance_enquiry.cpp. Customer login is a test double. Data lives only in a\n"
                 "temporary folder.\n\n";

    std::error_code ec;
    std::random_device rd;
    std::string folderName = TEMP_PREFIX
        + std::to_string(Clock::now().time_since_epoch().count()) + "_" + std::to_string(rd());
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

    writeSeedData();
    std::vector<Account> accounts;
    std::vector<Transaction> txns;
    const int seededTxns = ACCOUNT_COUNT * TXNS_PER_ACCOUNT;
    check(loadAccounts(accounts) && accounts.size() == static_cast<size_t>(ACCOUNT_COUNT)
              && loadTransactions(txns) && txns.size() == static_cast<size_t>(seededTxns),
          "Seed data loads through real storage: " + std::to_string(ACCOUNT_COUNT)
              + " accounts, " + std::to_string(seededTxns) + " transactions");
    std::cout << "  accounts.txt " << fs::file_size(fs::path("data") / "accounts.txt", ec)
              << " bytes, transactions.txt "
              << fs::file_size(fs::path("data") / "transactions.txt", ec) << " bytes\n\n";

    std::cout << "Timings (whole console flow: login check, validation, file read/write, output):\n";
    timeOperation("Deposit", depositMoney, "500\n", "Deposit successful");
    timeOperation("Withdrawal", withdrawMoney, "200\n", "Withdrawal successful");
    timeOperation("Balance enquiry", balanceEnquiry, "", "Current balance");

    // Every timed deposit and withdrawal must have reached disk exactly once.
    txns.clear();
    check(loadTransactions(txns) && txns.size() == static_cast<size_t>(seededTxns + 2 * RUNS),
          "All " + std::to_string(2 * RUNS) + " timed deposits and withdrawals were saved");

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
