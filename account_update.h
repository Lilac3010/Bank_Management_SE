#ifndef ACCOUNT_UPDATE_H
#define ACCOUNT_UPDATE_H

#include <string>

struct UpdateAccountResult {
    bool success = false;
    std::string message;
};

// FR-8: update permitted customer details for an authenticated employee.
UpdateAccountResult updateAccountDetails(int accountNo,
                                         const std::string& name,
                                         const std::string& address,
                                         const std::string& contactNumber);

// FR-8 console flow.
bool accountUpdate();

#endif