#ifndef ACCOUNT_CLOSURE_H
#define ACCOUNT_CLOSURE_H

#include <string>

struct AccountClosureResult {
    bool success = false;
    std::string message;
};

// FR-9: Close an existing account.
AccountClosureResult closeAccount(int accountNo);

// FR-9 console flow for an authenticated employee.
bool accountClosure();

#endif