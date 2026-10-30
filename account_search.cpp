#include "account_search.h"
#include "auth.h"
#include "storage.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <string>
#include <vector>

namespace {

// trim, isDigits, parseAccountNumber and formatRupees follow the same rules as the helpers
// in balance_enquiry.cpp (FR-5). They are copied rather than shared so those files stay
// unchanged; moving them into a shared helper is a later refactor.
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

const std::string ACCESS_DENIED = "Access denied: employee login required.";
const std::string INVALID_NUMBER = "Error: Account number must be a positive whole number.";

} // namespace

SearchAccountResult searchAccount(int accountNo) {
    SearchAccountResult result;

    if (!isEmployeeAuthenticated()) {
        result.message = ACCESS_DENIED;
        return result;
    }
    if (accountNo <= 0) {
        result.message = INVALID_NUMBER;
        return result;
    }

    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) {
        result.message = "Storage error: could not read account data.";
        return result;
    }

    // Employees may look up accounts of any status, so CLOSED records are found too.
    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo) {
            result.success = true;
            result.account = account;
            result.account.pin.clear();   // the PIN never leaves this module
            return result;
        }
    }

    result.message = "Error: Account not found.";
    return result;
}

bool accountSearch() {
    if (!isEmployeeAuthenticated()) {
        std::cout << ACCESS_DENIED << "\n";
        return false;
    }

    std::string accountText;
    std::cout << "\n--- Search Account ---\n";
    std::cout << "Account number: ";
    if (!std::getline(std::cin, accountText)) return false;

    int accountNo = 0;
    if (!parseAccountNumber(accountText, accountNo)) {
        std::cout << INVALID_NUMBER << "\n";
        return false;
    }

    SearchAccountResult result = searchAccount(accountNo);
    if (!result.success) {
        std::cout << result.message << "\n";
        return false;
    }

    const Account& a = result.account;
    std::cout << "\nAccount number : " << a.accountNumber << "\n"
              << "Name           : " << a.name << "\n"
              << "Address        : " << a.address << "\n"
              << "Contact number : " << a.contactNumber << "\n"
              << "Balance        : Rs. " << formatRupees(a.balance) << "\n"
              << "Status         : " << a.status << "\n";
    return true;
}
