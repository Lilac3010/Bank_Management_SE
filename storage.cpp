#include "storage.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

bool loadAccounts(std::vector<Account>& accounts) {
    std::ifstream file("accounts.txt");

    if (!file.is_open()) {
        // A missing file means no accounts have been saved yet.
        std::ofstream createFile("accounts.txt");
        if (!createFile.is_open()) {
            return false;
        }
        accounts.clear();
        return true;
    }

    std::vector<Account> loaded;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        Account account;
        std::string balance;

        if (!std::getline(ss, account.accountNumber, '|') ||
            !std::getline(ss, account.name, '|') ||
            !std::getline(ss, account.address, '|') ||
            !std::getline(ss, account.contactNumber, '|') ||
            !std::getline(ss, account.pin, '|') ||
            !std::getline(ss, balance, '|') ||
            !std::getline(ss, account.status)) {
            return false;
        }

        try {
            std::size_t pos = 0;
            account.balance = std::stod(balance, &pos);
            if (pos != balance.size() ||
                account.balance < 0.0) {
                return false;
            }
        } catch (...) {
            return false;
        }

        loaded.push_back(account);
    }

    if (file.bad()) {
        return false;
    }

    accounts = loaded;
    return true;
}

bool saveAccount(const Account& account) {
    std::vector<Account> accounts;

    if (!loadAccounts(accounts)) {
        return false;
    }

    bool found = false;

    for (Account& existing : accounts) {
        if (existing.accountNumber == account.accountNumber) {
            existing = account;
            found = true;
            break;
        }
    }

    if (!found) {
        accounts.push_back(account);
    }

    const std::string tempFile = "accounts.tmp";

    {
        std::ofstream file(tempFile, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }

        file << std::fixed << std::setprecision(2);

        for (const Account& item : accounts) {
            file << item.accountNumber << '|'
                 << item.name << '|'
                 << item.address << '|'
                 << item.contactNumber << '|'
                 << item.pin << '|'
                 << item.balance << '|'
                 << item.status << '\n';
        }

        file.flush();
        if (!file) {
            file.close();
            std::remove(tempFile.c_str());
            return false;
        }
    }

    // Rename the completed temporary file into place.
    // Do not delete the original first: a failed rename
    // should not destroy the existing account file.
   if (!MoveFileExA(
        tempFile.c_str(),
        "accounts.txt",
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    )) {
    std::remove(tempFile.c_str());
    return false;
}

return true;
   
   
}

bool loadTransactions(
    std::vector<Transaction>& transactions
) {
    std::ifstream file("transactions.txt");

    if (!file.is_open()) {
        std::ofstream createFile("transactions.txt");
        if (!createFile.is_open()) {
            return false;
        }
        transactions.clear();
        return true;
    }

    std::vector<Transaction> loaded;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        Transaction transaction;
        std::string amount;
        std::string balance;

        if (!std::getline(ss, transaction.transactionId, '|') ||
            !std::getline(ss, transaction.accountNumber, '|') ||
            !std::getline(ss, transaction.type, '|') ||
            !std::getline(ss, amount, '|') ||
            !std::getline(ss, transaction.dateTime, '|') ||
            !std::getline(ss, balance)) {
            return false;
        }

        try {
            std::size_t pos1 = 0;
            std::size_t pos2 = 0;

            transaction.amount = std::stod(amount, &pos1);
            transaction.resultingBalance =
                std::stod(balance, &pos2);

            if (pos1 != amount.size() ||
                pos2 != balance.size() ||
                transaction.amount <= 0.0 ||
                transaction.resultingBalance < 0.0) {
                return false;
            }
        } catch (...) {
            return false;
        }

        loaded.push_back(transaction);
    }

    if (file.bad()) {
        return false;
    }

    transactions = loaded;
    return true;
}

bool saveTransaction(const Transaction& transaction) {
    if (transaction.amount <= 0.0 ||
        transaction.resultingBalance < 0.0) {
        return false;
    }

    std::ofstream file(
        "transactions.txt",
        std::ios::app
    );

    if (!file.is_open()) {
        return false;
    }

    file << transaction.transactionId << '|'
         << transaction.accountNumber << '|'
         << transaction.type << '|'
         << std::fixed << std::setprecision(2)
         << transaction.amount << '|'
         << transaction.dateTime << '|'
         << transaction.resultingBalance << '\n';

    file.flush();
    return static_cast<bool>(file);
}