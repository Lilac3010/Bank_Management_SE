#ifndef AUTH_H
#define AUTH_H

#include <string>

// FR-10 / FR-11 - implemented in auth.cpp by the authentication owner.
bool loginEmployee();
bool authenticateCustomer(int accountNo, const std::string& pin);

// Integration point used by FR-1 (pending agreement with the FR-10 owner).
// True only after a successful loginEmployee().
bool isEmployeeAuthenticated();

#endif
