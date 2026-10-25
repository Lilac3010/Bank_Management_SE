#include "balance_enquiry.h"
#include "auth.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

namespace {

// trim, isDigits and parseAccountNumber follow the same rules as the helpers in
// transactions.cpp (FR-3/FR-4). They are copied rather than shared so those files
// stay unchanged; moving them into a shared helper is a later refactor.
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

// Accepts digits only, for a positive number that fits in an int.
bool parseAccountNumber(const std::string& text, int& accountNo) {
    std::string t = trim(text);
    if (t.empty() || t.size() > 10 || !isDigits(t, t.size())) return false;
    long long value = std::stoll(t);
    if (value <= 0 || value > INT_MAX) return false;
    accountNo = static_cast<int>(value);
    return true;
}

// Formats paise as rupees from the integer value, e.g. 125050 -> "1250.50".
// Works on the unsigned magnitude so every long long, including LLONG_MIN, is safe.
std::string formatRupees(long long paise) {
    bool negative = paise < 0;
    unsigned long long magnitude = negative ? 0ULL - static_cast<unsigned long long>(paise)
                                            : static_cast<unsigned long long>(paise);
    std::string fraction = std::to_string(magnitude % 100);
    if (fraction.size() < 2) fraction = "0" + fraction;
    return (negative ? "-" : "") + std::to_string(magnitude / 100) + "." + fraction;
}

} // namespace

bool balanceEnquiry() {
    std::string accountText, pin;
    std::cout << "\n--- Balance Enquiry ---\n";
    std::cout << "Account number: ";
    std::getline(std::cin, accountText);

    int accountNo = 0;
    if (!parseAccountNumber(accountText, accountNo)) {
        std::cout << "Error: Account number must be a positive whole number.\n";
        return false;
    }

    std::cout << "PIN: ";
    std::getline(std::cin, pin);

    if (!authenticateCustomer(accountNo, trim(pin))) {
        std::cout << "Access denied: invalid account number or PIN.\n";
        return false;
    }

    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) {
        std::cout << "Storage error: could not read account data.\n";
        return false;
    }

    const Account* found = nullptr;
    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            found = &account;
            break;
        }
    }
    if (found == nullptr) {
        std::cout << "Error: Account not found.\n";
        return false;
    }
    if (found->status == STATUS_CLOSED) {
        std::cout << "Error: Account is closed.\n";
        return false;
    }
    if (found->status != STATUS_ACTIVE) {
        std::cout << "Error: Account is not active.\n";
        return false;
    }

    std::cout << "Current balance for account " << accountNo << ": Rs. "
              << formatRupees(found->balance) << "\n";
    return true;
}
