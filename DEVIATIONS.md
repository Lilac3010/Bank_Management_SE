# Deviations from the SRS / SAD / Test Plan

Places where the implementation or its verification differs from what the SRS (01), Test Plan
(02) or SAD (03) says, and why. Each entry names the requirement, what the documents say, what
we did, and what would close the gap.

## D-1 NFR-5: `bank.exe` is compiled but not linked

- **Documents:** SRS NFR-5: "The program shall compile using a standard C/C++ compiler on the
  target development OS". Test Plan TC-18 expects "Build succeeds"; Test Plan §3.4 entry
  criteria include "Test build can start and display the main menu".
- **What we do:** `auth.cpp` (FR-10 / FR-11, owned by another team member) does not exist yet,
  so `bank.exe` cannot link (unresolved `loginEmployee`, `isEmployeeAuthenticated`,
  `authenticateCustomer`). The CI workflow and `build_and_test.ps1` therefore:
  - build and run every active test suite, each separately;
  - build and run `tests_nfr5.cpp`, which links every real module of `bank.exe` except
    `main.cpp` and `auth.cpp` (login is faked) and runs one pass through FR-1 to FR-9 with real
    storage;
  - compile every `bank.exe` source file, including `main.cpp`, without linking.
- **Closes when:** `auth.cpp` is added. The script then links `bank.exe` automatically, with
  no change to the script or the workflow. The program cannot start and show the main menu
  until then, so manual runs of `bank.exe` (TC-12, TC-13, TC-16, TC-17) are also waiting on it.

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
- **What we do:** `bank.exe` cannot link until `auth.cpp` exists (D-1), so it cannot be run and
  timed by hand. `tests_nfr1.cpp` instead times the same console flows (`depositMoney`,
  `withdrawMoney`, `balanceEnquiry`) with `std::chrono::steady_clock`, using the real
  `storage.cpp`, `transactions.cpp` and `balance_enquiry.cpp` on 500 accounts and 10000
  transactions. Only customer login is a fake. It measures processing time (input given to
  result printed), not the time a person spends typing. The Test Plan allows a "simple timing
  measurement", so this is a change of tool, not of the requirement.
- **Not covered:** the real FR-11 login cost. The fake reads the accounts file through real
  storage, as a real login would, but the final `auth.cpp` may do more work.
- **To close:** once `bank.exe` links, repeat TC-16 by hand on the real program (stopwatch, a few
  runs of each operation) and add the result to the README Performance results.

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
