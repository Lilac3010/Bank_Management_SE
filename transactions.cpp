#include "transactions.h"
#include "auth.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <ctime>
#include <iostream>
#include <vector>

namespace {

// Same limit as FR-1 account creation, so paise fit in a long long.
const size_t MAX_RUPEE_DIGITS = 15;

// trim, isDigits, parseRupees and formatRupees follow the same rules as the
// helpers in account.cpp (FR-1). They are copied rather than shared so FR-1
// stays unchanged; moving them into a shared money helper is a later refactor.
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool isDigits(const std::string& s, size_t length) {
    if (s.size() != length) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

// Parses a plain rupee amount such as 1000, 1000.5 or 1000.50 into paise.
// Rejects signs, exponents, hex, "inf"/"nan", thousands separators and
// amounts too large to store.
bool parseRupees(const std::string& text, long long& paise) {
    std::string t = trim(text);
    size_t dot = t.find('.');
    std::string whole = t.substr(0, dot);
    std::string fraction = dot == std::string::npos ? "" : t.substr(dot + 1);
    if (whole.empty() || whole.size() > MAX_RUPEE_DIGITS || !isDigits(whole, whole.size()))
        return false;
    if (dot != std::string::npos
        && (fraction.empty() || fraction.size() > 2 || !isDigits(fraction, fraction.size())))
        return false;
    if (fraction.size() == 1) fraction += "0";
    paise = std::stoll(whole) * 100 + (fraction.empty() ? 0 : std::stoll(fraction));
    return true;
}

// Formats paise as rupees, e.g. 100050 -> "1000.50".
std::string formatRupees(long long paise) {
    std::string fraction = std::to_string(paise % 100);
    if (fraction.size() < 2) fraction = "0" + fraction;
    return std::to_string(paise / 100) + "." + fraction;
}

// Accepts digits only, for a positive number that fits in an int.
bool parseAccountNumber(const std::string& text, int& accountNo) {
    std::string t = trim(text);
    if (t.empty() || t.size() > 10 || !isDigits(t, t.size())) return false;
    long long value = std::stoll(t);
    if (value <= 0 || value > INT_MAX) return false;
    accountNo = static_cast<int>(value);
    return true;
}

// Local time as "YYYY-MM-DD HH:MM:SS".
std::string currentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[20];
    std::strftime(buffer, sizeof buffer, "%Y-%m-%d %H:%M:%S", &local);
    return buffer;
}

// Highest existing transactionId + 1 (first ID is 1), or 0 if none is possible.
int nextTransactionId(const std::vector<Transaction>& existing) {
    int highest = 0;
    for (const Transaction& txn : existing)
        if (txn.transactionId > highest) highest = txn.transactionId;
    if (highest == INT_MAX) return 0;
    return highest + 1;
}

} // namespace

DepositResult deposit(int accountNo, long long amountPaise) {
    DepositResult result;

    if (amountPaise <= 0) {
        result.message = "Error: Deposit amount must be positive.";
        return result;
    }

    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) {
        result.message = "Storage error: could not read account data. Deposit not made.";
        return result;
    }

    const Account* found = nullptr;
    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            found = &account;
            break;
        }
    }
    if (found == nullptr) {
        result.message = "Error: Account not found.";
        return result;
    }
    if (found->status == STATUS_CLOSED) {
        result.message = "Error: Account is closed.";
        return result;
    }
    if (found->status != STATUS_ACTIVE) {
        result.message = "Error: Account is not active.";
        return result;
    }
    if (amountPaise > LLONG_MAX - found->balance) {
        result.message = "Error: Deposit would exceed the maximum balance.";
        return result;
    }

    std::vector<Transaction> transactions;
    if (!loadTransactions(transactions)) {
        result.message = "Storage error: could not read transaction data. Deposit not made.";
        return result;
    }
    int transactionId = nextTransactionId(transactions);
    if (transactionId == 0) {
        result.message = "Error: Could not generate a transaction ID. Deposit not made.";
        return result;
    }

    const Account original = *found;
    Account updated = original;
    updated.balance += amountPaise;

    Transaction txn;
    txn.transactionId = transactionId;
    txn.accountNumber = accountNo;
    txn.type = TXN_DEPOSIT;
    txn.amount = amountPaise;
    txn.dateTime = currentDateTime();

    // The storage interface has no combined save, so the account is saved first and
    // restored if the transaction cannot be recorded.
    if (!saveAccount(updated)) {
        result.message = "Storage error: deposit not made.";
        return result;
    }
    if (!saveTransaction(txn)) {
        if (saveAccount(original))
            result.message = "Storage error: transaction could not be recorded. "
                             "Deposit cancelled; balance unchanged.";
        else
            result.message = "Storage error: transaction could not be recorded and the balance "
                             "could not be restored. Account data may be inconsistent.";
        return result;
    }

    result.success = true;
    result.newBalance = updated.balance;
    result.transactionId = transactionId;
    result.message = "Deposit successful. Amount: Rs. " + formatRupees(amountPaise)
                     + ". New balance: Rs. " + formatRupees(updated.balance);
    return result;
}

bool depositMoney() {
    std::string accountText, pin, amountText;
    std::cout << "\n--- Deposit Money ---\n";
    std::cout << "Account number: ";
    std::getline(std::cin, accountText);
    std::cout << "PIN: ";
    std::getline(std::cin, pin);

    int accountNo = 0;
    if (!parseAccountNumber(accountText, accountNo) || !authenticateCustomer(accountNo, trim(pin))) {
        std::cout << "Access denied: invalid account number or PIN.\n";
        return false;
    }

    std::cout << "Deposit amount (Rs.): ";
    std::getline(std::cin, amountText);

    long long amountPaise = 0;
    if (!parseRupees(amountText, amountPaise)) {
        std::cout << "Error: Deposit amount must be an amount like 1000 or 1000.50.\n";
        return false;
    }

    DepositResult result = deposit(accountNo, amountPaise);
    std::cout << result.message << "\n";
    return result.success;
}
