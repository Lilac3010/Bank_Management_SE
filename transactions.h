#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include <string>
#include "models.h"

// Transaction type values
const std::string TXN_DEPOSIT = "DEPOSIT";

struct DepositResult {
    bool success = false;
    long long newBalance = 0;   // paise; set only on success
    int transactionId = 0;      // set only on success
    std::string message;
};

// FR-3 - adds amountPaise to an ACTIVE account and records a DEPOSIT transaction.
// The caller must already have authenticated the customer (see depositMoney).
// Nothing is changed unless every check passes.
DepositResult deposit(int accountNo, long long amountPaise);

// FR-3 - console flow: account number + PIN (authenticateCustomer), then amount.
bool depositMoney();

#endif
