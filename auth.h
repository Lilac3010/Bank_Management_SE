#ifndef AUTH_H
#define AUTH_H

#include <string>

// FR-10 / FR-11 - implemented in auth.cpp.
// loginEmployee: console flow; asks for a username and password (the password is not echoed
//   at a real console) and checks them against data/employees.txt. Any failure logs out.
bool loginEmployee();

// FR-11 - true only if a stored ACTIVE account has exactly this number (> 0) and this PIN.
//   Reads the accounts through storage every time; an unreadable accounts file gives false.
//   Prints nothing and saves nothing. There is no customer session: each self-service flow
//   (deposit, withdrawal, balance enquiry, mini-statement) asks for the account number and PIN
//   and calls this before it shows any data or asks for an amount.
bool authenticateCustomer(int accountNo, const std::string& pin);

// Integration point used by the employee features (FR-1, FR-7, FR-8, FR-9).
// True only after a successful loginEmployee(), until logoutEmployee() or a failed login.
bool isEmployeeAuthenticated();

// FR-10 - ends the employee session. Called by the menus when the Employee Menu is left.
void logoutEmployee();

// FR-10 - employee setup (bank.exe --add-employee). These checks are enforced here, whoever
// calls them, and neither function changes the login session:
//   createFirstEmployee: only when NO employee record exists. An unreadable or corrupt
//     employees file is never treated as "no employee".
//   addEmployee: only after `existingUsername` / `existingPassword` are verified against the
//     stored records in this call; a login made earlier is not enough.
// Both check the new username (3-32 of letters, digits, '.', '_', '-'; case-sensitive) and
// password (8-128 characters, no line breaks), reject a username already stored, and store
// only a PBKDF2 hash of the password. Existing records are never replaced.
struct EmployeeSetupResult {
    bool success = false;
    std::string message;
};
EmployeeSetupResult createFirstEmployee(const std::string& username, const std::string& password);
EmployeeSetupResult addEmployee(const std::string& existingUsername,
                                const std::string& existingPassword,
                                const std::string& username, const std::string& password);

// FR-10 - console flow for bank.exe --add-employee. Returns true only if an employee was added.
bool employeeSetup();

#endif
