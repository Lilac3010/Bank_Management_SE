# Bank Management System

**Team E11 | Software Engineering Project**

## Overview

The Bank Management System is a console-based application developed in C++17 to simulate essential banking operations. It provides functionality for managing customer accounts, processing transactions, viewing account balances, displaying transaction history, and storing banking records persistently.

The project follows a modular design, with separate components for account management, transaction processing, authentication, and data storage.

## Objectives

- Simplify customer account management.
- Support deposits and withdrawals with appropriate validation.
- Provide secure access to customer-specific account information through authentication.
- Maintain transaction history and account balances.
- Store account and transaction records persistently.
- Verify functionality through automated tests.

## Features

| Feature | Description |
|---|---|
| Account Management | Create and manage customer account records. |
| Account Number Generation | Generate unique account numbers. |
| Deposit | Deposit money into eligible customer accounts. |
| Withdrawal | Withdraw money subject to validation and available balance. |
| Balance Enquiry | View the balance of an authenticated customer's account. |
| Mini-Statement | Display recent transactions for an authenticated customer. |
| Authentication | Support employee login and customer authentication. |
| Persistent Storage | Store and retrieve account and transaction records. |

Features are implemented incrementally and verified through individual test suites. Some functionality remains dependent on pending integration work.

## Technology Stack

- **Programming Language:** C++17
- **Compiler:** GCC (`g++`) or Microsoft Visual C++ (MSVC)
- **Development Environment:** Visual Studio Code
- **Version Control:** Git and GitHub
- **Testing:** Automated C++ test programs
- **Data Storage:** Local text files

## Project Structure

```text
Bank_Management_SE/
├── main.cpp
├── account.cpp
├── account.h
├── account_number.cpp
├── account_number.h
├── transactions.cpp
├── transactions.h
├── balance_enquiry.cpp
├── balance_enquiry.h
├── mini_statement.cpp
├── mini_statement.h
├── auth.h
├── storage.cpp
├── storage.h
├── tests.cpp
├── tests_fr2.cpp
├── tests_fr3.cpp
├── tests_fr4.cpp
├── tests_fr5.cpp
├── tests_fr6.cpp
├── tests_fr13.cpp
├── README.md
└── .gitignore
```

*The repository may contain additional supporting files. The authentication interface is present, but its implementation is not yet available.*

## Getting Started

### Prerequisites

Install:

- Git
- A C++17-compatible compiler, such as GCC or MSVC
- Visual Studio Code or another C++ development environment

### Clone the repository

```bash
git clone https://github.com/Lilac3010/Bank_Management_SE.git
cd Bank_Management_SE
```

### Compile and run tests

Each test suite must be compiled separately because each test program has its own `main()` function and may provide its own test doubles.

For example, to compile and run the account-management tests using GCC:

```bash
g++ -std=c++17 -Wall -Wextra tests.cpp account.cpp -o tests.exe
```

On Windows PowerShell:

```powershell
.\tests.exe
```

Refer to the Testing section below for the commands for the other test suites.

## Testing

The project uses automated tests to verify individual modules and their interactions with supporting components.

### Available test suites

| Test file | Module tested | Build command using GCC |
|---|---|---|
| `tests.cpp` | Account creation | `g++ -std=c++17 -Wall -Wextra tests.cpp account.cpp -o tests.exe` |
| `tests_fr2.cpp` | Account number generation | `g++ -std=c++17 -Wall -Wextra tests_fr2.cpp account.cpp account_number.cpp -o tests_fr2.exe` |
| `tests_fr4.cpp` | Withdrawal | `g++ -std=c++17 -Wall -Wextra tests_fr4.cpp transactions.cpp -o tests_fr4.exe` |
| `tests_fr5.cpp` | Balance enquiry | `g++ -std=c++17 -Wall -Wextra tests_fr5.cpp balance_enquiry.cpp -o tests_fr5.exe` |
| `tests_fr6.cpp` | Mini-statement | `g++ -std=c++17 -Wall -Wextra tests_fr6.cpp mini_statement.cpp -o tests_fr6.exe` |

To run a test executable in PowerShell, use its filename with the `.\` prefix. For example:

```powershell
.\tests_fr5.exe
```

### Persistent storage tests

The storage test suite uses the real storage implementation and exercises account and transaction persistence.

Compile it using:

```bash
g++ -std=c++17 -Wall -Wextra -o tests_fr13.exe tests_fr13.cpp storage.cpp account.cpp
