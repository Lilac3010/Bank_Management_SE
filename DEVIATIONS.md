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
