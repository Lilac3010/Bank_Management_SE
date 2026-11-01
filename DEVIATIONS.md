# Deviations from the SRS / SAD / Test Plan

Places where the implementation or its verification differs from what the SRS (01), Test Plan
(02) or SAD (03) says, and why. Each entry names the requirement, what the documents say, what
we did, and what would close the gap.

## D-1 NFR-5: `bank.exe` was compiled but not linked (closed)

- **Status:** **closed on 2026-11-01.** `auth.cpp` was added by the SEC-1 pull request (#41).
  With no change to the script or the workflow, `build_and_test.ps1` now links `bank.exe`, and
  CI does so on `windows-latest` (MSVC) and `ubuntu-latest` (g++) on every push and pull request.
  A scripted run of the MSVC build showed the main menu, employee login, account creation, a
  deposit, and denial of a wrong PIN and a wrong employee password. Manual runs of `bank.exe`
  (TC-12, TC-13, TC-16, TC-17) are no longer blocked.
- **Documents:** SRS NFR-5: "The program shall compile using a standard C/C++ compiler on the
  target development OS". Test Plan TC-18 expects "Build succeeds"; Test Plan §3.4 entry
  criteria include "Test build can start and display the main menu".
- **What we did until then:** `auth.cpp` (FR-10 / FR-11, owned by another team member) did not
  exist, so `bank.exe` could not link (unresolved `loginEmployee`, `isEmployeeAuthenticated`,
  `authenticateCustomer`). The CI workflow and `build_and_test.ps1` therefore:
  - built and ran every active test suite, each separately;
  - built and ran `tests_nfr5.cpp`, which links every real module of `bank.exe` except
    `main.cpp` and `auth.cpp` (login is faked) and runs one pass through FR-1 to FR-9 with real
    storage (it still does, as a smoke test that needs no typed credentials);
  - compiled every `bank.exe` source file, including `main.cpp`, without linking.

## D-2 NFR-5: "standard C/C++ compiler" needs C++17 `<filesystem>`

- **Documents:** SRS NFR-5 asks for "a standard C/C++ compiler"; Test Plan §1.3 says "GCC or
  equivalent". Neither names a version.
- **What we do:** `storage.cpp` (FR-13) uses `std::filesystem`, which needs **MSVC 2019+ or
  g++ 8+**. With an older g++ (for example MinGW g++ 6.3.0) only the suites that do not use
  `storage.cpp` can be built; `build_and_test.ps1` skips the others with a message instead of
  failing. CI uses MSVC on `windows-latest` and a current g++ on `ubuntu-latest`.
- **Known warning:** MSVC `/W4` reports C4996 for `localtime` in `transactions.cpp`. It is a
  warning, not an error; it is left as it is because changing it is outside NFR-5.

## D-3 NFR-5: CI uses GitHub Actions, which is an external service

- **Documents:** SRS NFR-5 says the program shall compile "without requiring an external
  service".
- **What we do:** the program itself needs no external service to build or run: the same
  `build_and_test.ps1` runs offline on a local machine. GitHub Actions is used only to run that
  script automatically on every push and pull request to `main`, as required by the course
  brief. It checks the build; the build does not depend on it.

## D-4 Traceability: NFR-5 verification method

- **Documents:** the SRS traceability matrix verifies NFR-5 by "Build check"; the Test Plan
  verifies it with TC-18 (together with NFR-4).
- **What we do:** both are covered by the same evidence: the CI build on two compilers and the
  `tests_nfr5.cpp` link test. The module-structure part of TC-18 belongs to NFR-4.

## D-5 SEC-4: the SRS maps SEC-4 to TC-06, which does not test non-positive withdrawals

- **Documents:** the SRS traceability matrix verifies SEC-4 ("zero or negative
  deposits/withdrawals are not accepted") with TC-04 and TC-06. TC-04 is "Deposit zero/negative
  amount", but TC-06 is "Withdraw greater than balance" (withdraw Rs. 1500 from Rs. 1000), which
  is an FR-4 insufficient-balance test. No numbered test case covers a zero or negative
  **withdrawal**; only Test Plan §5.1 step 6 ("Attempt zero or negative transaction amounts")
  does.
- **What we do:** `tests_sec4.cpp` treats TC-04 and §5.1 step 6 as the SEC-4 tests and applies
  them to **both** deposit and withdrawal: every zero or negative amount, given directly or typed
  at the console, must be rejected with no change. TC-06 stays an FR-4 test (`tests_fr4.cpp`).
- **To close:** in the next Test Plan revision, either add a "Withdraw zero/negative amount"
  case for SEC-4 or extend TC-04 to cover withdrawals, and update the RTM row for SEC-4.

## D-6 NFR-1: TC-16 is an automated timing test, not a stopwatch on `bank.exe`

- **Documents:** SRS NFR-1: "For normal local use, deposit, withdrawal and balance enquiry shall
  complete within 2 seconds." Test Plan §3.3: "Performance will be checked with a stopwatch or
  simple timing measurement"; TC-16: "Run representative deposit/withdrawal/balance operations
  locally".
- **What we do:** when NFR-1 was done, `bank.exe` could not link (D-1), so it could not be run and
  timed by hand. `tests_nfr1.cpp` instead times the same console flows (`depositMoney`,
  `withdrawMoney`, `balanceEnquiry`) with `std::chrono::steady_clock`, using the real
  `storage.cpp`, `transactions.cpp` and `balance_enquiry.cpp` on 500 accounts and 10000
  transactions. Only customer login is a fake. It measures processing time (input given to
  result printed), not the time a person spends typing. The Test Plan allows a "simple timing
  measurement", so this is a change of tool, not of the requirement.
- **Login cost:** the fake reads the accounts file through real storage and searches it. The real
  `authenticateCustomer()` added in PR #41 does the same work (one `loadAccounts` and a search),
  so the measured times still apply.
- **No longer blocked (2026-11-01):** `bank.exe` links since `auth.cpp` was added (D-1).
- **To close:** repeat TC-16 by hand on the real `bank.exe` (stopwatch, a few runs of deposit,
  withdrawal and balance enquiry) and add the result to the README Performance results. Not done
  yet; it needs a person timing the program, so it is left for the team.

## D-7 NFR-3: FR-8 / FR-9 accepted invalid input and changed stored records (fixed)

- **Documents:** SRS NFR-3: "Invalid input shall be rejected without changing the affected
  account balance or stored account record." Test Plan TC-14: invalid account number gives a
  "Clear error; no unintended record/balance change". SAD §5: invalid account number, "do not
  modify data".
- **What was wrong** (found by `tests_nfr3.cpp`; 13 of its cases failed before the fix):
  - `accountClosure()` and `accountUpdate()` read the account number with `std::cin >> int`,
    which stops at the first non-digit. Typing `100001abc` or ` 100001 x` **closed** or
    **updated** account 100001. Typing `1e5` or `100 001` was read as 1 or 100, and `-5` / `0`
    reached the lookup, so the user saw "Account not found." instead of being told the number
    was invalid.
  - `updateAccountDetails()` accepted a name or address made only of spaces, and a name with no
    letters (`...`), and saved it. FR-1 rejects all of these when an account is created.
- **What changed** (kept small; owners: FR-8 / FR-9):
  - `account_closure.cpp`, `account_update.cpp`: the account number is read as a whole line and
    must be digits only (surrounding spaces ignored), 1 to 2147483647, the same rule as FR-5 /
    FR-7. Anything else, or end of input, shows "Invalid account number." and nothing is loaded
    or saved.
  - `account_update.cpp` `updateAccountDetails()`: name, address and contact number are trimmed
    first, and a name must contain at least one letter, the same rules as FR-1. FR-1's checks
    are private to `account.cpp`, so the rules are **matched, not reused**: `trim` and the
    letter rule are copied into `account_update.cpp` (as other modules already copy shared
    helpers; see README Open points).
  - Side effect of trimming: values are stored without surrounding spaces, and a contact number
    typed with surrounding spaces (` 9876543210 `) is now accepted, as in FR-1.
  - Messages are unchanged; making them consistent with other flows is NFR-2.
- **Related structural change:** to test the main menu and Employee Menu, the menu loops moved
  unchanged from `main.cpp` to `menu.cpp` (`runMainMenu()`); `main()` now only calls it. This
  matches the SAD "Console UI" component and does not change behaviour.
- **Not a gap:** at end of input (Ctrl+Z / Ctrl+D) some flows return without a message, because
  nobody is left to read it. `tests_nfr3.cpp` still checks that nothing changed and that the
  program returns.

## D-8 NFR-2 / NFR-4: menus moved from `main.cpp` to `menu.cpp` (SAD update needed)

- **Documents:** SAD §2.3 names a "Console UI" component ("Displays menus, reads input,
  validates basic format and displays results"), but SAD §4.2 (internal module interfaces) and
  §2.5 / §7 (traceability) list no console-UI function or file. The implementation first had the
  menus inside `main.cpp`.
- **What we did:** the main menu and the Employee Menu moved, unchanged, into `menu.cpp` with one
  public function, `void runMainMenu()` (`menu.h`); `main()` only calls it. The Employee Menu stays
  a private function in `menu.cpp` and is reached through main menu option 1. The move was made in
  the NFR-3 pull request (#39) so the menus could be tested without `main()`; NFR-2 relies on it
  for `tests_nfr2.cpp`. Every `bank.exe` build list now includes `menu.cpp`.
- **To close:** in the next SAD revision, add `runMainMenu()` (`menu.cpp`) to the §4.2 interface
  table as the Console UI interface, and map it to FR-12 / NFR-2 in §2.5 and §7.

## D-9 NFR-2: FR-8 / FR-9 screen wording aligned with the other features

- **Documents:** SRS NFR-2: "numbered, menu-driven console interface with clear prompts and
  messages"; SRS §4.1 and Test Plan TC-17. The SRS and SAD do not fix exact wording.
- **What was different:** the FR-8 (update) and FR-9 (close) screens had no `--- Title ---` line,
  asked "Enter account number to update/close:", and their messages had no `Error: ` /
  `Storage error: ` prefix and used other wording than FR-1 to FR-7 for the same situations.
- **What changed** (text only; no checks, order or behaviour changed):

  | Situation | Before | After |
  |---|---|---|
  | Screen start | (none) | `--- Update Account ---` / `--- Close Account ---` |
  | Account-number prompt | `Enter account number to update: ` / `... to close: ` | `Account number: ` |
  | Not logged in | `Access denied. Employee login required.` | `Access denied: employee login required.` |
  | Bad account number | `Invalid account number.` | `Error: Account number must be a positive whole number.` |
  | Data cannot be read | `Unable to load account records.` | `Storage error: could not read account data.` |
  | Unknown account | `Account not found.` | `Error: Account not found.` |
  | Update: closed account | `Cannot update a closed account.` | `Error: Cannot update a closed account.` |
  | Update: bad name / address / contact | `Invalid name. ...` / `Invalid address.` / `Contact number must ...` | Same text with `Error: ` in front |
  | Update: contact in use | `Contact number is already used by another active account.` | Same text with `Error: ` in front |
  | Update: save fails | `Failed to save account updates.` | `Storage error: account details were not updated.` |
  | Close: already closed / other status | `Account is already closed.` / `Account has an invalid status.` | Same text with `Error: ` in front |
  | Close: save fails | `Failed to save account closure.` | `Storage error: account was not closed.` |
  | Successes | `Account details updated successfully.` / `Account closed successfully.` | Unchanged |

- **Tests affected:** `tests_fr8.cpp` and `tests_fr9.cpp` check only success / failure, not
  wording, and pass unchanged. `tests_nfr3.cpp` expected the old `Invalid account number.` for
  its 20 update / close account-number cases; they now expect the shared message. Nothing else
  in that suite changed.

## D-10 SEC-3: no attempt limit, and no explicit employee logout

- **Documents:** SRS SEC-3: "Failed authentication shall deny access and display a clear error
  message without performing the requested operation." SRS SO-1 / SEC-1 and SAD §3 ("Employee
  login is checked before administrative menu operations"). Test Plan TC-12 and TC-13. None of
  them asks for a lockout, an attempt limit or a logout function.
- **What we do:** `tests_sec3.cpp` verifies SEC-3 as written: every failed employee or customer
  login shows a clear error and performs no operation, and the Employee Menu cannot be reached
  without a successful login. It deliberately also checks that there is **no lockout**: five
  failed employee logins in a row do not block a later correct one.
- **Observed, not a gap:** `auth.cpp` keeps the employee logged in after `0. Back to Main Menu`
  until the next login attempt; a failed attempt logs them out (tested). Through the menus this
  cannot be used to skip a login, because the Employee Menu is only reached by main-menu option 1,
  which always asks for the username and password again. Direct calls to the employee functions
  would still succeed in that state; a logout on leaving the Employee Menu would be the FR-10
  owner's change.
- **To close (optional):** if the team wants an attempt limit or logout, add it to the SRS first
  (new SEC requirement and test case), then to `auth.cpp`.

## D-11 NFR-4: SAD §4.2 interface names, and how NFR-4 is verified

- **Documents:** SRS NFR-4: "Each major feature shall be implemented in a separate
  function/module so that individual features can be modified independently." SRS RTM: verified
  by "Code review"; Test Plan TC-18: "inspect module structure". SAD §4.2 lists suggested
  function names and says they "are illustrative and may be changed during implementation".
- **What we do:** the code review is backed by automated checks that run on every build:
  `tests_nfr4.cpp` (function signatures, one header and source per feature, and include rules
  between layers) and the header check in `build_and_test.ps1`. The README Module map is the
  review record.
- **SAD names vs. code** (allowed by SAD §4.2, listed so the SAD can be updated):

  | SAD §4.2 | Code |
  |---|---|
  | `loginEmployee()`, `authenticateCustomer(accountNo, pin)` | Same; plus `isEmployeeAuthenticated()` |
  | `createAccount()` | `createAccount()` (console) and `createAccountWithDetails(...)`; FR-2 adds `generateAccountNumber(existing)` |
  | `searchAccount(accountNo)`, `closeAccount(accountNo)` | Same; plus console flows `accountSearch()`, `accountClosure()` |
  | `updateAccount()` | `updateAccountDetails(accountNo, name, address, contact)` and console `accountUpdate()` |
  | `deposit(accountNo, amount)`, `withdraw(accountNo, amount)` | Same, amount in paise; plus `depositMoney()`, `withdrawMoney()` |
  | `getBalance(accountNo)` | No separate function: console flow `balanceEnquiry()` reads storage directly |
  | `getMiniStatement(accountNo)` | No separate function: console flow `miniStatement()` reads storage directly |
  | `loadAccounts()`, `saveAccount(account)`, `saveTransaction(txn)` | Same; plus `loadTransactions(transactions)` |
  | (none) | `runMainMenu()` (D-8) |

- **Fixed:** `account_update.cpp` (FR-8) included `account_search.h` (FR-7) but used nothing
  from it. It was the only feature that included another feature's header, so the include was
  removed; FR-8 still builds and its tests pass unchanged.
- **Known debt, not changed:** small input helpers (`trim`, `parseAccountNumber`, `formatRupees`
  and others) are copied into several modules instead of shared (README Module map). This keeps
  modules independent, as NFR-4 asks, at the cost of duplicate code.
- **To close:** in the next SAD revision, update the §4.2 table with the names above (and decide
  whether `getBalance` / `getMiniStatement` should become separate functions).
