
#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include <string>
#include "models.h"

// Transaction type values
const std::string TXN_DEPOSIT = "DEPOSIT";
const std::string TXN_WITHDRAWAL = "WITHDRAW";

struct DepositResult {
    bool success = false;
    long long newBalance = 0;   // paise; set only on success
    int transactionId = 0;      // set only on success
    std::string message;
};

struct WithdrawalResult {
    bool success = false;
    long long newBalance = 0;   // paise; set only on success
    int transactionId = 0;      // set only on success
    std::string message;
};

// FR-3: caller must authenticate the customer before calling deposit().
DepositResult deposit(int accountNo, long long amountPaise);
bool depositMoney();

// FR-4: caller must authenticate the customer before calling withdraw().
WithdrawalResult withdraw(int accountNo, long long amountPaise);
bool withdrawMoney();

#endif
