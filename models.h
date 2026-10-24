#ifndef MODELS_H
#define MODELS_H

#include <string>

// Account status values
const std::string STATUS_ACTIVE = "ACTIVE";
const std::string STATUS_CLOSED = "CLOSED";

struct Account {
    int accountNumber = 0;
    std::string name;
    std::string address;
    std::string contactNumber;   // digits only, kept as text
    std::string pin;
    long long balance = 0;       // paise (100 paise = 1 rupee)
    std::string status = STATUS_ACTIVE;
};

struct Transaction {
    int transactionId = 0;
    int accountNumber = 0;
    std::string type;            // e.g. "DEPOSIT", "WITHDRAW"
    long long amount = 0;        // paise
    std::string dateTime;        // e.g. "2026-10-24 14:30:00"
};

struct Employee {
    std::string username;
    std::string password;
};

#endif
