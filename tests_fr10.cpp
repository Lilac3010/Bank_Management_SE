// FR-10 employee login tests (Test Plan TC-12; SRS FR-10, SEC-1, SEC-3).
// Build: g++ -std=c++17 -Wall -Wextra -o tests_fr10.exe tests_fr10.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp -lcrypto
//    or: cl /std:c++17 /EHsc /W4 tests_fr10.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_fr10.exe
//
// NO test doubles: the real auth.cpp, password_hash.cpp (Windows CNG or OpenSSL), storage.cpp,
// menus and employee features. Never link with a file that fakes login or storage.
// Needs <filesystem> (storage.cpp): MSVC 2019+ or g++ 8+. On Linux, needs libssl-dev.
// Data lives only in a temporary folder that this program creates and deletes.
// Each password hash takes about half a second (600,000 PBKDF2 iterations), so the suite takes
// a little while.
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "account.h"
#include "account_closure.h"
#include "account_search.h"
#include "account_update.h"
#include "auth.h"
#include "menu.h"
#include "password_hash.h"
#include "storage.h"
#include "transactions.h"

namespace {

namespace fs = std::filesystem;

const std::string TEMP_PREFIX = "bms_fr10_test_";
const fs::path EMP_FILE = fs::path("data") / "employees.txt";
const fs::path ACC_FILE = fs::path("data") / "accounts.txt";
const fs::path TXN_FILE = fs::path("data") / "transactions.txt";
const std::string EMP_HEADER = "BMS-EMPLOYEES 1\n";

const std::string USER = "asha.rao";
const std::string PASS = "Correct-Horse-7";
const std::string USER2 = "ravi_k";
const std::string PASS2 = "Second-Staff-9";

const std::string EMPLOYEE_MENU = "=== Employee Menu ===";
const std::string LOGIN_OK = "Employee login successful.";
const std::string LOGIN_FAILED = "Invalid username or password.";
const std::string NO_EMPLOYEES =
    "No employee accounts exist. Run bank.exe --add-employee to create one.";
const std::string EMP_STORAGE_ERROR = "Storage error: could not read employee data.";
const std::string SETUP_DENIED = "Access denied: invalid username or password.";
const std::string EMPLOYEE_DENIED = "Access denied: employee login required.";
const std::string LOGGED_OUT = "Employee logged out.";

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& name) {
    if (condition) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else           { ++failed; std::cout << "[FAIL] " << name << "\n"; }
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string readBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeBytes(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// File contents shown in a test name, with line ends written as \n.
std::string printable(const std::string& text) {
    std::string out;
    for (char c : text) out += c == '\n' ? std::string("\\n") : std::string(1, c);
    return "\"" + out + "\"";
}

std::string toHex(const std::string& bytes) {
    const char* digits = "0123456789abcdef";
    std::string hex;
    for (unsigned char b : bytes) {
        hex += digits[b >> 4];
        hex += digits[b & 0x0f];
    }
    return hex;
}

// True only inside the temp folder created by this program.
bool inTestFolder() {
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    return !ec && cwd.filename().string().rfind(TEMP_PREFIX, 0) == 0;
}

void resetData() {
    if (!inTestFolder()) {
        std::cout << "Refusing to delete data/ outside the test temp folder.\n";
        std::exit(3);
    }
    std::error_code ec;
    fs::remove_all("data", ec);
    logoutEmployee();
}

bool runFlow(bool (*flow)(), const std::string& input, std::string& output) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    bool ok = flow();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    output = out.str();
    return ok;
}

std::string runMenu(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    runMainMenu();
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    return out.str();
}

int runApp(const std::vector<std::string>& args, const std::string& input, std::string& output) {
    std::vector<std::string> storage = args;
    std::vector<char*> argv;
    for (std::string& a : storage) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    int code = runApplication(static_cast<int>(storage.size()), argv.data());
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    output = out.str();
    return code;
}

std::string login(const std::string& username, const std::string& password, bool& ok) {
    std::string output;
    ok = runFlow(loginEmployee, username + "\n" + password + "\n", output);
    return output;
}

size_t employeeCount() {
    std::vector<Employee> employees;
    if (!loadEmployees(employees)) return static_cast<size_t>(-1);
    return employees.size();
}

// Fresh data/ with one employee (USER / PASS).
void seedOneEmployee() {
    resetData();
    createFirstEmployee(USER, PASS);
}

// ---------------------------------------------------------------- hashing
void testPbkdf2Vectors() {
    struct Vector { std::string password, salt; int iterations; size_t length; const char* hex; };
    const Vector vectors[] = {
        // RFC 7914 section 11 (PBKDF2-HMAC-SHA256)
        {"passwd", "salt", 1, 64,
         "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
         "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783"},
        {"Password", "NaCl", 80000, 64,
         "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
         "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d"},
        // Widely published PBKDF2-HMAC-SHA256 vectors (the RFC 6070 inputs with SHA-256)
        {"password", "salt", 1, 32, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
        {"password", "salt", 2, 32, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
        {"password", "salt", 4096, 32, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"},
        {"passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40,
         "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9"},
        {std::string("pass\0word", 9), std::string("sa\0lt", 5), 4096, 16,
         "89b69d0516f829893c696226650a8687"},
    };
#ifdef _WIN32
    const std::string backend = "Windows CNG";
#else
    const std::string backend = "OpenSSL";
#endif
    for (const Vector& v : vectors) {
        std::string out;
        bool ok = pbkdf2HmacSha256(v.password, v.salt, v.iterations, v.length, out);
        check(ok && toHex(out) == v.hex,
              "PBKDF2-HMAC-SHA256 (" + backend + ") c=" + std::to_string(v.iterations) + ", "
                  + std::to_string(v.length) + " bytes matches the published vector");
    }

    std::string out = "unchanged";
    check(!pbkdf2HmacSha256("password", "salt", 0, 32, out) && out == "unchanged",
          "pbkdf2HmacSha256 rejects 0 iterations");
    check(!pbkdf2HmacSha256("password", "salt", 1, 0, out) && out == "unchanged",
          "pbkdf2HmacSha256 rejects an output length of 0");
}

void testHashPassword() {
    std::string a, b;
    bool ok = hashPassword(PASS, a) && hashPassword(PASS, b);
    check(ok && isWellFormedPasswordHash(a) && isWellFormedPasswordHash(b),
          "hashPassword produces the strict stored form");
    check(a.rfind("pbkdf2-sha256$600000$", 0) == 0 && a.size() == 21 + 32 + 1 + 64,
          "Stored form: pbkdf2-sha256, 600000 iterations, 16-byte salt, 32-byte hash");
    check(ok && a != b && a.substr(21, 32) != b.substr(21, 32),
          "Same password hashed twice: different random salts and hashes");
    check(!contains(a, PASS) && !contains(a, toHex(PASS)),
          "Stored form contains neither the password nor its hex");

    // The stored hash is exactly PBKDF2 of the password with the stored salt.
    std::string saltHex = a.substr(21, 32);
    std::string salt;
    for (size_t i = 0; i < saltHex.size(); i += 2)
        salt += static_cast<char>(std::stoi(saltHex.substr(i, 2), nullptr, 16));
    std::string derived;
    check(pbkdf2HmacSha256(PASS, salt, 600000, 32, derived) && toHex(derived) == a.substr(54),
          "Stored hash = PBKDF2-HMAC-SHA256(password, stored salt, 600000, 32)");

    check(verifyPassword(PASS, a) && verifyPassword(PASS, b), "verifyPassword accepts the right password");
    check(!verifyPassword("correct-horse-7", a), "verifyPassword rejects a password in other case");
    check(!verifyPassword(PASS + "x", a) && !verifyPassword(PASS.substr(1), a),
          "verifyPassword rejects an extra or a missing character");
    check(!verifyPassword("", a), "verifyPassword rejects an empty password");
}

void testHashFormat() {
    std::string good;
    hashPassword(PASS, good);
    std::string salt = good.substr(21, 32), hash = good.substr(54);
    struct Bad { const char* label; std::string value; };
    std::string upper = hash;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const Bad bad[] = {
        {"empty", ""},
        {"other algorithm", "pbkdf2-sha1$600000$" + salt + "$" + hash},
        {"algorithm in other case", "PBKDF2-SHA256$600000$" + salt + "$" + hash},
        {"iterations below 600000", "pbkdf2-sha256$599999$" + salt + "$" + hash},
        {"iterations above the maximum", "pbkdf2-sha256$10000001$" + salt + "$" + hash},
        {"iterations with a leading zero", "pbkdf2-sha256$0600000$" + salt + "$" + hash},
        {"iterations with a sign", "pbkdf2-sha256$+600000$" + salt + "$" + hash},
        {"iterations missing", "pbkdf2-sha256$$" + salt + "$" + hash},
        {"salt too short", "pbkdf2-sha256$600000$" + salt.substr(2) + "$" + hash},
        {"salt too long", "pbkdf2-sha256$600000$" + salt + "00$" + hash},
        {"hash too short", "pbkdf2-sha256$600000$" + salt + "$" + hash.substr(2)},
        {"hash too long", "pbkdf2-sha256$600000$" + salt + "$" + hash + "00"},
        {"uppercase hex", "pbkdf2-sha256$600000$" + salt + "$" + upper},
        {"non-hex digit", "pbkdf2-sha256$600000$" + salt + "$" + hash.substr(1) + "g"},
        {"extra field", good + "$00"},
        {"missing hash field", "pbkdf2-sha256$600000$" + salt},
        {"plain password", PASS},
    };
    for (const Bad& b : bad)
        check(!isWellFormedPasswordHash(b.value) && !verifyPassword(PASS, b.value),
              std::string("Malformed stored hash rejected and never matches: ") + b.label);
    check(isWellFormedPasswordHash("pbkdf2-sha256$10000000$" + salt + "$" + hash),
          "Highest accepted iteration count (10000000) is well formed");
}

// ---------------------------------------------------------------- storage
void testEmployeeStorage() {
    resetData();
    std::vector<Employee> employees = {Employee{"stale", "x"}};
    check(loadEmployees(employees) && employees.empty(),
          "No employees file: load succeeds with an empty list (new installation)");
    check(!fs::exists("data"), "Loading creates no file or folder");

    Employee e;
    e.username = USER;
    hashPassword(PASS, e.passwordHash);
    check(saveEmployee(e) && employeeCount() == 1, "saveEmployee adds the first record");
    std::string file = readBytes(EMP_FILE);
    check(file == EMP_HEADER + USER + "|" + e.passwordHash + "\n",
          "File format: header line, then username|passwordHash");
    check(!contains(file, PASS), "The password itself is not in the employees file");

    Employee again = e;
    hashPassword("Different-Pass-1", again.passwordHash);
    check(!saveEmployee(again) && readBytes(EMP_FILE) == file,
          "saveEmployee refuses a username already stored; file unchanged (no overwrite)");

    Employee bad = e;
    bad.username = "other";
    bad.passwordHash = "plain-text-password";
    check(!saveEmployee(bad) && readBytes(EMP_FILE) == file,
          "saveEmployee refuses a malformed password hash");
    bad.username = "";
    bad.passwordHash = e.passwordHash;
    check(!saveEmployee(bad) && readBytes(EMP_FILE) == file, "saveEmployee refuses an empty username");

    Employee second;
    second.username = USER2;
    hashPassword(PASS2, second.passwordHash);
    employees.clear();
    check(saveEmployee(second) && loadEmployees(employees) && employees.size() == 2
              && employees[0].username == USER && employees[0].passwordHash == e.passwordHash
              && employees[1].username == USER2,
          "Second employee appended; the first record is kept unchanged and in order");

    const std::string line = USER + "|" + e.passwordHash + "\n";
    struct Corrupt { const char* label; std::string content; };
    const Corrupt corrupt[] = {
        {"empty file", ""},
        {"header only", EMP_HEADER},
        {"wrong header", "BMS-EMPLOYEES 2\n" + line},
        {"blank line", EMP_HEADER + "\n" + line},
        {"three fields", EMP_HEADER + USER + "|" + e.passwordHash + "|x\n"},
        {"one field", EMP_HEADER + USER + "\n"},
        {"empty username", EMP_HEADER + "|" + e.passwordHash + "\n"},
        {"plain-text password", EMP_HEADER + USER + "|" + PASS + "\n"},
        {"bad escape", EMP_HEADER + USER + "\\x|" + e.passwordHash + "\n"},
        {"duplicate username", EMP_HEADER + line + line},
    };
    for (const Corrupt& c : corrupt) {
        resetData();
        writeBytes(EMP_FILE, c.content);
        std::vector<Employee> loaded = {Employee{"keep", "me"}};
        bool loadFailed = !loadEmployees(loaded) && loaded.size() == 1 && loaded[0].username == "keep";
        check(loadFailed && !saveEmployee(second) && readBytes(EMP_FILE) == c.content,
              std::string("Corrupt employees file (") + c.label
                  + "): load fails, caller's list unchanged, save refuses, file kept");
    }

    resetData();
    fs::create_directories(EMP_FILE);   // a folder where the file should be
    check(!loadEmployees(employees) && !saveEmployee(second),
          "employees.txt is a folder: load fails and save refuses");
}

void testAccountDataUntouched() {
    resetData();
    Account a;
    a.accountNumber = 100001;
    a.name = "Asha Rao";
    a.address = "12 MG Road";
    a.contactNumber = "9876543210";
    a.pin = "1234";
    a.balance = 100000;
    Transaction t;
    t.transactionId = 1;
    t.accountNumber = 100001;
    t.type = TXN_DEPOSIT;
    t.amount = 5000;
    t.dateTime = "2026-11-01 10:00:00";
    bool seeded = saveAccount(a) && saveTransaction(t);
    std::string accounts = readBytes(ACC_FILE), transactions = readBytes(TXN_FILE);

    createFirstEmployee(USER, PASS);
    addEmployee(USER, PASS, USER2, PASS2);
    bool ok = false;
    login(USER, PASS, ok);
    logoutEmployee();
    std::vector<Account> loadedAccounts;
    std::vector<Transaction> loadedTxns;
    check(seeded && readBytes(ACC_FILE) == accounts && readBytes(TXN_FILE) == transactions
              && loadAccounts(loadedAccounts) && loadedAccounts.size() == 1
              && loadTransactions(loadedTxns) && loadedTxns.size() == 1,
          "Employee setup and login leave accounts.txt and transactions.txt byte-for-byte unchanged");
}

// ---------------------------------------------------------------- setup rules
void testCreateFirstEmployee() {
    resetData();
    EmployeeSetupResult r = createFirstEmployee(USER, PASS);
    check(r.success && r.message == "Employee " + USER + " added." && employeeCount() == 1,
          "createFirstEmployee on a new installation adds the employee");
    check(!isEmployeeAuthenticated(), "Creating an employee does not log anyone in");
    check(!contains(readBytes(EMP_FILE), PASS), "Only the hash is stored, not the password");

    std::string file = readBytes(EMP_FILE);
    r = createFirstEmployee(USER2, PASS2);
    check(!r.success && contains(r.message, "Employees already exist") && readBytes(EMP_FILE) == file,
          "createFirstEmployee when an employee exists: refused, file unchanged");
    r = createFirstEmployee(USER, "Overwrite-Attempt-1");
    check(!r.success && readBytes(EMP_FILE) == file && verifyPassword(PASS, file.substr(EMP_HEADER.size() + USER.size() + 1, 118)),
          "createFirstEmployee cannot replace an existing employee's password");

    for (const char* content : {"", "BMS-EMPLOYEES 1\n", "garbage\n"}) {
        resetData();
        writeBytes(EMP_FILE, content);
        r = createFirstEmployee(USER, PASS);
        check(!r.success && contains(r.message, EMP_STORAGE_ERROR) && readBytes(EMP_FILE) == content,
              "Unreadable employees file (" + printable(content)
                  + ") is NOT treated as a new installation: refused, file kept");
    }
}

void testUsernameAndPasswordRules() {
    const char* badUsers[] = {"", "ab", "abcdefghijklmnopqrstuvwxyz0123456", "has space", " lead",
                              "trail ", "pipe|name", "semi;colon", "na\xc3\xafve", "tab\tname",
                              "slash/name", "at@sign"};
    for (const char* u : badUsers) {
        resetData();
        EmployeeSetupResult r = createFirstEmployee(u, PASS);
        check(!r.success && contains(r.message, "Username must be 3 to 32") && !fs::exists(EMP_FILE),
              std::string("Username \"") + u + "\" rejected; nothing stored");
    }
    for (const std::string& u : {std::string("abc"), std::string(32, 'z'), std::string("A.b_c-9")}) {
        resetData();
        check(createFirstEmployee(u, PASS).success,
              "Username \"" + u + "\" accepted (3-32 of letters, digits, '.', '_', '-')");
    }

    const std::string badPasswords[] = {"", "short7c", std::string(129, 'p'), "line\nbreak",
                                        "carriage\rreturn", "trailing-cr\r"};
    for (const std::string& p : badPasswords) {
        resetData();
        EmployeeSetupResult r = createFirstEmployee(USER, p);
        check(!r.success && contains(r.message, "Password must be 8 to 128") && !fs::exists(EMP_FILE),
              "Password of " + std::to_string(p.size()) + " characters"
                  + (p.find_first_of("\r\n") != std::string::npos ? " with a line break" : "")
                  + " rejected; nothing stored");
    }
    for (const std::string& p : {std::string(8, 'p'), std::string(128, 'q'), std::string("sp ace & $ymbols|ok")}) {
        resetData();
        bool ok = createFirstEmployee(USER, p).success;
        bool loggedIn = false;
        login(USER, p, loggedIn);
        check(ok && loggedIn, "Password of " + std::to_string(p.size())
                                  + " characters accepted and usable for login");
    }
}

void testAddEmployee() {
    seedOneEmployee();
    std::string file = readBytes(EMP_FILE);
    EmployeeSetupResult r = addEmployee(USER, "wrong-password", USER2, PASS2);
    check(!r.success && r.message == SETUP_DENIED && readBytes(EMP_FILE) == file,
          "addEmployee with a wrong existing password: access denied, nothing stored");
    r = addEmployee("nobody", PASS, USER2, PASS2);
    check(!r.success && r.message == SETUP_DENIED && readBytes(EMP_FILE) == file,
          "addEmployee with an unknown existing username: access denied");
    r = addEmployee("ASHA.RAO", PASS, USER2, PASS2);
    check(!r.success && r.message == SETUP_DENIED, "addEmployee: existing username is case-sensitive");
    r = addEmployee("", "", USER2, PASS2);
    check(!r.success && r.message == SETUP_DENIED, "addEmployee with empty existing credentials: denied");

    r = addEmployee(USER, PASS, USER, "Overwrite-Attempt-1");
    check(!r.success && contains(r.message, "already exists") && readBytes(EMP_FILE) == file,
          "addEmployee with a duplicate username: rejected, the original record is kept");
    r = addEmployee(USER, PASS, "x", PASS2);
    check(!r.success && contains(r.message, "Username must"), "addEmployee checks the new username");
    r = addEmployee(USER, PASS, USER2, "short");
    check(!r.success && contains(r.message, "Password must"), "addEmployee checks the new password");

    r = addEmployee(USER, PASS, USER2, PASS2);
    bool ok = false;
    login(USER2, PASS2, ok);
    logoutEmployee();
    check(r.success && employeeCount() == 2 && ok,
          "addEmployee with correct existing credentials adds a working login");
    r = addEmployee(USER2, PASS2, "Asha.Rao", "Third-Staff-11");
    check(r.success && employeeCount() == 3,
          "Usernames are case-sensitive: Asha.Rao and asha.rao are different employees");

    // A login session is not enough: credentials are checked in every addEmployee call.
    seedOneEmployee();
    login(USER, PASS, ok);
    check(ok && isEmployeeAuthenticated(), "(setup) employee logged in");
    r = addEmployee(USER, "wrong-password", USER2, PASS2);
    check(!r.success && r.message == SETUP_DENIED && employeeCount() == 1,
          "addEmployee while logged in but with a wrong password: still denied");
    check(isEmployeeAuthenticated(), "addEmployee does not change the login session");
    logoutEmployee();

    resetData();
    r = addEmployee(USER, PASS, USER2, PASS2);
    check(!r.success && contains(r.message, "No employee exists yet") && !fs::exists(EMP_FILE),
          "addEmployee with no employees: refused (use first-employee setup)");

    resetData();
    writeBytes(EMP_FILE, "BMS-EMPLOYEES 1\n");
    r = addEmployee(USER, PASS, USER2, PASS2);
    check(!r.success && contains(r.message, EMP_STORAGE_ERROR) && readBytes(EMP_FILE) == "BMS-EMPLOYEES 1\n",
          "addEmployee with a corrupt employees file: storage error, file kept");
}

// ---------------------------------------------------------------- login
void testLogin() {
    seedOneEmployee();
    bool ok = false;
    std::string output = login(USER, PASS, ok);
    check(ok && contains(output, LOGIN_OK) && isEmployeeAuthenticated(),
          "TC-12 correct credentials: login succeeds, employee authenticated");
    check(contains(output, "=== Employee Login ===") && contains(output, "Username: ")
              && contains(output, "Password: "),
          "Login screen keeps its title and prompts");
    check(!contains(output, PASS), "The password is not shown in the output");

    struct Bad { const char* label; std::string username, password; };
    const Bad bad[] = {
        {"wrong password", USER, "Wrong-Horse-7"},
        {"password in other case", USER, "correct-horse-7"},
        {"unknown username", "nobody", PASS},
        {"username in other case", "ASHA.RAO", PASS},
        {"username with a leading space", " " + USER, PASS},
        {"password with a trailing space", USER, PASS + " "},
        {"empty username and password", "", ""},
    };
    for (const Bad& b : bad) {
        login(USER, PASS, ok);   // start logged in: a failure must log out
        output = login(b.username, b.password, ok);
        check(!ok && contains(output, LOGIN_FAILED) && !contains(output, LOGIN_OK)
                  && !isEmployeeAuthenticated(),
              std::string("TC-12 ") + b.label + ": \"Invalid username or password.\", logged out");
    }

    login(USER, PASS, ok);
    std::string partial;
    ok = runFlow(loginEmployee, USER + "\n", partial);
    check(!ok && !isEmployeeAuthenticated() && !contains(partial, LOGIN_OK),
          "Input ends before the password: login fails, logged out");

    output = login(USER, PASS + "\r", ok);
    check(ok, "A Windows line end (\\r) after the password is ignored");
    logoutEmployee();

    for (int i = 0; i < 5; ++i) login(USER, "wrong" + std::to_string(i), ok);
    output = login(USER, PASS, ok);
    check(ok && isEmployeeAuthenticated(), "No lockout: correct login works after 5 failures");
    logoutEmployee();

    resetData();
    output = login(USER, PASS, ok);
    check(!ok && contains(output, NO_EMPLOYEES) && !isEmployeeAuthenticated() && !fs::exists("data"),
          "No employees file: login denied with a pointer to --add-employee; nothing created");

    for (const char* content : {"", "BMS-EMPLOYEES 1\n", "not a header\n"}) {
        resetData();
        writeBytes(EMP_FILE, content);
        output = login(USER, PASS, ok);
        check(!ok && contains(output, EMP_STORAGE_ERROR) && !isEmployeeAuthenticated()
                  && readBytes(EMP_FILE) == content,
              "Unreadable employees file (" + printable(content) + "): storage error, login denied");
    }
}

// ---------------------------------------------------------------- logout
bool everyEmployeeOperationDenied() {
    CreateAccountResult created = createAccountWithDetails("Kiran Das", "4 Lake Road",
                                                           "9000000009", "4321", 50000);
    SearchAccountResult found = searchAccount(100001);
    UpdateAccountResult updated = updateAccountDetails(100001, "Changed", "9 New Road", "9000000008");
    AccountClosureResult closed = closeAccount(100001);
    bool direct = !created.success && created.message == EMPLOYEE_DENIED && !found.success
                  && found.message == EMPLOYEE_DENIED && !updated.success
                  && updated.message == EMPLOYEE_DENIED && !closed.success
                  && closed.message == EMPLOYEE_DENIED;
    bool (*const flows[])() = {createAccount, accountSearch, accountUpdate, accountClosure};
    bool screens = true;
    for (auto flow : flows) {
        std::string output;
        if (runFlow(flow, "100001\n", output) || !contains(output, EMPLOYEE_DENIED)) screens = false;
    }
    return direct && screens;
}

void testLogout() {
    seedOneEmployee();
    bool ok = false;
    login(USER, PASS, ok);
    CreateAccountResult created = createAccountWithDetails("Asha Rao", "12 MG Road", "9876543210",
                                                           "1234", 100000);
    check(ok && created.success && created.accountNumber == 100001,
          "(setup) logged-in employee created account 100001");
    logoutEmployee();
    check(!isEmployeeAuthenticated(), "logoutEmployee() ends the session");
    check(everyEmployeeOperationDenied(),
          "After logoutEmployee(): create, search, update and close all denied (direct and screens)");

    std::string output = runMenu("1\n" + USER + "\n" + PASS + "\n2\n100001\n0\n");
    check(contains(output, EMPLOYEE_MENU) && contains(output, "Account number : 100001"),
          "Through the menu: logged-in employee can search before leaving");
    check(contains(output, LOGGED_OUT) && !isEmployeeAuthenticated(),
          "Employee Menu 0: \"Employee logged out.\" and the session is ended");
    check(everyEmployeeOperationDenied(), "After leaving the Employee Menu: every employee operation denied");

    output = runMenu("1\n" + USER + "\n" + PASS + "\n");   // input ends inside the Employee Menu
    check(contains(output, EMPLOYEE_MENU) && contains(output, LOGGED_OUT) && !isEmployeeAuthenticated(),
          "End of input in the Employee Menu also logs out");

    output = runMenu("1\n" + USER + "\n" + PASS + "\n0\n4\n100001\n");
    check(!contains(output, "--- Close Account ---") && contains(output, "--- Balance Enquiry ---"),
          "After logout, the next keys reach the main menu, not an employee screen");

    output = runMenu("1\n" + USER + "\nwrong-password\n");
    check(!contains(output, EMPLOYEE_MENU) && !contains(output, LOGGED_OUT)
              && contains(output, "Returning to main menu."),
          "Failed login: no Employee Menu, no logout message, back to the main menu");
}

// ---------------------------------------------------------------- bank.exe --add-employee
void testSetupCommand() {
    resetData();
    std::string output;
    int code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\n" + PASS + "\n", output);
    check(code == 0 && contains(output, "--- Add Employee ---")
              && contains(output, "Creating the first employee") && contains(output, "Employee " + USER + " added.")
              && employeeCount() == 1,
          "--add-employee on a new installation creates the first employee (exit 0)");
    check(!contains(output, PASS) && !contains(output, "Existing username"),
          "First setup asks for no existing login and never shows the password");
    check(!isEmployeeAuthenticated(), "Setup does not log anyone in");

    std::string file = readBytes(EMP_FILE);
    code = runApp({"bank.exe", "--add-employee"}, USER + "\nwrong-password\n" + USER2 + "\n" + PASS2 + "\n" + PASS2 + "\n", output);
    check(code == 1 && contains(output, "Existing username: ") && contains(output, SETUP_DENIED)
              && !contains(output, "New username") && readBytes(EMP_FILE) == file,
          "Second setup with a wrong existing password: denied before any new details, nothing stored");

    code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\n" + USER2 + "\n" + PASS2 + "\n" + PASS2 + "\n", output);
    check(code == 0 && contains(output, "Employee " + USER2 + " added.") && employeeCount() == 2
              && !contains(output, PASS) && !contains(output, PASS2),
          "Second setup with correct existing credentials adds the employee; no password shown");

    file = readBytes(EMP_FILE);
    code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\n" + USER2 + "\nReplace-Pass-1\nReplace-Pass-1\n", output);
    check(code == 1 && contains(output, "already exists") && readBytes(EMP_FILE) == file,
          "Setup with a duplicate username: rejected, existing record not overwritten");

    code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\nnew.one\nFirst-Try-111\nOther-Try-222\n", output);
    check(code == 1 && contains(output, "Passwords do not match") && readBytes(EMP_FILE) == file,
          "Setup with a mismatched confirmation: nothing stored");

    code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\nnew.one\n", output);
    check(code == 1 && readBytes(EMP_FILE) == file, "Setup with input ending early: nothing stored");

    // A session from the menus is never reused by setup.
    bool ok = false;
    login(USER, PASS, ok);
    code = runApp({"bank.exe", "--add-employee"}, "\n\nnew.one\nFirst-Try-111\nFirst-Try-111\n", output);
    check(ok && code == 1 && contains(output, SETUP_DENIED) && readBytes(EMP_FILE) == file,
          "Setup while logged in still asks for and checks existing credentials");
    logoutEmployee();

    resetData();
    writeBytes(EMP_FILE, "BMS-EMPLOYEES 1\n");
    code = runApp({"bank.exe", "--add-employee"}, USER + "\n" + PASS + "\n" + PASS + "\n", output);
    check(code == 1 && contains(output, EMP_STORAGE_ERROR) && !contains(output, "New username")
              && readBytes(EMP_FILE) == "BMS-EMPLOYEES 1\n",
          "Setup with a corrupt employees file: storage error, no prompts, file kept");

    resetData();
    code = runApp({"bank.exe", "--add-employee"}, "bad name\n" + PASS + "\n" + PASS + "\n", output);
    check(code == 1 && contains(output, "Username must") && !fs::exists(EMP_FILE),
          "Setup with an invalid username: rejected, nothing stored");

    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"bank.exe", "--help"},
          std::vector<std::string>{"bank.exe", "--add-employee", "extra"},
          std::vector<std::string>{"bank.exe", "add-employee"}}) {
        code = runApp(args, "", output);
        check(code == 2 && contains(output, "Usage:") && !contains(output, "=== Bank Management System ==="),
              "Unknown command line (" + args.back() + "): usage shown, exit 2, nothing run");
    }
    code = runApp({"bank.exe"}, "0\n", output);
    check(code == 0 && contains(output, "=== Bank Management System ==="),
          "No arguments: the main menu runs as before (exit 0)");
}

} // namespace

int main() {
    std::cout << "FR-10 employee login tests with the REAL auth.cpp, password_hash.cpp, storage.cpp,\n"
                 "menus and employee features (no test doubles). Data lives only in a temporary folder.\n\n";

    std::error_code ec;
    std::random_device rd;
    std::string folderName = TEMP_PREFIX
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_"
        + std::to_string(rd());
    fs::path original = fs::current_path(ec);
    fs::path testRoot = fs::temp_directory_path(ec) / folderName;
    if (ec || !fs::create_directory(testRoot, ec) || ec) {
        std::cout << "Could not create a temporary test folder; no tests run.\n";
        return 2;
    }
    fs::current_path(testRoot, ec);
    if (ec) {
        std::cout << "Could not switch to the temporary test folder; no tests run.\n";
        return 2;
    }
    std::cout << "Temporary folder: " << testRoot.string() << "\n\n";

    check(!isEmployeeAuthenticated(), "Nobody is logged in at program start");
    testPbkdf2Vectors();
    testHashPassword();
    testHashFormat();
    testEmployeeStorage();
    testAccountDataUntouched();
    testCreateFirstEmployee();
    testUsernameAndPasswordRules();
    testAddEmployee();
    testLogin();
    testLogout();
    testSetupCommand();

    fs::current_path(original, ec);
    fs::remove_all(testRoot, ec);
    check(!fs::exists(testRoot), "Temporary test folder removed");

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
