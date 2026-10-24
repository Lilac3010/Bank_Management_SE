#ifndef STORAGE_H
#define STORAGE_H

#include <vector>
#include "models.h"

// FR-13 - persistent storage.
// loadAccounts: fills `accounts` with all stored accounts and returns true.
//   No data file yet -> true with an empty list.
//   File exists but cannot be read or parsed -> false.
// saveAccount: insert or update by accountNumber; false on any I/O failure.
bool loadAccounts(std::vector<Account>& accounts);
bool saveAccount(const Account& account);

// loadTransactions: fills `transactions` with all stored transactions (every account,
//   oldest first) and returns true.
//   No data file yet -> true with an empty list.
//   File exists but cannot be read or parsed -> false.
//   Used by FR-3 to pick the next transactionId (highest + 1), and later by FR-6.
// saveTransaction: appends one transaction exactly as given (the caller sets
//   transactionId); never rewrites earlier entries; false on any I/O failure.
bool loadTransactions(std::vector<Transaction>& transactions);
bool saveTransaction(const Transaction& txn);

#endif
