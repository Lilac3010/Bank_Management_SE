# Bank Management System — Team E11

Console C++ application (C++17). See the SRS, Test Plan and SAD PDFs for requirements.

## Status
| Module | State |
|---|---|
| FR-1 create account (`account.cpp`) | Implemented |
| FR-2 `generateAccountNumber` (`account_number.cpp`) | Implemented |
| FR-3 deposit (`transactions.cpp`) | Implemented; own suite parked (see Verification status) |
| FR-4 withdrawal (`transactions.cpp`) | Implemented |
| FR-5 balance enquiry (`balance_enquiry.cpp`) | Implemented |
| FR-6 mini-statement (`mini_statement.cpp`) | Implemented |
| FR-7 employee account search (`account_search.cpp`) | Implemented; Employee Menu option 2 |
| FR-8 update account details (`account_update.cpp`) | Implemented; Employee Menu option 3. Input checks tightened in NFR-3 (DEVIATIONS.md D-7); wording aligned in NFR-2 (D-9) |
| FR-9 close account (`account_closure.cpp`) | Implemented; Employee Menu option 4. Input checks tightened in NFR-3 (DEVIATIONS.md D-7); wording aligned in NFR-2 (D-9) |
| Console menus (`menu.cpp`, `main.cpp`) | Main menu and Employee Menu in `runMainMenu()`; `main()` only calls it (DEVIATIONS.md D-8) |
| FR-13 storage (`storage.cpp`) | Implemented (see Data storage) |
| FR-12 error messages for invalid operations | Met by the existing checks in FR-1, FR-3 / FR-4, FR-7 to FR-11 (no code change needed); verified by `tests_fr12.cpp` for all five SRS categories (see FR-12 error handling) |
| FR-10 / FR-11 auth (`auth.cpp`) | Implemented (SEC-1, PR #41) |
| FR-11 customer authentication (`auth.cpp`) | Implemented: account number + PIN checked by `authenticateCustomer()` before every self-service operation (no customer session). Verified by `tests_fr11.cpp` and `tests_sec2.cpp` (see Customer authentication) |
| FR-10 employee login (`auth.cpp`, `password_hash.cpp`) | Implemented: employees in `data/employees.txt` with PBKDF2-HMAC-SHA256 hashes (no built-in credentials), first employee and later ones added with `bank.exe --add-employee`, logout on leaving the Employee Menu. Verified by `tests_fr10.cpp` and `tests_sec3.cpp` (see Employee logins and DEVIATIONS.md D-12) |
| NFR-5 build on target OS (`build_and_test.ps1`, CI) | Implemented; `bank.exe` links and is built by CI on both runners (DEVIATIONS.md D-1, closed) |
| SEC-2 customer access control | Met; verified by `tests_sec2.cpp` with the real `auth.cpp` and storage (Test Plan TC-13) |
| SEC-3 failed login denies the operation | Met; verified by `tests_sec3.cpp` through the real menus (Test Plan TC-12, TC-13; see DEVIATIONS.md D-10) |
| NFR-4 modular code structure | Met; see Module map. Checked by `tests_nfr4.cpp` and the header check in `build_and_test.ps1` (DEVIATIONS.md D-11) |
| SEC-4 positive transaction amounts (`transactions.cpp`) | Implemented (already in FR-3 / FR-4); verified by `tests_sec4.cpp` |
| NFR-1 2-second response time | Met; verified by `tests_nfr1.cpp` (see Performance results and DEVIATIONS.md D-6) |
| NFR-3 invalid input changes nothing | Met after FR-8 / FR-9 fixes; verified by `tests_nfr3.cpp` (see DEVIATIONS.md D-7) |
| NFR-2 numbered menu-driven interface | Met; verified by `tests_nfr2.cpp` and the TC-17 checklist (manual walk-through of `bank.exe` still to be done) |

## Build and run all test suites (NFR-5)
`build_and_test.ps1` builds every active suite **separately** (each from its own file list
below), runs it, compiles every `bank.exe` source, and prints a summary. It exits with 0 only
if nothing failed.

    powershell -ExecutionPolicy Bypass -File build_and_test.ps1             # Windows
    pwsh ./build_and_test.ps1 -Compiler gcc                                  # Linux / macOS

- `-Compiler auto` (default) uses MSVC on Windows when it is installed (found automatically,
  no Developer prompt needed), otherwise g++. `-Compiler msvc` or `-Compiler gcc` forces one.
- Programs and object files go to `build/<compiler>/`, which git ignores.
- **Compiler versions:** C++17 with `<filesystem>` is needed for `storage.cpp`, so use
  **MSVC 2019+ or g++ 8+**. With an older g++ (for example MinGW g++ 6.3.0) the suites that use
  `storage.cpp` (FR-13, NFR-1, NFR-3, NFR-5, SEC-2, SEC-3, FR-10, FR-11, FR-12) and the `bank.exe` build are **skipped** with
  a message; the other suites still build and run.
- **Crypto library (FR-10):** `password_hash.cpp` uses the operating system's PBKDF2. On Windows
  that is CNG (`bcrypt.lib`, part of the Windows SDK that MSVC already uses; the source asks the
  MSVC linker for it, so `cl` commands need nothing extra). On Linux it is OpenSSL `libcrypto`:
  install `libssl-dev` (Debian / Ubuntu) and add `-lcrypto` to g++ commands that include
  `password_hash.cpp`. The script adds it automatically (`-lbcrypt` for g++ on Windows).
- The script also builds `bank.exe` (it is interactive, so it is not run). Since `auth.cpp` was
  added in PR #41 it links; if `auth.cpp` were missing, the script would compile the other files
  and skip only the link.
- MSVC `/W4` reports warning C4996 (`localtime`) in `transactions.cpp`. It is a warning only and
  is left unchanged (see DEVIATIONS.md D-2).

**CI:** `.github/workflows/build-and-test.yml` runs the same script on every push and pull
request to `main`, on `windows-latest` (MSVC) and `ubuntu-latest` (g++). On Linux it first
installs `libssl-dev` (FR-10). A pull request should only be merged when both jobs pass.

## Build and run FR-1 tests (no other modules needed)
    g++ -std=c++17 -Wall -Wextra -o tests.exe tests.cpp account.cpp
    ./tests.exe

With MSVC (run in "Developer PowerShell for VS"):

    cl /std:c++17 /EHsc /W4 tests.cpp account.cpp /Fe:tests.exe
    .\tests.exe

### Test doubles
Each test suite uses its own test doubles:

- **`tests.cpp` (FR-1)** has its own fake account-number generator, plus fakes for
  `isEmployeeAuthenticated`, `loadAccounts` and `saveAccount`. Because of that fake
  `generateAccountNumber`, it must **never** be linked with `account_number.cpp`.
  These tests do **not** verify FR-2, FR-10, FR-13 or persistence across restart.
- **`tests_fr2.cpp` (FR-2)** uses the **real** generator from `account_number.cpp`, and has its
  own fakes for login (`isEmployeeAuthenticated`) and storage (`loadAccounts`, `saveAccount`).
  These tests do **not** verify FR-10, FR-13 or persistence across restart.
- **`tests_fr3.cpp` (FR-3)** is **parked**: it is kept in the repository but is not built or
  run as part of any flow (see Verification status). It has its own fakes for customer login
  and storage.
- **`tests_fr7.cpp` (FR-7)** uses the real `account_search.cpp`, with fakes for employee login
  (`isEmployeeAuthenticated`) and storage (`loadAccounts`). These tests do **not** verify FR-10,
  FR-13 or persistence across restart.
- **`tests_fr8.cpp` (FR-8)** and **`tests_fr9.cpp` (FR-9)** use the real `account_update.cpp` /
  `account_closure.cpp`, with fakes for login and storage (`loadAccounts`, `saveAccount`). They
  check results with `assert()`, so build them **without** `NDEBUG` (no `/DNDEBUG`, `-DNDEBUG`
  or release build), or the checks are silently skipped.
- **`tests_fr13.cpp` (FR-13)** uses the **real** `storage.cpp` together with the real FR-1 to FR-6
  code. Only employee and customer login (`isEmployeeAuthenticated`, `authenticateCustomer`) are
  fakes. It works only inside a temporary folder it creates and deletes (see Data storage).
- **`tests_nfr5.cpp` (NFR-5)** links the **real** code of every `bank.exe` module except
  `main.cpp`, `auth.cpp` and `password_hash.cpp`; only the `auth.h` functions the menus use are
  fakes (logout does nothing there). It runs one pass
  through FR-1 to FR-9 with real storage inside a temporary folder it creates and deletes.
- **`tests_sec4.cpp` (SEC-4)** uses the real `transactions.cpp`, with fakes for customer login
  (`authenticateCustomer`) and storage. It calls `deposit()` / `withdraw()` directly and through
  the console prompts. These tests do **not** verify FR-11, FR-13 or persistence across restart.
- **`tests_nfr1.cpp` (NFR-1)** uses the **real** `storage.cpp`, `transactions.cpp` and
  `balance_enquiry.cpp`; only customer login (`authenticateCustomer`) is a fake, and it reads
  accounts through the real storage. It works only inside a temporary folder it creates and
  deletes.
- **`tests_nfr3.cpp` (NFR-3)** types invalid input into the **real** menus (`menu.cpp`) and
  console flows with the **real** `storage.cpp`; only login (`loginEmployee`,
  `isEmployeeAuthenticated`, `authenticateCustomer`, `logoutEmployee`, `employeeSetup`) is faked. It works only inside a temporary
  folder it creates and deletes.
- **`tests_nfr2.cpp` (NFR-2)** uses the **real** menus (`menu.cpp`) and the **real** FR-8 / FR-9
  screens (`account_update.cpp`, `account_closure.cpp`). The other six features (create, search,
  deposit, withdraw, balance, mini-statement), login and storage are fakes that count calls, so
  it must **never** be linked with those features' `.cpp` files or `storage.cpp`.
- **`tests_sec2.cpp` (SEC-2)** has **no test doubles**: it uses the real `auth.cpp`, `storage.cpp`
  and the four customer flows. Because it contains the real `auth.cpp`, it must **never** be linked
  with a file that fakes login. It works only inside a temporary folder it creates and deletes.
- **`tests_sec3.cpp` (SEC-3)** has **no test doubles** either: it is the `bank.exe` file list with
  itself in place of `main.cpp`, so it must **never** be linked with a file that fakes login,
  storage or a feature. It works only inside a temporary folder it creates and deletes, and
  creates its test employee there with `createFirstEmployee()`.
- **`tests_fr12.cpp` (FR-12)** has **no test doubles** either (same file list as FR-10); it works
  only inside a temporary folder it creates and deletes.
- **`tests_fr11.cpp` (FR-11)** has **no test doubles**: the same file list as FR-10, with the real
  `auth.cpp`, `storage.cpp`, menus and the four self-service flows. It works only inside a
  temporary folder it creates and deletes.
- **`tests_fr10.cpp` (FR-10)** has **no test doubles**: the `bank.exe` file list with itself in
  place of `main.cpp`, including the real `auth.cpp`, `password_hash.cpp` and `storage.cpp`. It
  works only inside a temporary folder it creates and deletes. It takes about a minute, because
  every password hash costs about half a second (600,000 PBKDF2 iterations).
- **`tests_nfr4.cpp` (NFR-4)** is built **alone**: it includes the headers but links no module,
  and reads the source files, so run it from the repository root.
- Build the active suites **separately**, each from its source-file list:
  - FR-1: `tests.cpp account.cpp`
  - FR-2: `tests_fr2.cpp account.cpp account_number.cpp`
  - FR-4: `tests_fr4.cpp transactions.cpp`
  - FR-5: `tests_fr5.cpp balance_enquiry.cpp`
  - FR-6: `tests_fr6.cpp mini_statement.cpp`
  - FR-7: `tests_fr7.cpp account_search.cpp`
  - FR-8: `tests_fr8.cpp account_update.cpp`
  - FR-9: `tests_fr9.cpp account_closure.cpp`
  - FR-13: `tests_fr13.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp`
  - SEC-4: `tests_sec4.cpp transactions.cpp`
  - NFR-2: `tests_nfr2.cpp menu.cpp account_update.cpp account_closure.cpp`
  - NFR-1: `tests_nfr1.cpp storage.cpp transactions.cpp balance_enquiry.cpp`
  - NFR-3: `tests_nfr3.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp`
  - NFR-5: `tests_nfr5.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp`
  - SEC-2: `tests_sec2.cpp auth.cpp password_hash.cpp storage.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp`
  - SEC-3: `tests_sec3.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp`
  - FR-10: `tests_fr10.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp`
  - FR-11: `tests_fr11.cpp` with the same other files as FR-10
  - FR-12: `tests_fr12.cpp` with the same other files as FR-10

  On Linux add `-lcrypto` to every g++ list that contains `password_hash.cpp`.
  - NFR-4: `tests_nfr4.cpp` (nothing else)

  Linking test files together fails, because each defines `main()` and the same fakes.
- Never build every `.cpp` file together (for example an IDE "build all" or a `*.cpp` glob):
  the test files and `main.cpp` each define `main()`, and the fakes clash with the real
  `account_number.cpp` / `auth.cpp` / `storage.cpp` (duplicate-symbol link errors).

## Build and run FR-2 tests (real generator; login and storage are test doubles)
    g++ -std=c++17 -Wall -Wextra -o tests_fr2.exe tests_fr2.cpp account.cpp account_number.cpp
    ./tests_fr2.exe

With MSVC:

    cl /std:c++17 /EHsc /W4 tests_fr2.cpp account.cpp account_number.cpp /Fe:tests_fr2.exe
    .\tests_fr2.exe

## Build and run FR-7 tests (employee login and storage are test doubles)
    cl /std:c++17 /EHsc /W4 tests_fr7.cpp account_search.cpp /Fe:tests_fr7.exe
    .\tests_fr7.exe

g++ equivalent: `g++ -std=c++17 -Wall -Wextra -o tests_fr7.exe tests_fr7.cpp account_search.cpp`.

## Build and run FR-13 storage tests (real storage; login is a test double)
    cl /std:c++17 /EHsc /W4 tests_fr13.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp /Fe:tests_fr13.exe
    .\tests_fr13.exe

g++ equivalent (g++ 8+ only, see NFR-5 above): the same file list with
`g++ -std=c++17 -Wall -Wextra -o tests_fr13.exe`.
The program prints the temporary folder it uses, and restarts itself twice as separate
processes to check that saved data survives a restart.

## Build and run FR-8 / FR-9 tests (login and storage are test doubles)
    cl /std:c++17 /EHsc /W4 tests_fr8.cpp account_update.cpp /Fe:tests_fr8.exe
    cl /std:c++17 /EHsc /W4 tests_fr9.cpp account_closure.cpp /Fe:tests_fr9.exe

g++ equivalent: the same file lists with `g++ -std=c++17 -Wall -Wextra -o <name>.exe`.

## Build and run NFR-5 link and smoke tests (real modules; login is a test double)
    cl /std:c++17 /EHsc /W4 tests_nfr5.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr5.exe
    .\tests_nfr5.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_nfr5.exe`.

## Build and run SEC-4 tests (customer login and storage are test doubles)
    cl /std:c++17 /EHsc /W4 tests_sec4.cpp transactions.cpp /Fe:tests_sec4.exe
    .\tests_sec4.exe

g++ equivalent: `g++ -std=c++17 -Wall -Wextra -o tests_sec4.exe tests_sec4.cpp transactions.cpp`.

Covers Test Plan TC-04 and §5.1 step 6 for both deposit and withdrawal: amounts 0, -1, -100,
-50000 and `LLONG_MIN` paise passed to `deposit()` / `withdraw()`, and `0`, `0.00`, `0.0`, `00`,
` 0 `, `-0`, `-0.00`, `-0.01`, `-1`, `-500` typed at the console. Each must be rejected with an
error, leave the balance and stored record unchanged, save nothing and record no transaction.
Four control checks confirm that the smallest positive amount (Rs. 0.01) is accepted.

## Module map (NFR-4)
Each major feature is its own module: one header (its interface) and one source file. The
layers follow SAD §2.1: Presentation -> Service / Business -> Persistence.

| Requirement | SAD component | Interface | Source | Public functions |
|---|---|---|---|---|
| Console UI (NFR-2) | Console UI | `menu.h` | `menu.cpp`, `main.cpp` | `runMainMenu()`, `runApplication(argc, argv)`; `main()` only calls `runApplication` |
| FR-10 / FR-11 login | Authentication | `auth.h` | `auth.cpp` | `loginEmployee()`, `isEmployeeAuthenticated()`, `authenticateCustomer(accountNo, pin)`, `logoutEmployee()`, `createFirstEmployee(...)`, `addEmployee(...)`, `employeeSetup()` |
| FR-10 password hashing | Authentication | `password_hash.h` | `password_hash.cpp` | `pbkdf2HmacSha256(...)`, `hashPassword(...)`, `verifyPassword(...)`; header-only `isWellFormedPasswordHash(...)` |
| FR-1 create account | Account Management | `account.h` | `account.cpp` | `createAccountWithDetails(...)`, `createAccount()` |
| FR-2 account numbers | Account Management | `account.h` | `account_number.cpp` | `generateAccountNumber(existing)` |
| FR-3 / FR-4 deposit, withdrawal | Transaction Management | `transactions.h` | `transactions.cpp` | `deposit(accountNo, paise)`, `depositMoney()`, `withdraw(accountNo, paise)`, `withdrawMoney()` |
| FR-5 balance enquiry | Transaction Management | `balance_enquiry.h` | `balance_enquiry.cpp` | `balanceEnquiry()` |
| FR-6 mini-statement | Transaction Log | `mini_statement.h` | `mini_statement.cpp` | `miniStatement()` |
| FR-7 search account | Account Management | `account_search.h` | `account_search.cpp` | `searchAccount(accountNo)`, `accountSearch()` |
| FR-8 update account | Account Management | `account_update.h` | `account_update.cpp` | `updateAccountDetails(...)`, `accountUpdate()` |
| FR-9 close account | Account Management | `account_closure.h` | `account_closure.cpp` | `closeAccount(accountNo)`, `accountClosure()` |
| FR-13 storage | File Storage / Transaction Log | `storage.h` | `storage.cpp` | `loadAccounts`, `saveAccount`, `loadTransactions`, `saveTransaction`; FR-10 adds `loadEmployees`, `saveEmployee` |
| Shared data | (SAD §4.1) | `models.h` | none | `Account`, `Transaction`, `Employee`, status values |

Dependency rules (checked by `tests_nfr4.cpp` on every build):
- Headers include at most `models.h`; `menu.h` and `auth.h` include no project header.
- `main.cpp` includes only `menu.h`. `menu.cpp` may call login and every feature, never storage.
- A feature `.cpp` includes its own header, `models.h`, `auth.h` and `storage.h`, and **never
  another feature's header**, so each feature can be changed on its own.
- `storage.cpp` includes only `storage.h` / `models.h`, plus `password_hash.h` for its
  header-only stored-hash format check (FR-10; no crypto is linked into storage); nothing below
  it depends upward.
- `password_hash.cpp` includes only its own header; `auth.cpp` is its only user.
- Each test suite links only the modules it tests, with its own fakes for the rest; this works
  only because the modules are separate.

Known debt, left as it is so the owners' files stay unchanged: small input helpers are
copied into several modules instead of shared: `trim` (6 files), `parseAccountNumber` (6),
`formatRupees` (5), `isDigits` (4), `parseRupees` (2), `isValidName` (2). A shared helper module
would remove the copies, but every module and most test build lists would change.

## Build and run NFR-4 modular-structure tests (links no module)
    cl /std:c++17 /EHsc /W4 tests_nfr4.cpp /Fe:tests_nfr4.exe
    .\tests_nfr4.exe

g++ equivalent: `g++ -std=c++17 -Wall -Wextra -o tests_nfr4.exe tests_nfr4.cpp` (works with g++ 6.3).
Run it from the repository root. It checks:
- **Interfaces:** 33 `static_assert`s compare every public module function with the signature
  above. A mismatch stops the build. Nothing is called, so nothing needs to be linked.
- **Module map:** each row of the table has its header and source file, and both name its
  functions.
- **Dependencies:** the `#include "..."` lines of all 25 production `.h` / `.cpp` files follow the
  rules above, and none includes a `.cpp` or test file. The test only checks the files in its
  table: add any new production file to the table in `tests_nfr4.cpp` so its dependencies are
  checked too.

`build_and_test.ps1` also compiles every `.h` on its own, included twice (syntax check only), to
prove each header includes what it needs and has a working include guard (summary row
`headers`).

## Build and run SEC-3 failed-authentication tests (whole program, no test doubles)
    cl /std:c++17 /EHsc /W4 tests_sec3.cpp menu.cpp auth.cpp password_hash.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_sec3.exe
    .\tests_sec3.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_sec3.exe`
(add `-lcrypto` on Linux).

Covers SRS SEC-3 and Test Plan TC-12 and TC-13. Input is typed into the real main menu, as in
`bank.exe`. Since FR-10 there are no built-in credentials: the test creates its own employee
(`admin` / `admin123`) in its temporary folder with `createFirstEmployee()`, and the employees
file is part of the "no operation" check.
- **Employee (TC-12):** wrong password, wrong username, username or password in other case, empty
  username and password, a leading space, and input ending early. Each shows `Invalid username or
  password.`, never shows the Employee Menu, leaves nobody logged in and saves nothing. Typing the
  Close Account or Create Account keys after a failed login runs neither (account 100001 stays
  ACTIVE, no account created). After a failed login, `createAccountWithDetails`, `searchAccount`,
  `updateAccountDetails`, `closeAccount` and their four screens all answer `Access denied:
  employee login required.` without showing a screen or saving. The correct credentials open
  the Employee Menu, and leaving it logs the employee out (FR-10); a failed login after a
  successful one logs the employee out again.
- **Customer (TC-13):** wrong PIN, unknown account and CLOSED account for deposit, withdrawal,
  balance enquiry and mini-statement from the main menu. Each shows `Access denied: invalid
  account number or PIN.` and never reaches the amount prompt or shows data.
- **"No operation"** means no success output, the data files (balances, account records, the
  transaction log and the employee records) byte-for-byte unchanged, and no save at all (file
  timestamps untouched).
- **No lockout:** five failed employee logins in a row do not block a later correct one. The SRS
  does not ask for an attempt limit (DEVIATIONS.md D-10).

## Build and run SEC-2 customer access-control tests (real login and storage, no test doubles)
    cl /std:c++17 /EHsc /W4 tests_sec2.cpp auth.cpp password_hash.cpp storage.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp /Fe:tests_sec2.exe
    .\tests_sec2.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_sec2.exe`
(add `-lcrypto` on Linux).

Covers SRS SEC-2 and Test Plan TC-13 (customer part). Saved data: 100001 ACTIVE (PIN 1234),
100002 CLOSED (PIN 5678), 100003 ACTIVE (PIN 4321). For each of deposit, withdrawal, balance
enquiry and mini-statement:
- **denied**: wrong PIN, another account's PIN, unknown account, CLOSED account with its correct
  PIN, empty PIN, PIN of only spaces. Each must show `Access denied: invalid account number or
  PIN.`, never reach the amount prompt or show a balance / statement, and leave both data files
  byte-for-byte unchanged with their timestamps untouched (no save). Input ending before the PIN
  must also run nothing (no message is required at end of input, as in NFR-3);
- **allowed**: the correct account number and PIN completes the operation (deposit and
  withdrawal saved and recorded; balance and only that account's statement shown).

It also calls `authenticateCustomer()` directly for the same cases and checks that it never
saves anything.

## Build and run NFR-2 menu and message-style tests (features, login and storage are test doubles)
    cl /std:c++17 /EHsc /W4 tests_nfr2.cpp menu.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr2.exe
    .\tests_nfr2.exe

g++ equivalent: `g++ -std=c++17 -Wall -Wextra -o tests_nfr2.exe tests_nfr2.cpp menu.cpp account_update.cpp account_closure.cpp`
(no `<filesystem>` needed, so this also works with g++ 6.3).

Covers Test Plan TC-17 (automated part) by typing scripted input into `runMainMenu()`:
- both menus show a title and options numbered 1, 2, ... with 0 last, unchanged from before,
  each followed by `Enter choice: `;
- invalid choices (letters, out of range, empty, `" 1"`, `1.0`, `00`) show
  `Invalid choice. Please try again.`, run nothing and show the menu again;
- 0 exits the main menu or goes back from the Employee Menu; end of input returns;
- every option runs exactly the right feature, then the menu is shown again; a failed employee
  login does not open the Employee Menu;
- the FR-8 / FR-9 screens start with `--- Update Account ---` / `--- Close Account ---`, ask for
  `Account number: `, and every message uses the shared style (next section).

## Console usability review (NFR-2, Test Plan TC-17)
Style rules shared by every screen:
- Menus: `=== Title ===`, options `1. ...` to `n. ...`, then `0. Exit` / `0. Back to Main Menu`,
  then `Enter choice: `. Anything else: `Invalid choice. Please try again.` and the menu again.
- Feature screens start with `--- Title ---` and ask one thing per prompt, saying the format
  where it matters (`Contact number (10 digits)`, `Deposit amount (Rs.)`, `PIN (4 digits)`).
- Failures start with `Error: ` (bad input or a rule), `Access denied: ` (login) or
  `Storage error: ` (files), and say what was wrong. Successes say what happened and show the
  result (new account number, new balance).

TC-17 checklist (status on 2026-11-01):

| # | Check | How verified | Result |
|---|---|---|---|
| 1 | Every menu has a title and numbered options with 0 last | `tests_nfr2.cpp` | Pass |
| 2 | Same options and numbers as the SRS feature list (5 customer / login, 4 employee) | `tests_nfr2.cpp` | Pass |
| 3 | Invalid menu choice gives a clear message and the menu again | `tests_nfr2.cpp`, `tests_nfr3.cpp` | Pass |
| 4 | Each option opens the right feature; 0 goes back / exits | `tests_nfr2.cpp` | Pass |
| 5 | Every feature screen starts with a `--- Title ---` line | Code review of all 8 flows; FR-8 / FR-9 by `tests_nfr2.cpp` | Pass (FR-8 / FR-9 fixed in NFR-2) |
| 6 | Failure messages use `Error: ` / `Access denied: ` / `Storage error: ` | FR-8 / FR-9 by `tests_nfr2.cpp`; others by their own suites and `tests_nfr3.cpp` | Pass (FR-8 / FR-9 fixed in NFR-2) |
| 7 | The PIN is never shown | `tests_fr7.cpp` | Pass |
| 8 | A person outside the team completes every menu flow without help | Manual walk-through of `bank.exe` | **Not run yet**. No longer blocked: `bank.exe` links since `auth.cpp` was added (PR #41). Needs a person outside the team; record the result here |

Known usability points, left for the feature owners because they change behaviour, not wording:
- Close Account closes immediately; there is no "Are you sure?" step.
- A negative amount such as `-500` gets "must be an amount like 1000 or 1000.50" rather than
  saying it must be positive (it is still rejected; SEC-4).
- A failed employee login shows `Invalid username or password.` (from `loginEmployee()` in
  `auth.cpp`) and then `Returning to main menu.`. The `auth.cpp` message has no `Access denied: `
  prefix, unlike the style rule above.
- (Fixed in FR-11.) A wrong customer account number or PIN used to show two lines: `Invalid
  account number or PIN.` from `authenticateCustomer()` and then `Access denied: invalid account
  number or PIN.` from the screen. `authenticateCustomer()` now prints nothing, so only the
  screen's `Access denied: ` line is shown.

## Build and run NFR-3 invalid-input tests (real menus and storage; login is a test double)
    cl /std:c++17 /EHsc /W4 tests_nfr3.cpp menu.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp account_search.cpp account_update.cpp account_closure.cpp /Fe:tests_nfr3.exe
    .\tests_nfr3.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_nfr3.exe`.

Covers Test Plan TC-14. Every case types input into `runMainMenu()`, the same menu `bank.exe`
uses: non-numeric and out-of-range menu choices, letters in amounts and account numbers,
empty lines and end of input, for the main menu, the Employee Menu, create, search, update,
close, deposit, withdrawal, balance enquiry and mini-statement. Each case starts from the same
saved data (2 accounts, 1 transaction) and checks that:
- the expected error message is shown and no success message is;
- `data/accounts.txt` and `data/transactions.txt` are byte-for-byte unchanged;
- no save happened: each file's timestamp is set to a day ago before the case and must not
  change (every save rewrites the whole file), and no `.tmp` file is left;
- the menu returns within 5 seconds; otherwise the suite reports an endless loop and stops.

At end of input (no keyboard left) a flow may return without a message; the other checks still
apply. Three self-checks at the start confirm that the data check notices a save even when it
writes identical bytes.

## Build and run NFR-1 response-time tests (real storage; login is a test double)
    cl /std:c++17 /EHsc /W4 tests_nfr1.cpp storage.cpp transactions.cpp balance_enquiry.cpp /Fe:tests_nfr1.exe
    .\tests_nfr1.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_nfr1.exe`.

The program writes 500 accounts and 10000 transactions (20 per account) into `data/` inside a
temporary folder, checks that the real `loadAccounts` / `loadTransactions` read them back, then
times 50 runs each of the deposit, withdrawal and balance-enquiry console flows on different
accounts with `std::chrono::steady_clock`. Each run is timed from the typed input to the printed
result. It prints the median and worst time and fails if any operation's worst run reaches
2 seconds (SRS NFR-1, Test Plan TC-16), or if any run does not succeed.

## Performance results (NFR-1)
Measured on 2026-11-01 with `tests_nfr1.cpp`. Machine: Intel Core Ultra 7 155H, 32 GB RAM,
SSD, Windows 11 Home 10.0.26200. Compiler: MSVC 2022 19.41 with the README build flags
(`/std:c++17 /EHsc /W4`, no optimisation flags).

Data: 500 accounts (`accounts.txt` 36799 bytes), 10000 transactions (`transactions.txt`
463913 bytes). 50 runs per operation:

| Operation (whole console flow) | Median | Worst | Limit |
|---|---|---|---|
| Deposit | 358.82 ms | 425.75 ms | 2000 ms |
| Withdrawal | 353.99 ms | 426.19 ms | 2000 ms |
| Balance enquiry | 20.12 ms | 28.53 ms | 2000 ms |

- The times exclude typing: they measure the processing between the input and the result.
- **Headroom:** deposit and withdrawal time grows with the size of `transactions.txt`, because
  each one reads the whole file twice (`loadTransactions`, then `saveTransaction`) and rewrites
  it. In a one-off run with 50000 transactions on the same machine, the worst deposit took
  1679 ms and the worst withdrawal 1673 ms, so on this machine the 2-second limit would be reached
  at roughly 60000 stored transactions. Balance enquiry does not read transactions and stays
  under 0.1 s. See Open points.
- CI runs the same test on GitHub's `windows-latest` and `ubuntu-latest` runners; their times
  are printed in the job log and vary with runner load.

## Verification status
Latest run on 2026-11-03 with `build_and_test.ps1`, during FR-12 work, on Windows 11 with
MSVC 2022 19.41 (`/std:c++17 /EHsc /W4`) and MinGW g++ 6.3.0 (`-Wall -Wextra`). Linux (g++ with
OpenSSL) is checked only by CI; see the note under the table.

| Suite | MSVC 2022 | g++ 6.3.0 |
|---|---|---|
| FR-1 `tests.cpp` | 67 passed, 0 failed | 67 passed, 0 failed |
| FR-2 `tests_fr2.cpp` | 19 passed, 0 failed | 19 passed, 0 failed |
| FR-3 `tests_fr3.cpp` | **Parked** (see below) | **Parked** |
| FR-4 `tests_fr4.cpp` | 26 passed, 0 failed | 26 passed, 0 failed |
| FR-5 `tests_fr5.cpp` | 29 passed, 0 failed | 29 passed, 0 failed |
| FR-6 `tests_fr6.cpp` | 19 passed, 0 failed | 19 passed, 0 failed |
| FR-7 `tests_fr7.cpp` | 46 passed, 0 failed | 46 passed, 0 failed |
| FR-8 `tests_fr8.cpp` | all 9 test cases passed | all 9 test cases passed |
| FR-9 `tests_fr9.cpp` | all 5 test cases passed | all 5 test cases passed |
| SEC-4 `tests_sec4.cpp` | 34 passed, 0 failed | 34 passed, 0 failed |
| NFR-2 `tests_nfr2.cpp` | 57 passed, 0 failed | 57 passed, 0 failed |
| FR-13 `tests_fr13.cpp` | 58 passed, 0 failed (plus 12 checks inside the two restart processes, all passed) | Skipped: needs g++ 8+ |
| NFR-1 `tests_nfr1.cpp` | 9 passed, 0 failed (timings in Performance results) | Skipped: needs g++ 8+ |
| NFR-3 `tests_nfr3.cpp` | 95 passed, 0 failed | Skipped: needs g++ 8+ |
| NFR-5 `tests_nfr5.cpp` | 12 passed, 0 failed | Skipped: needs g++ 8+ |
| SEC-2 `tests_sec2.cpp` | 44 passed, 0 failed | Skipped: needs g++ 8+ |
| SEC-3 `tests_sec3.cpp` | 36 passed, 0 failed | Skipped: needs g++ 8+ |
| FR-10 `tests_fr10.cpp` | 146 passed, 0 failed (Windows CNG; about 50 s) | Skipped: needs g++ 8+ |
| FR-11 `tests_fr11.cpp` | 102 passed, 0 failed | Skipped: needs g++ 8+ |
| FR-12 `tests_fr12.cpp` | 76 passed, 0 failed | Skipped: needs g++ 8+ |
| NFR-4 `tests_nfr4.cpp` | 39 passed, 0 failed | 39 passed, 0 failed |
| NFR-4 header check | all 12 headers compile alone | all 12 headers compile alone |
| `bank.exe` | Linked (not run: interactive; a scripted run is described under Employee logins) | Skipped: needs g++ 8+ |

CI (`windows-latest` MSVC, `ubuntu-latest` g++) runs the same script on every pull request.
**The Linux side of FR-10** (`password_hash.cpp` with OpenSSL, and the terminal echo code in
`auth.cpp`) cannot be built on the Windows development machine, so it is verified **only** by the
`ubuntu-latest` CI job. Check that job before merging the FR-10 pull request.

- **FR-3 (`tests_fr3.cpp`)** is still parked: it is not part of `build_and_test.ps1`, CI or any
  documented flow, because its test program was blocked by Windows Smart App Control earlier.
  Un-parking it is the FR-3 owner's decision. For information: when built by hand on 2026-11-01
  (`tests_fr3.cpp transactions.cpp`) it was not blocked and gave **42 passed, 0 failed** with both
  MSVC 2022 and g++ 6.3.0. FR-3's `deposit()` is also exercised by `tests_fr13.cpp` against real
  storage (successful deposits, rollback when the transaction cannot be saved, and deposits after
  a restart).
- Smart App Control can block any freshly built test program on this machine. A blocked run
  means "not run", not "failed".

## Build the application
    cl /std:c++17 /EHsc /W4 main.cpp menu.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp storage.cpp account_search.cpp account_update.cpp account_closure.cpp auth.cpp password_hash.cpp /Fe:bank.exe
    .\bank.exe

g++ equivalent (g++ 8+ only): the same file list with `g++ -std=c++17 -Wall -Wextra -o bank.exe`,
plus `-lcrypto` on Linux (needs `libssl-dev`). `build_and_test.ps1` also builds it, as
`build/<compiler>/bank.exe`.

Run `bank.exe` from the folder where you want the data kept (normally the repository root);
see Data storage. The first run starts with no accounts and **no employee login**:
1. Create the first employee once: `.\bank.exe --add-employee` (see Employee logins).
2. Start `.\bank.exe`, main menu `1` (Employee Login) with that username and password.
3. Employee Menu `1` to create an account; note the account number and the 4-digit PIN.
4. `0` back to the main menu (this logs the employee out), then `2` to `5` for deposit,
   withdrawal, balance and mini-statement with that account number and PIN.

## Employee logins (FR-10)
There are **no built-in credentials**. Employees are stored in `data/employees.txt` (see Data
storage) and are added only with the setup command:

    .\bank.exe --add-employee

- **First employee:** allowed only while no employees file exists. The command asks for a new
  username and the password twice.
- **Every later employee:** the command first asks for an **existing** employee's username and
  password and checks them against the stored records in that run (a login from the menus or an
  earlier run is never reused). Wrong credentials stop the command before any new details are
  asked for. Then it asks for the new username and the password twice.
- **Rules:** username 3-32 characters of letters, digits, `.`, `_` and `-`, case-sensitive
  (`Asha` and `asha` are different employees). Password 8-128 characters, no line breaks.
  A username that already exists is rejected; existing records are never replaced or deleted.
- **Unreadable file:** if `data/employees.txt` exists but cannot be read (wrong header, header
  only, a malformed record or hash, a duplicate username), the command and the login both refuse
  with `Storage error: could not read employee data.`. Such a file is never treated as "no
  employees", so it cannot be used to create a new first employee. Fix or restore the file.
- **Exit code:** 0 when an employee was added, 1 otherwise; any other argument prints the usage
  and returns 2.
- **Passwords** are not echoed while typed at a real console (Windows console or Linux
  terminal). When input is redirected (scripts, tests) nothing is changed.
- **Login:** main menu `1`. Wrong username and wrong password give the same `Invalid username or
  password.`; with no employees file the login says to run `--add-employee`. There is no attempt
  limit (SEC-3 tests expect none).
- **Logout:** leaving the Employee Menu (`0`, or end of input) prints `Employee logged out.` and
  ends the session; every employee operation is denied again until the next login.
- **How passwords are stored:** PBKDF2-HMAC-SHA256 with 600,000 iterations, a 16-byte random salt
  and a 32-byte hash, computed by the operating system's crypto library (Windows CNG, Linux
  OpenSSL), never by project code. The stored hash is compared in constant time. Each password
  check takes about half a second on purpose.
- **Lost password:** there is no reset. Back up `data/`; restoring an old `employees.txt` is the
  only way back. See DEVIATIONS.md D-12 for the limits of this design.

Checked on 2026-11-01 with a scripted run of the MSVC `bank.exe` in an empty folder: first
employee added; second setup refused with a wrong existing password; second employee added with
the correct one; duplicate username refused; unknown argument printed the usage (exit 2); login,
account search, `0` printed `Employee logged out.`; a wrong password was refused. The employees
file held only the two hash records.

## FR-12 error handling
SRS FR-12 asks for an appropriate error message when an operation fails because of invalid
input, invalid credentials, insufficient balance, duplicate account data or a non-existent
account. The existing features already did this; `tests_fr12.cpp` checks each category once
through the real program, and that nothing changes on disk (files byte-for-byte equal and not
rewritten):

| Category | Where it is checked | Message (examples) |
|---|---|---|
| Invalid input | FR-1 fields and opening deposit; deposit / withdrawal amounts; account numbers on every screen; menu choices | `Error: PIN must be exactly 4 digits.`, `Error: Deposit amount must be an amount like 1000 or 1000.50.`, `Error: Account number must be a positive whole number.`, `Invalid choice. Please try again.` |
| Invalid credentials | FR-10 employee login, FR-11 customer screens | `Invalid username or password.`, `Access denied: invalid account number or PIN.` |
| Insufficient balance | FR-4 `withdraw()` | `Error: Insufficient balance. Withdrawal not made.` |
| Duplicate account data | FR-1 create, FR-8 update (contact number of another ACTIVE account) | `Error: An active account with this contact number already exists.` |
| Non-existent account | FR-7 search, FR-8 update, FR-9 close, `deposit()` / `withdraw()` | `Error: Account not found.`; at the customer screens `Access denied: ...`, so the message does not reveal which account numbers exist |

Deeper cases for each category are in `tests_nfr3.cpp`, `tests_sec2.cpp` to `tests_sec4.cpp`,
`tests_fr10.cpp` and `tests_fr11.cpp`. Known gaps are listed in DEVIATIONS.md D-14.

## Customer authentication (FR-11)
- **Per operation, no session:** deposit, withdrawal, balance enquiry and mini-statement each ask
  for the account number and PIN, and call `authenticateCustomer(accountNo, pin)` before they show
  any data or ask for an amount. Nothing is remembered between operations, so there is no
  customer login state to clear and no customer logout. Customer and employee logins are
  independent: neither grants the other.
- **Check:** the account must be stored, `ACTIVE`, and its stored PIN must match exactly (text
  comparison, so `0420` is not `420`). The screens trim spaces around both inputs first.
  CLOSED or any other status is denied even with the correct PIN. Accounts are read from
  storage on every check; an unreadable accounts file denies access.
- **Denial:** one message, `Access denied: invalid account number or PIN.`, which does not say
  which part was wrong. Balance enquiry and mini-statement reject a malformed account number
  (`Error: Account number must be a positive whole number.`) before asking for the PIN. Nothing is
  saved on any denial. There is no attempt limit (SRS has none; DEVIATIONS.md D-10).
- **Limits** (DEVIATIONS.md D-13): PINs are stored in plain text; an unreadable accounts file is
  reported as invalid credentials, not as a storage error; `deposit()` / `withdraw()` themselves
  do not check login, so every caller must authenticate first (all current callers do).

## Data storage (FR-13)
- **Location:** `data/accounts.txt`, `data/transactions.txt` and (FR-10) `data/employees.txt`,
  relative to the folder the program is started from. `data/` is created on the first save; on
  first launch there are no files and the program starts with no accounts, no transactions and
  no employees.
- **Not committed:** `data/` is listed in `.gitignore`. Never commit real or test banking data.
- **Reset:** close the program and delete `data/`. Back it up first if you need the records.
- **Format:** UTF-8 text, one record per line, fields separated by `|`, after a header line:

      BMS-ACCOUNTS 1
      100001|Asha Rao|12 MG Road, Bengaluru|9876543210|1234|150000|ACTIVE

      BMS-TRANSACTIONS 1
      1|100001|DEPOSIT|50000|2026-10-27 14:30:00

      BMS-EMPLOYEES 1
      asha.rao|pbkdf2-sha256$600000$<32 hex digits: salt>$<64 hex digits: hash>

  - Account fields: `accountNumber|name|address|contactNumber|pin|balance|status`.
  - Transaction fields: `transactionId|accountNumber|type|amount|dateTime`.
  - Employee fields: `username|passwordHash`. The password itself is never stored. A record is
    only ever added (never replaced), a header-only employees file is unreadable (the program
    never writes one), and the hash must have exactly the form above with 600,000 to 10,000,000
    iterations and lowercase hex.
  - Money is integer paise (`150000` = Rs. 1500.00), never a decimal number.
  - Spaces are stored as-is. In text fields `\`, `|`, newline and carriage return are written
    as `\\`, `\|`, `\n` and `\r`, so any text round-trips safely.
- **Strict reading:** a file with a wrong header, a wrong field count, a non-integer or
  out-of-range number, an invalid escape, a blank line, or a duplicate account number /
  transaction ID is treated as unreadable. Loading then returns `false` and changes nothing,
  and every save refuses to overwrite that file, so the bad file is kept for inspection.
- **Safe writing:** each save writes the whole file to `<file>.tmp` and then replaces the real
  file with it (`std::filesystem::rename`, checked to replace existing files with MSVC). If
  anything fails, the original file is left unchanged and the temp file is removed.
- **Records are never deleted:** updates replace the record with the same account number in
  place; CLOSED accounts stay stored, which FR-2 relies on.
- **PINs are stored in plain text.** This is an academic simulation (SAD §3 excludes
  encryption); do not use real PINs. Employee passwords, by contrast, are stored only as
  PBKDF2 hashes (FR-10); see DEVIATIONS.md D-12 for why the two differ.
- **Consistency limit:** each file is updated all-or-nothing, but a deposit or withdrawal
  updates two files. FR-3 / FR-4 save the account first and, if the transaction cannot be
  saved, restore the original account (tested with real files). If that restore also fails,
  the user is told data may be inconsistent; that double failure is only tested with fakes
  (`tests_fr4.cpp`). Saving both files as one atomic step would need a new combined storage
  call. The storage also assumes one running copy of the program at a time.

## Shared decisions (FR-1 / FR-2 / FR-3 / FR-13)
- Money is stored as whole **paise** in `long long` (`Account::balance`, `Transaction::amount`):
  Rs. 1000.50 = `100050`. No floating point is used for money.
- `bool loadAccounts(std::vector<Account>& accounts)` returns `false` when an existing data
  file cannot be read or parsed, and `true` with an empty list when no data file exists yet.
  FR-1 refuses to create an account when loading fails.
- Account numbers (FR-2): the first account is **100001**; each new number is the highest
  number among **all** stored accounts (ACTIVE and CLOSED) + 1. Gaps are never filled, so
  no stored number is reissued. If `INT_MAX` is already used, the generator returns 0 and
  FR-1 creates nothing.
- Closing an account (FR-9) must mark it `CLOSED` and keep the record. Physically deleting
  the highest-numbered account would let its number be issued again.
- Transactions (FR-3): `bool loadTransactions(std::vector<Transaction>& transactions)` has the
  same contract as `loadAccounts` and is implemented in `storage.cpp`.
  `saveTransaction` appends one entry exactly as given and refuses a duplicate ID. Each new `transactionId` is the highest
  stored ID + 1, starting at 1. Deposit transactions use type `"DEPOSIT"` (`TXN_DEPOSIT` in
  `transactions.h`).
- Deposit authentication: the console flow `depositMoney()` checks the account number and PIN
  with `authenticateCustomer()` before calling `deposit()`. `deposit()` itself does not check
  login, so any other customer-facing caller must authenticate first.
- Deposit saving is **not atomic**: the storage interface has separate `saveAccount` and
  `saveTransaction` calls. `deposit()` saves the account first; if the transaction cannot be
  saved it re-saves the original account and reports an error. If that rollback also fails,
  the error says account data may be inconsistent. True all-or-nothing saving needs a combined
  FR-13 storage call.

## FR-3 assumptions (not specified in the SRS)
- Deposit amounts follow the FR-1 rules: greater than 0, at most 2 decimal places, at most
  15 digits before the decimal point. Text such as `nan`, `inf`, `-500`, `1e3` or `1,000` is
  rejected (a `long long` paise amount cannot hold NaN or infinity).
- Only ACTIVE accounts accept deposits; CLOSED or any other status is rejected.
- Login failures show one message ("invalid account number or PIN") without saying which part
  was wrong.

## Open points for team discussion
- `Transaction` has no resulting-balance field; FR-6 mini-statements may want one.
- `transactions.cpp` copies FR-1's money parsing/formatting helpers; a shared helper would
  remove the duplication. The same is true for several other modules; see Module map (NFR-4).
- A combined storage call for account + transaction would make deposits (and FR-4
  withdrawals) atomic.
- NFR-1 headroom: deposit and withdrawal time grows with `transactions.txt`, because
  `saveTransaction` re-reads and rewrites the whole file. On the test machine the 2-second limit
  would be reached at roughly 60000 stored transactions (see Performance results). Appending
  one line instead of rewriting, or passing the already-loaded list to storage, would remove most
  of that cost.

## FR-7 decisions (not specified in the SRS)
- Access: `accountSearch()` (console) and `searchAccount(accountNo)` both require
  `isEmployeeAuthenticated()`. The console flow checks it before prompting; nothing is read
  from storage for an unauthenticated caller.
- Search is an exact match on the account number (digits only, 1 to 2147483647).
- Accounts of **every** status are found, including CLOSED; the status is shown with the
  details. "Account not found" means no stored record has that number.
- Shown: account number, name, address, contact number, balance (Rs., 2 decimals), status.
  The **PIN is never shown**, and `searchAccount()` returns the account with `pin` cleared.
- Search is read-only: it never calls `saveAccount`.

## FR-1 assumptions (not specified in the SRS)
- 4-digit numeric PIN entered by the employee at account creation.
- Initial deposit must be greater than 0 (minimum Rs. 0.01 = 1 paisa). At the console only
  plain amounts such as `1000`, `1000.5` or `1000.50` are accepted, with at most 15 digits
  before the decimal point; signs, exponents (`1e3`), hex (`0x10`) and separators (`1,000`)
  are rejected.
- Contact number: exactly 10 digits. Name: letters, spaces, '.' and apostrophes only.
- Address must not contain '|'.
- Duplicate account = an ACTIVE account with the same contact number.
- The initial deposit is not recorded as a transaction.
