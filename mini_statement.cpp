
#include "mini_statement.h"
#include "auth.h"
#include "storage.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <climits>
#include <string>
#include <vector>

namespace {

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool parseAccountNumber(const std::string& text, int& accountNo) {
    const std::string value = trim(text);

    if (value.empty() || value.size() > 10) return false;

    for (char c : value) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    try {
        const long long number = std::stoll(value);

        if (number <= 0 || number > INT_MAX) return false;

        accountNo = static_cast<int>(number);
        return true;
    } catch (...) {
        return false;
    }
}

std::string formatRupees(long long paise) {
    const bool negative = paise < 0;

    // Avoid overflowing when finding the magnitude of LLONG_MIN.
    const unsigned long long magnitude =
        negative ? 0ULL - static_cast<unsigned long long>(paise)
                 : static_cast<unsigned long long>(paise);

    std::string fraction = std::to_string(magnitude % 100);
    if (fraction.size() < 2) fraction = "0" + fraction;

    return (negative ? "-" : "") +
           std::to_string(magnitude / 100) + "." + fraction;
}

} // namespace

bool miniStatement() {
    std::string accountText;
    std::string pin;

    std::cout << "\n--- Mini-Statement ---\n";
    std::cout << "Account number: ";

    if (!std::getline(std::cin, accountText)) return false;

    int accountNo = 0;

    if (!parseAccountNumber(accountText, accountNo)) {
        std::cout << "Error: Account number must be a positive whole number.\n";
        return false;
    }

    std::cout << "PIN: ";

    if (!std::getline(std::cin, pin)) return false;

    if (!authenticateCustomer(accountNo, trim(pin))) {
        std::cout << "Access denied: invalid account number or PIN.\n";
        return false;
    }

    std::vector<Transaction> transactions;

    if (!loadTransactions(transactions)) {
        std::cout << "Storage error: could not read transaction data.\n";
        return false;
    }

    std::vector<Transaction> customerTransactions;

    for (const Transaction& txn : transactions) {
        if (txn.accountNumber == accountNo) {
            customerTransactions.push_back(txn);
        }
    }

    if (customerTransactions.empty()) {
        std::cout << "No transactions found for this account.\n";
        return true;
    }

    // Transaction IDs increase as transactions are created.
    std::sort(customerTransactions.begin(), customerTransactions.end(),
              [](const Transaction& a, const Transaction& b) {
                  return a.transactionId > b.transactionId;
              });

    const size_t count =
        std::min<size_t>(5, customerTransactions.size());

    std::cout << "\nRecent transactions for account "
              << accountNo << ":\n";

    std::cout << std::left
              << std::setw(8) << "ID"
              << std::setw(14) << "Type"
              << std::setw(16) << "Amount (Rs.)"
              << "Date/Time\n";

    std::cout << std::string(58, '-') << '\n';

    for (size_t i = 0; i < count; ++i) {
        const Transaction& txn = customerTransactions[i];

        std::cout << std::left
                  << std::setw(8) << txn.transactionId
                  << std::setw(14) << txn.type
                  << std::setw(16) << formatRupees(txn.amount)
                  << txn.dateTime << '\n';
    }

    return true;
}
