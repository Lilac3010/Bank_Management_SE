#ifndef MENU_H
#define MENU_H

// Console UI - the numbered main menu and, after employee login, the Employee Menu.
// Returns when the user chooses 0 (Exit) or input ends. Implemented in menu.cpp.
// Leaving the Employee Menu (0 or end of input) logs the employee out.
void runMainMenu();

// Console UI - command line of bank.exe (FR-10):
//   bank.exe                  -> runMainMenu(); returns 0
//   bank.exe --add-employee   -> employee setup; returns 0 if an employee was added, else 1
//   anything else             -> prints the usage; returns 2
int runApplication(int argc, char* argv[]);

#endif
