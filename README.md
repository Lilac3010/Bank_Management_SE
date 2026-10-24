# Bank Management System — Team E11

Console C++ application (C++17). See the SRS, Test Plan and SAD PDFs for requirements.

## Status
| Module | State |
|---|---|
| FR-1 create account (`account.cpp`) | Implemented |
| FR-2 `generateAccountNumber` | Interface only (`account.h`) |
| FR-13 storage (`storage.h`) | Interface only |
| FR-10 auth (`auth.h`) | Interface only; `isEmployeeAuthenticated()` pending FR-10 owner |

## Build and run FR-1 tests (no other modules needed)
    g++ -std=c++17 -Wall -Wextra -o tests.exe tests.cpp account.cpp
    ./tests.exe

With MSVC (run in "Developer PowerShell for VS"):

    cl /std:c++17 /EHsc /W4 tests.cpp account.cpp /Fe:tests.exe
    .\tests.exe

### Test doubles
`tests.cpp` contains test-only fakes for `isEmployeeAuthenticated`, `generateAccountNumber`,
`loadAccounts` and `saveAccount`. The tests are FR-1 unit tests: they do **not** verify
FR-2, FR-10, FR-13 or persistence across restart.

- Build the tests from `tests.cpp` and `account.cpp` only.
- Never build every `.cpp` file together (for example an IDE "build all" or a `*.cpp` glob):
  `tests.cpp` and `main.cpp` both define `main()`, and the fakes clash with the real
  `auth.cpp` / `storage.cpp` (duplicate-symbol link errors).
- When FR-2 is implemented in `account.cpp`, delete the fake `generateAccountNumber` from
  `tests.cpp` in the same change, and rewrite the tests that use `fake::generateCalls` /
  `fake::forceNumber`.

## Build the application (after auth.cpp, storage.cpp and generateAccountNumber exist)
    g++ -std=c++17 -Wall -Wextra -o bank.exe main.cpp account.cpp auth.cpp storage.cpp
    ./bank.exe

With MSVC:

    cl /std:c++17 /EHsc /W4 main.cpp account.cpp auth.cpp storage.cpp /Fe:bank.exe

## Shared decisions (FR-1 / FR-2 / FR-13)
- Money is stored as whole **paise** in `long long` (`Account::balance`, `Transaction::amount`):
  Rs. 1000.50 = `100050`. No floating point is used for money.
- `bool loadAccounts(std::vector<Account>& accounts)` returns `false` when an existing data
  file cannot be read or parsed, and `true` with an empty list when no data file exists yet.
  FR-1 refuses to create an account when loading fails.

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
