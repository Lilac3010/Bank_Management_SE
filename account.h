#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>
#include "models.h"

struct CreateAccountResult {
    bool success = false;
    int accountNumber = 0;
    std::string message;
};

// FR-2 - returns an account number not used by any account in `existing`,
// or 0 if no such number can be generated. Implemented in account_number.cpp.
int generateAccountNumber(const std::vector<Account>& existing);

// FR-1 - validates and creates an account. Requires an authenticated employee.
// initialDepositPaise is in paise (Rs. 1000.50 = 100050).
// Nothing is saved unless every check passes.
CreateAccountResult createAccountWithDetails(const std::string& name,
                                             const std::string& address,
                                             const std::string& contactNumber,
                                             const std::string& pin,
                                             long long initialDepositPaise);

// FR-1 - console flow: prompts for details, then calls createAccountWithDetails.
bool createAccount();

#endif
