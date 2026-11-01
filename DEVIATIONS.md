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
