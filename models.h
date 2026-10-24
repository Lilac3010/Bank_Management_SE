#ifndef MODELS_H
#define MODELS_H

#include <string>

struct Account {
    std::string accountNumber;
    std::string name;
    std::string address;
    std::string contactNumber;
    std::string pin;
    double balance = 0.0;
    std::string status;
};

struct Transaction {
    std::string transactionId;
    std::string accountNumber;
    std::string type;
    double amount = 0.0;
    std::string dateTime;
    double resultingBalance = 0.0;
};

struct Employee {
    std::string username;
    std::string password;
};

#endif