#ifndef STORAGE_H
#define STORAGE_H

#include <vector>
#include "models.h"

// FR-13 - persistent storage, implemented in storage.cpp.
// Files: data/accounts.txt and data/transactions.txt, relative to the folder the
//   program is started from. data/ is created on the first save; loading never creates files.
// Any load failure leaves the caller's vector unchanged. A save never overwrites a data
//   file that cannot be read back, and a failed save leaves the existing file unchanged.
//
// loadAccounts: fills `accounts` with all stored accounts and returns true.
//   No data file yet -> true with an empty list.
//   File exists but cannot be read or parsed -> false.
// saveAccount: insert or update by accountNumber (> 0), keeping record order; records are
//   never deleted, so CLOSED accounts stay stored; false on any I/O failure.
bool loadAccounts(std::vector<Account>& accounts);
bool saveAccount(const Account& account);

// loadTransactions: fills `transactions` with all stored transactions (every account,
//   oldest first) and returns true.
//   No data file yet -> true with an empty list.
//   File exists but cannot be read or parsed -> false.
//   Used by FR-3 to pick the next transactionId (highest + 1), and later by FR-6.
// saveTransaction: appends one transaction exactly as given (the caller sets
//   transactionId); never rewrites earlier entries; false on any I/O failure, or if
//   transactionId / accountNumber is <= 0 or the transactionId is already stored.
bool loadTransactions(std::vector<Transaction>& transactions);
bool saveTransaction(const Transaction& txn);

#endif
