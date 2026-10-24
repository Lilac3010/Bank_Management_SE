# Bank Management System — Team E11

Console C++ application (C++17). See the SRS, Test Plan and SAD PDFs for requirements.

## Status
| Module | State |
|---|---|
| FR-1 create account (`account.cpp`) | Implemented |
| FR-2 `generateAccountNumber` (`account_number.cpp`) | Implemented |
| FR-3 deposit (`transactions.cpp`) | Implemented; compiles; **not yet verified by tests** (see Verification status) |
| FR-13 storage (`storage.h`) | Interface only |
| FR-10 auth (`auth.h`) | Interface only; `isEmployeeAuthenticated()` pending FR-10 owner |

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
- Build the active suites **separately**, each from its documented source-file list:
  - FR-1: `tests.cpp account.cpp`
  - FR-2: `tests_fr2.cpp account.cpp account_number.cpp`

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

## Verification status
- **FR-1 tests (`tests.cpp`)**: last run passed 67/67 with MSVC. A later rebuild during FR-3
  work was blocked from running by Windows Smart App Control ("An Application Control policy
  has blocked this file"). The FR-1 source has not changed since the passing run.
- **FR-2 tests (`tests_fr2.cpp`)**: pass 19/19 with MSVC (run during FR-3 work).
- **FR-3 tests (`tests_fr3.cpp`)**: **never run.** The built test program was blocked by Smart
  App Control. The suite is parked: it is not compiled or run in any documented flow until a
  way to run it is found. FR-3 deposit logic is therefore **not verified by tests**; it only
  compiles cleanly with `cl /std:c++17 /EHsc /W4`.

## Build the application (after auth.cpp and storage.cpp exist)
    g++ -std=c++17 -Wall -Wextra -o bank.exe main.cpp account.cpp account_number.cpp transactions.cpp auth.cpp storage.cpp
    ./bank.exe

With MSVC:

    cl /std:c++17 /EHsc /W4 main.cpp account.cpp account_number.cpp transactions.cpp auth.cpp storage.cpp /Fe:bank.exe

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
- Transactions (FR-3): `bool loadTransactions(std::vector<Transaction>& transactions)` is
  declared in `storage.h` for FR-13 to implement (same contract as `loadAccounts`).
  `saveTransaction` appends one entry exactly as given. Each new `transactionId` is the highest
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
  remove the duplication.
- A combined storage call for account + transaction would make deposits (and FR-4
  withdrawals) atomic.

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
