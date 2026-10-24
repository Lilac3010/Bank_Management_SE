//This defines how the application reads and saves account and transaction records.
#ifndef STORAGE_H
#define STORAGE_H

#include "models.h"
#include <vector>

bool loadAccounts(std::vector<Account>& accounts);
bool saveAccount(const Account& account);

bool loadTransactions(std::vector<Transaction>& transactions);
bool saveTransaction(const Transaction& transaction);

#endif