#include "auth.h"

#include "models.h"
#include "password_hash.h"
#include "storage.h"

#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {

bool employeeAuthenticated = false;

const size_t MIN_USERNAME = 3;
const size_t MAX_USERNAME = 32;
const size_t MIN_PASSWORD = 8;
const size_t MAX_PASSWORD = 128;

const std::string LOGIN_FAILED = "Invalid username or password.";
const std::string EMPLOYEE_STORAGE_ERROR = "Storage error: could not read employee data.";
const std::string NO_EMPLOYEES =
    "No employee accounts exist. Run bank.exe --add-employee to create one.";
const std::string SETUP_DENIED = "Access denied: invalid username or password.";
const std::string USERNAME_RULE =
    "Error: Username must be 3 to 32 characters: letters, digits, '.', '_' or '-'.";
const std::string PASSWORD_RULE = "Error: Password must be 8 to 128 characters with no line breaks.";

// A well-formed hash that matches no stored employee. Checking a password against it for an
// unknown username makes that failure take as long as a wrong password, so the response time
// does not reveal which usernames exist. It is not a credential: no record holds it.
const std::string UNKNOWN_USER_HASH =
    "pbkdf2-sha256$600000$5a1e7c0b9d2f4e6a8c0b1d3f5e7a9c2b$"
    "0f1e2d3c4b5a69788796a5b4c3d2e1f00f1e2d3c4b5a69788796a5b4c3d2e1f0";

// std::cin's buffer at program start. Echo is only switched off when std::cin still reads
// from it; tests that redirect std::cin to a string are left alone.
std::streambuf* const ORIGINAL_CIN_BUFFER = std::cin.rdbuf();

// Turns off keyboard echo for its lifetime, only when standard input is a real console.
class EchoOff {
public:
#ifdef _WIN32
    EchoOff() {
        handle_ = GetStdHandle(STD_INPUT_HANDLE);
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(handle_, &mode_))
            active_ = SetConsoleMode(handle_, mode_ & ~static_cast<DWORD>(ENABLE_ECHO_INPUT)) != 0;
    }
    ~EchoOff() { if (active_) SetConsoleMode(handle_, mode_); }
#else
    EchoOff() {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved_) == 0) {
            termios quiet = saved_;
            quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            active_ = tcsetattr(STDIN_FILENO, TCSANOW, &quiet) == 0;
        }
    }
    ~EchoOff() { if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_); }
#endif
    EchoOff(const EchoOff&) = delete;
    EchoOff& operator=(const EchoOff&) = delete;
    bool active() const { return active_; }

private:
    bool active_ = false;
#ifdef _WIN32
    HANDLE handle_ = nullptr;
    DWORD mode_ = 0;
#else
    termios saved_{};
#endif
};

// One input line; a trailing '\r' (Windows line end read on Linux) is dropped.
bool readLine(std::string& line) {
    if (!std::getline(std::cin, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

// Like readLine, but the typed text is not shown at a real console.
bool readHiddenLine(std::string& line) {
    std::cout.flush();
    if (std::cin.rdbuf() != ORIGINAL_CIN_BUFFER) return readLine(line);
    EchoOff echoOff;
    bool ok = readLine(line);
    if (echoOff.active()) std::cout << "\n";   // the Enter key was not echoed either
    return ok;
}

bool isValidUsername(const std::string& username) {
    if (username.size() < MIN_USERNAME || username.size() > MAX_USERNAME) return false;
    for (char c : username) {
        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                       || c == '.' || c == '_' || c == '-';
        if (!allowed) return false;
    }
    return true;
}

bool isValidPassword(const std::string& password) {
    return password.size() >= MIN_PASSWORD && password.size() <= MAX_PASSWORD
           && password.find_first_of("\r\n") == std::string::npos;
}

enum class CredentialCheck { Ok, Invalid, NoEmployees, StorageError };

// Checks a username and password against the stored records, read now.
CredentialCheck checkCredentials(const std::string& username, const std::string& password) {
    std::vector<Employee> employees;
    if (!loadEmployees(employees)) return CredentialCheck::StorageError;
    if (employees.empty()) return CredentialCheck::NoEmployees;

    for (const Employee& employee : employees) {
        if (employee.username == username)   // exact, case-sensitive match
            return verifyPassword(password, employee.passwordHash) ? CredentialCheck::Ok
                                                                   : CredentialCheck::Invalid;
    }
    verifyPassword(password, UNKNOWN_USER_HASH);   // same cost as a wrong password
    return CredentialCheck::Invalid;
}

// Validates the new details, hashes the password and adds the record. The caller has already
// decided that adding is allowed.
EmployeeSetupResult storeNewEmployee(const std::vector<Employee>& existing,
                                     const std::string& username, const std::string& password) {
    EmployeeSetupResult result;
    if (!isValidUsername(username)) {
        result.message = USERNAME_RULE;
        return result;
    }
    if (!isValidPassword(password)) {
        result.message = PASSWORD_RULE;
        return result;
    }
    for (const Employee& employee : existing) {
        if (employee.username == username) {
            result.message = "Error: An employee with this username already exists.";
            return result;
        }
    }

    Employee employee;
    employee.username = username;
    if (!hashPassword(password, employee.passwordHash)) {
        result.message = "Error: The password could not be hashed. No employee was added.";
        return result;
    }
    if (!saveEmployee(employee)) {
        result.message = "Storage error: employee was not added.";
        return result;
    }
    result.success = true;
    result.message = "Employee " + username + " added.";
    return result;
}

} // namespace

bool loginEmployee() {
    std::string username;
    std::string password;

    std::cout << "\n=== Employee Login ===\n";
    std::cout << "Username: ";
    if (!readLine(username)) {
        employeeAuthenticated = false;
        return false;
    }

    std::cout << "Password: ";
    if (!readHiddenLine(password)) {
        employeeAuthenticated = false;
        return false;
    }

    employeeAuthenticated = false;
    switch (checkCredentials(username, password)) {
        case CredentialCheck::Ok:
            employeeAuthenticated = true;
            std::cout << "Employee login successful.\n";
            return true;
        case CredentialCheck::StorageError:
            std::cout << EMPLOYEE_STORAGE_ERROR << "\n";
            return false;
        case CredentialCheck::NoEmployees:
            std::cout << NO_EMPLOYEES << "\n";
            return false;
        case CredentialCheck::Invalid:
            break;
    }
    std::cout << LOGIN_FAILED << "\n";
    return false;
}

bool authenticateCustomer(int accountNo, const std::string& pin) {
    std::vector<Account> accounts;

    if (accountNo <= 0 || pin.empty() || !loadAccounts(accounts)) {
        std::cout << "Invalid account number or PIN.\n";
        return false;
    }

    for (const Account& account : accounts) {
        if (account.accountNumber == accountNo &&
            account.status == STATUS_ACTIVE &&
            account.pin == pin) {
            return true;
        }
    }

    std::cout << "Invalid account number or PIN.\n";
    return false;
}

bool isEmployeeAuthenticated() {
    return employeeAuthenticated;
}

void logoutEmployee() {
    employeeAuthenticated = false;
}

EmployeeSetupResult createFirstEmployee(const std::string& username, const std::string& password) {
    EmployeeSetupResult result;
    std::vector<Employee> employees;
    if (!loadEmployees(employees)) {
        result.message = EMPLOYEE_STORAGE_ERROR + " No employee was added.";
        return result;
    }
    if (!employees.empty()) {
        result.message = "Error: Employees already exist. An existing employee must log in to "
                         "add another.";
        return result;
    }
    return storeNewEmployee(employees, username, password);
}

EmployeeSetupResult addEmployee(const std::string& existingUsername,
                                const std::string& existingPassword,
                                const std::string& username, const std::string& password) {
    EmployeeSetupResult result;
    std::vector<Employee> employees;
    if (!loadEmployees(employees)) {
        result.message = EMPLOYEE_STORAGE_ERROR + " No employee was added.";
        return result;
    }
    if (employees.empty()) {
        result.message = "Error: No employee exists yet. Create the first employee instead.";
        return result;
    }
    if (checkCredentials(existingUsername, existingPassword) != CredentialCheck::Ok) {
        result.message = SETUP_DENIED;
        return result;
    }
    return storeNewEmployee(employees, username, password);
}

bool employeeSetup() {
    std::cout << "\n--- Add Employee ---\n";

    std::vector<Employee> employees;
    if (!loadEmployees(employees)) {
        std::cout << EMPLOYEE_STORAGE_ERROR << " No employee was added.\n";
        return false;
    }

    bool first = employees.empty();
    std::string existingUsername, existingPassword;
    if (first) {
        std::cout << "No employees exist yet. Creating the first employee.\n";
    } else {
        // Always asked here: a login from an earlier run or screen is never reused.
        std::cout << "An existing employee must log in to add another employee.\n";
        std::cout << "Existing username: ";
        if (!readLine(existingUsername)) return false;
        std::cout << "Existing password: ";
        if (!readHiddenLine(existingPassword)) return false;
        if (checkCredentials(existingUsername, existingPassword) != CredentialCheck::Ok) {
            std::cout << SETUP_DENIED << "\n";
            return false;
        }
    }

    std::string username, password, confirmation;
    std::cout << "New username: ";
    if (!readLine(username)) return false;
    std::cout << "New password (8-128 characters): ";
    if (!readHiddenLine(password)) return false;
    std::cout << "Confirm password: ";
    if (!readHiddenLine(confirmation)) return false;
    if (password != confirmation) {
        std::cout << "Error: Passwords do not match. No employee was added.\n";
        return false;
    }

    EmployeeSetupResult result = first
        ? createFirstEmployee(username, password)
        : addEmployee(existingUsername, existingPassword, username, password);
    std::cout << result.message << "\n";
    return result.success;
}
