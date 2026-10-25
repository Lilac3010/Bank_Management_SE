
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "auth.h"
#include "mini_statement.h"
#include "storage.h"
#include "models.h"

// ===================== TEST-ONLY FAKES =====================
namespace fake {
    std::vector<Transaction> transactions;
    bool forceAuthFailure = false;
    bool failLoadTransactions = false;
    int authCalls = 0;
    int loadCalls = 0;
    int lastAuthAccount = 0;
    std::string lastAuthPin;
}

bool authenticateCustomer(int accountNo, const std::string& pin) {
    ++fake::authCalls;
    fake::lastAuthAccount = accountNo;
    fake::lastAuthPin = pin;

    return !fake::forceAuthFailure &&
           accountNo == 100001 &&
           pin == "1234";
}

bool loadTransactions(std::vector<Transaction>& transactions) {
    ++fake::loadCalls;

    if (fake::failLoadTransactions) return false;

    transactions = fake::transactions;
    return true;
}
// =================== END TEST-ONLY FAKES ===================

namespace {
int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) {
        ++passed;
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failed;
        std::cout << "[FAIL] " << name << '\n';
    }
}

Transaction makeTransaction(int id, int accountNo,
                            const std::string& type,
                            long long amount,
                            const std::string& dateTime) {
    Transaction txn;
    txn.transactionId = id;
    txn.accountNumber = accountNo;
    txn.type = type;
    txn.amount = amount;
    txn.dateTime = dateTime;
    return txn;
}

void reset() {
    fake::transactions = {
        makeTransaction(1, 100001, "DEPOSIT", 50000, "2026-10-20 10:00:00"),
        makeTransaction(2, 200002, "WITHDRAW", 20000, "2026-10-21 11:00:00"),
        makeTransaction(3, 100001, "WITHDRAW", 10000, "2026-10-22 12:00:00"),
        makeTransaction(4, 100001, "DEPOSIT", 25000, "2026-10-23 13:00:00"),
        makeTransaction(5, 100001, "WITHDRAW", 5000, "2026-10-24 14:00:00"),
        makeTransaction(6, 100001, "DEPOSIT", 70000, "2026-10-25 15:00:00"),
        makeTransaction(7, 100001, "DEPOSIT", 80000, "2026-10-25 16:00:00")
    };

    fake::forceAuthFailure = false;
    fake::failLoadTransactions = false;
    fake::authCalls = 0;
    fake::loadCalls = 0;
    fake::lastAuthAccount = 0;
    fake::lastAuthPin.clear();
}

bool runStatement(const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;

    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());

    bool result = miniStatement();

    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);

    output = out.str();
    return result;
}

void testValidStatement() {
    reset();
    std::string output;

    bool result = runStatement("100001\n1234\n", output);

    check(result, "Valid customer can view mini-statement");
    check(fake::authCalls == 1, "Customer authentication is called");
    check(fake::lastAuthAccount == 100001 &&
          fake::lastAuthPin == "1234",
          "Correct account number and PIN are passed to authentication");
    check(fake::loadCalls == 1, "Transactions are loaded after authentication");
}

void testAccountFiltering() {
    reset();
    std::string output;

    runStatement("100001\n1234\n", output);

    check(output.find("200002") == std::string::npos,
          "Other customer's account number is not displayed");
    check(output.find("WITHDRAW") != std::string::npos,
          "Customer transaction types are displayed");
}

void testRecentFive() {
    reset();
    std::string output;

    runStatement("100001\n1234\n", output);

    // The five newest IDs for account 100001 should be 7, 6, 5, 4, 3.
    size_t p7 = output.find("7");
    size_t p6 = output.find("6");
    size_t p5 = output.find("5");
    size_t p4 = output.find("4");
    size_t p3 = output.find("3");

    check(p7 != std::string::npos &&
          p6 != std::string::npos &&
          p5 != std::string::npos &&
          p4 != std::string::npos &&
          p3 != std::string::npos,
          "Recent transaction IDs are displayed");

    // Check the actual ID columns using their complete row prefixes.
    size_t row7 = output.find("7       DEPOSIT");
    size_t row6 = output.find("6       DEPOSIT");
    size_t row5 = output.find("5       WITHDRAW");
    size_t row4 = output.find("4       DEPOSIT");
    size_t row3 = output.find("3       WITHDRAW");

    check(row7 < row6 && row6 < row5 &&
          row5 < row4 && row4 < row3,
          "Transactions are shown newest ID first");

    check(output.find("1       DEPOSIT") == std::string::npos,
          "Older transactions beyond the five-item limit are excluded");
}

void testNoTransactions() {
    reset();
    fake::transactions.clear();

    std::string output;
    bool result = runStatement("100001\n1234\n", output);

    check(result, "Empty history is handled successfully");
    check(output.find("No transactions found") != std::string::npos,
          "Empty history displays an informative message");
}

void testInvalidAccount() {
    reset();
    std::string output;

    bool result = runStatement("abc\n1234\n", output);

    check(!result, "Invalid account number is rejected");
    check(fake::authCalls == 0,
          "Invalid account number does not call authentication");
    check(fake::loadCalls == 0,
          "Invalid account number does not load transactions");
}

void testWrongPin() {
    reset();
    std::string output;

    bool result = runStatement("100001\n9999\n", output);

    check(!result, "Incorrect PIN is rejected");
    check(output.find("Access denied") != std::string::npos,
          "Authentication failure displays an access-denied message");
    check(fake::loadCalls == 0,
          "Transactions are not loaded after failed authentication");
}

void testStorageFailure() {
    reset();
    fake::failLoadTransactions = true;

    std::string output;
    bool result = runStatement("100001\n1234\n", output);

    check(!result, "Storage failure returns false");
    check(output.find("Storage error") != std::string::npos,
          "Storage failure displays an error message");
}

} // namespace

int main() {
    testValidStatement();
    testAccountFiltering();
    testRecentFive();
    testNoTransactions();
    testInvalidAccount();
    testWrongPin();
    testStorageFailure();

    std::cout << "\n============================\n";
    std::cout << "Passed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';
    std::cout << "============================\n";

    return failed == 0 ? 0 : 1;
}
