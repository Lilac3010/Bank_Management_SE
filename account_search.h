#ifndef ACCOUNT_SEARCH_H
#define ACCOUNT_SEARCH_H

#include <string>
#include "models.h"

struct SearchAccountResult {
    bool success = false;
    Account account;             // set only on success; pin is always left empty
    std::string message;         // set only on failure
};

// FR-7 - finds the stored account with exactly this number. Requires an authenticated
// employee. Accounts of every status (ACTIVE, CLOSED, ...) are found. Never saves anything.
SearchAccountResult searchAccount(int accountNo);

// FR-7 - console flow: prompts for an account number, then shows that account's details
// (never the PIN). Returns true only when details were shown.
bool accountSearch();

#endif
