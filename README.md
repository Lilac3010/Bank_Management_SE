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
| FR-13 storage (`storage.cpp`) | Implemented (see Data storage) |
| FR-10 / FR-11 auth (`auth.h`) | Interface only: no `auth.cpp` yet, so `bank.exe` cannot link |

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
- **`tests_fr13.cpp` (FR-13)** uses the **real** `storage.cpp` together with the real FR-1 to FR-6
  code. Only employee and customer login (`isEmployeeAuthenticated`, `authenticateCustomer`) are
  fakes. It works only inside a temporary folder it creates and deletes (see Data storage).
- Build the active suites **separately**, each from its source-file list:
  - FR-1: `tests.cpp account.cpp`
  - FR-2: `tests_fr2.cpp account.cpp account_number.cpp`
  - FR-4: `tests_fr4.cpp transactions.cpp`
  - FR-5: `tests_fr5.cpp balance_enquiry.cpp`
  - FR-6: `tests_fr6.cpp mini_statement.cpp`
  - FR-7: `tests_fr7.cpp account_search.cpp`
  - FR-13: `tests_fr13.cpp storage.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp`

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

g++ equivalent: the same file list with `g++ -std=c++17 -Wall -Wextra -o tests_fr13.exe`.
The program prints the temporary folder it uses, and restarts itself twice as separate
processes to check that saved data survives a restart.

## Verification status
Latest run with MSVC 2022 (`/std:c++17 /EHsc /W4`), during FR-7 work:

| Suite | Result |
|---|---|
| FR-1 `tests.cpp` | 67 passed, 0 failed |
| FR-2 `tests_fr2.cpp` | 19 passed, 0 failed |
| FR-3 `tests_fr3.cpp` | **Parked, not run** (see below) |
| FR-4 `tests_fr4.cpp` | 26 passed, 0 failed |
| FR-5 `tests_fr5.cpp` | 29 passed, 0 failed |
| FR-6 `tests_fr6.cpp` | 19 passed, 0 failed |
| FR-7 `tests_fr7.cpp` | 46 passed, 0 failed (also 46 / 0 with MinGW g++ 6.3.0 `-Wall -Wextra`) |
| FR-13 `tests_fr13.cpp` | 58 passed, 0 failed (plus 12 checks inside the two restart processes, all passed) |

- **FR-3 (`tests_fr3.cpp`)** is still parked: it is not compiled or run in any documented flow,
  because its test program was blocked by Windows Smart App Control earlier. FR-3's `deposit()`
  is, however, exercised by `tests_fr13.cpp` against real storage (successful deposits,
  rollback when the transaction cannot be saved, and deposits after a restart).
- Smart App Control can block any freshly built test program on this machine. A blocked run
  means "not run", not "failed".

## Build the application
    cl /std:c++17 /EHsc /W4 main.cpp account.cpp account_number.cpp transactions.cpp balance_enquiry.cpp mini_statement.cpp storage.cpp account_search.cpp auth.cpp /Fe:bank.exe
    .\bank.exe

g++ equivalent: the same file list with `g++ -std=c++17 -Wall -Wextra -o bank.exe`.

**Not possible yet:** `auth.cpp` (FR-10 / FR-11) does not exist. Every other file compiles, but
the link fails with exactly three unresolved symbols: `loginEmployee`, `isEmployeeAuthenticated`
and `authenticateCustomer`.

Run `bank.exe` from the folder where you want the data kept (normally the repository root);
see Data storage.

## Data storage (FR-13)
- **Location:** `data/accounts.txt` and `data/transactions.txt`, relative to the folder the
  program is started from. `data/` is created on the first save; on first launch there are no
  files and the program starts with no accounts and no transactions.
- **Not committed:** `data/` is listed in `.gitignore`. Never commit real or test banking data.
- **Reset:** close the program and delete `data/`. Back it up first if you need the records.
- **Format:** UTF-8 text, one record per line, fields separated by `|`, after a header line:

      BMS-ACCOUNTS 1
      100001|Asha Rao|12 MG Road, Bengaluru|9876543210|1234|150000|ACTIVE

      BMS-TRANSACTIONS 1
      1|100001|DEPOSIT|50000|2026-10-27 14:30:00

  - Account fields: `accountNumber|name|address|contactNumber|pin|balance|status`.
  - Transaction fields: `transactionId|accountNumber|type|amount|dateTime`.
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
  encryption); do not use real PINs.
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
  remove the duplication.
- A combined storage call for account + transaction would make deposits (and FR-4
  withdrawals) atomic.

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
