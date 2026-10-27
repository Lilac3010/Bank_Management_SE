#include "storage.h"

#include <climits>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Data files, relative to the folder the program is started from.
const char* const DATA_DIR = "data";
const std::string ACCOUNTS_HEADER = "BMS-ACCOUNTS 1";
const std::string TRANSACTIONS_HEADER = "BMS-TRANSACTIONS 1";
const size_t ACCOUNT_FIELDS = 7;
const size_t TRANSACTION_FIELDS = 5;

fs::path accountsPath() { return fs::path(DATA_DIR) / "accounts.txt"; }
fs::path transactionsPath() { return fs::path(DATA_DIR) / "transactions.txt"; }

enum class ReadResult { Missing, Ok, Failed };

// Text fields escape '\', '|', newline and carriage return so any string round-trips.
std::string escapeField(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '|':  out += "\\|"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default:   out += c;
        }
    }
    return out;
}

// Splits one record line on unescaped '|' and unescapes each field.
// Returns false on an invalid or unfinished escape sequence.
bool splitFields(const std::string& line, std::vector<std::string>& fields) {
    fields.assign(1, "");
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '|') {
            fields.emplace_back();
        } else if (c != '\\') {
            fields.back() += c;
        } else {
            if (++i == line.size()) return false;
            switch (line[i]) {
                case '\\': fields.back() += '\\'; break;
                case '|':  fields.back() += '|'; break;
                case 'n':  fields.back() += '\n'; break;
                case 'r':  fields.back() += '\r'; break;
                default:   return false;
            }
        }
    }
    return true;
}

// Plain decimal integer (optional leading '-') within [minValue, maxValue].
bool parseInteger(const std::string& text, long long minValue, long long maxValue,
                  long long& value) {
    size_t start = (!text.empty() && text[0] == '-') ? 1 : 0;
    if (start == text.size()) return false;
    for (size_t i = start; i < text.size(); ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    try {
        long long parsed = std::stoll(text);
        if (parsed < minValue || parsed > maxValue) return false;
        value = parsed;
        return true;
    } catch (const std::exception&) {
        return false;   // out of range for long long
    }
}

// Reads the record lines after the header. Missing file -> Missing.
// Unreadable file, wrong header or blank line -> Failed.
ReadResult readRecordLines(const fs::path& path, const std::string& header,
                           std::vector<std::string>& lines) {
    std::error_code ec;
    fs::file_status status = fs::status(path, ec);
    if (status.type() == fs::file_type::not_found) return ReadResult::Missing;
    if (ec || !fs::is_regular_file(status)) return ReadResult::Failed;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return ReadResult::Failed;

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            if (line != header) return ReadResult::Failed;
            first = false;
        } else {
            if (line.empty()) return ReadResult::Failed;
            lines.push_back(line);
        }
    }
    if (in.bad() || first) return ReadResult::Failed;   // read error, or empty file
    return ReadResult::Ok;
}

bool parseAccount(const std::string& line, Account& account) {
    std::vector<std::string> f;
    long long number = 0, balance = 0;
    if (!splitFields(line, f) || f.size() != ACCOUNT_FIELDS) return false;
    if (!parseInteger(f[0], 1, INT_MAX, number)) return false;
    if (!parseInteger(f[5], LLONG_MIN, LLONG_MAX, balance)) return false;
    account.accountNumber = static_cast<int>(number);
    account.name = f[1];
    account.address = f[2];
    account.contactNumber = f[3];
    account.pin = f[4];
    account.balance = balance;
    account.status = f[6];
    return true;
}

bool parseTransaction(const std::string& line, Transaction& txn) {
    std::vector<std::string> f;
    long long id = 0, number = 0, amount = 0;
    if (!splitFields(line, f) || f.size() != TRANSACTION_FIELDS) return false;
    if (!parseInteger(f[0], 1, INT_MAX, id)) return false;
    if (!parseInteger(f[1], 1, INT_MAX, number)) return false;
    if (!parseInteger(f[3], LLONG_MIN, LLONG_MAX, amount)) return false;
    txn.transactionId = static_cast<int>(id);
    txn.accountNumber = static_cast<int>(number);
    txn.type = f[2];
    txn.amount = amount;
    txn.dateTime = f[4];
    return true;
}

// Loads every account, rejecting the whole file on any bad or duplicate record.
ReadResult readAccounts(std::vector<Account>& accounts) {
    std::vector<std::string> lines;
    ReadResult result = readRecordLines(accountsPath(), ACCOUNTS_HEADER, lines);
    if (result != ReadResult::Ok) return result;

    std::set<int> numbers;
    for (const std::string& line : lines) {
        Account account;
        if (!parseAccount(line, account) || !numbers.insert(account.accountNumber).second)
            return ReadResult::Failed;
        accounts.push_back(account);
    }
    return ReadResult::Ok;
}

// Loads every transaction, rejecting the whole file on any bad or duplicate record.
ReadResult readTransactions(std::vector<Transaction>& transactions) {
    std::vector<std::string> lines;
    ReadResult result = readRecordLines(transactionsPath(), TRANSACTIONS_HEADER, lines);
    if (result != ReadResult::Ok) return result;

    std::set<int> ids;
    for (const std::string& line : lines) {
        Transaction txn;
        if (!parseTransaction(line, txn) || !ids.insert(txn.transactionId).second)
            return ReadResult::Failed;
        transactions.push_back(txn);
    }
    return ReadResult::Ok;
}

std::string accountLine(const Account& a) {
    return std::to_string(a.accountNumber) + "|" + escapeField(a.name) + "|"
           + escapeField(a.address) + "|" + escapeField(a.contactNumber) + "|"
           + escapeField(a.pin) + "|" + std::to_string(a.balance) + "|"
           + escapeField(a.status);
}

std::string transactionLine(const Transaction& t) {
    return std::to_string(t.transactionId) + "|" + std::to_string(t.accountNumber) + "|"
           + escapeField(t.type) + "|" + std::to_string(t.amount) + "|"
           + escapeField(t.dateTime);
}

void removeTempFile(const fs::path& temp) {
    std::error_code ec;
    if (fs::is_regular_file(temp, ec)) fs::remove(temp, ec);
}

// Writes the whole file to "<path>.tmp", then replaces `path` with it. The original
// file is left untouched unless the final replace succeeds.
bool writeFileAtomically(const fs::path& path, const std::string& header,
                         const std::vector<std::string>& lines) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;

    fs::path temp = path;
    temp += ".tmp";
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << header << '\n';
    for (const std::string& line : lines) out << line << '\n';
    out.close();
    if (out.fail()) {
        removeTempFile(temp);
        return false;
    }

    fs::rename(temp, path, ec);
    if (ec) {
        removeTempFile(temp);
        return false;
    }
    return true;
}

} // namespace

bool loadAccounts(std::vector<Account>& accounts) {
    std::vector<Account> loaded;
    if (readAccounts(loaded) == ReadResult::Failed) return false;
    accounts = std::move(loaded);
    return true;
}

bool saveAccount(const Account& account) {
    if (account.accountNumber <= 0) return false;

    // Never overwrite a file that cannot be read back.
    std::vector<Account> accounts;
    if (readAccounts(accounts) == ReadResult::Failed) return false;

    bool replaced = false;
    for (Account& existing : accounts) {
        if (existing.accountNumber == account.accountNumber) {
            existing = account;
            replaced = true;
            break;
        }
    }
    if (!replaced) accounts.push_back(account);

    std::vector<std::string> lines;
    for (const Account& a : accounts) lines.push_back(accountLine(a));
    return writeFileAtomically(accountsPath(), ACCOUNTS_HEADER, lines);
}

bool loadTransactions(std::vector<Transaction>& transactions) {
    std::vector<Transaction> loaded;
    if (readTransactions(loaded) == ReadResult::Failed) return false;
    transactions = std::move(loaded);
    return true;
}

bool saveTransaction(const Transaction& txn) {
    if (txn.transactionId <= 0 || txn.accountNumber <= 0) return false;

    std::vector<Transaction> transactions;
    if (readTransactions(transactions) == ReadResult::Failed) return false;
    for (const Transaction& existing : transactions)
        if (existing.transactionId == txn.transactionId) return false;

    std::vector<std::string> lines;
    for (const Transaction& t : transactions) lines.push_back(transactionLine(t));
    lines.push_back(transactionLine(txn));
    return writeFileAtomically(transactionsPath(), TRANSACTIONS_HEADER, lines);
}
