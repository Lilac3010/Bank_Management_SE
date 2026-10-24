#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include "models.h"
#include <string>
#include <vector>

bool deposit(
    const std::string& accountNo,
    double amount,
    std::vector<Account>& accounts,
    std::vector<Transaction>& transactions
);

bool withdraw(
    const std::string& accountNo,
    double amount,
    std::vector<Account>& accounts,
    std::vector<Transaction>& transactions
);

double getBalance(
    const std::string& accountNo,
    const std::vector<Account>& accounts
);

std::vector<Transaction> getMiniStatement(
    const std::string& accountNo,
    const std::vector<Transaction>& transactions
);

#endif