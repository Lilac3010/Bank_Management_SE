#include "account.h"
#include "auth.h"
#include "storage.h"

#include <cctype>
#include <iostream>

namespace {

// Rupee amounts with more digits than this are rejected so paise fit in a long long.
const size_t MAX_RUPEE_DIGITS = 15;

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

bool isValidName(const std::string& s) {
    bool hasLetter = false;
    for (char c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        if (std::isalpha(u)) hasLetter = true;
        else if (c != ' ' && c != '.' && c != '\'') return false;
    }
    return hasLetter;
}

// Returns an error message, or "" when all details are valid.
std::string validateDetails(const std::string& name, const std::string& address,
                            const std::string& contactNumber, const std::string& pin,
                            long long initialDepositPaise) {
    if (name.empty()) return "Name cannot be empty.";
    if (!isValidName(name)) return "Name may contain only letters, spaces, '.' and apostrophes.";
    if (address.empty()) return "Address cannot be empty.";
    if (address.find('|') != std::string::npos) return "Address cannot contain the '|' character.";
    if (!isDigits(contactNumber, 10)) return "Contact number must be exactly 10 digits.";
    if (!isDigits(pin, 4)) return "PIN must be exactly 4 digits.";
    if (initialDepositPaise <= 0) return "Initial deposit must be a positive amount.";
    return "";
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

} // namespace

CreateAccountResult createAccountWithDetails(const std::string& name,
                                             const std::string& address,
                                             const std::string& contactNumber,
                                             const std::string& pin,
                                             long long initialDepositPaise) {
    CreateAccountResult result;

    if (!isEmployeeAuthenticated()) {
        result.message = "Access denied: employee login required.";
        return result;
    }

    Account account;
    account.name = trim(name);
    account.address = trim(address);
    account.contactNumber = trim(contactNumber);
    account.pin = trim(pin);

    std::string error = validateDetails(account.name, account.address,
                                        account.contactNumber, account.pin, initialDepositPaise);
    if (!error.empty()) {
        result.message = "Error: " + error;
        return result;
    }

    std::vector<Account> accounts;
    if (!loadAccounts(accounts)) {
        result.message = "Storage error: could not read account data. Account was not created.";
        return result;
    }

    for (const Account& existing : accounts) {
        if (existing.status == STATUS_ACTIVE && existing.contactNumber == account.contactNumber) {
            result.message = "Error: An active account with this contact number already exists.";
            return result;
        }
    }

    // saveAccount inserts or updates by number, so a reused number would overwrite an account.
    account.accountNumber = generateAccountNumber(accounts);
    bool numberInUse = account.accountNumber <= 0;
    for (const Account& existing : accounts)
        if (existing.accountNumber == account.accountNumber) numberInUse = true;
    if (numberInUse) {
        result.message = "Error: Could not generate a unique account number. Account was not created.";
        return result;
    }

    account.balance = initialDepositPaise;
    account.status = STATUS_ACTIVE;

    if (!saveAccount(account)) {
        result.message = "Storage error: account was not created.";
        return result;
    }

    result.success = true;
    result.accountNumber = account.accountNumber;
    result.message = "Account created successfully. Account number: "
                     + std::to_string(account.accountNumber)
                     + ". Opening balance: Rs. " + formatRupees(account.balance);
    return result;
}

bool createAccount() {
    if (!isEmployeeAuthenticated()) {
        std::cout << "Access denied: employee login required.\n";
        return false;
    }

    std::string name, address, contactNumber, pin, depositText;
    std::cout << "\n--- Create Customer Account ---\n";
    std::cout << "Customer name: ";
    std::getline(std::cin, name);
    std::cout << "Address: ";
    std::getline(std::cin, address);
    std::cout << "Contact number (10 digits): ";
    std::getline(std::cin, contactNumber);
    std::cout << "PIN (4 digits): ";
    std::getline(std::cin, pin);
    std::cout << "Initial deposit (Rs.): ";
    std::getline(std::cin, depositText);

    long long initialDepositPaise = 0;
    if (!parseRupees(depositText, initialDepositPaise)) {
        std::cout << "Error: Initial deposit must be an amount like 1000 or 1000.50.\n";
        return false;
    }

    CreateAccountResult result = createAccountWithDetails(name, address, contactNumber,
                                                          pin, initialDepositPaise);
    std::cout << result.message << "\n";
    return result.success;
}
