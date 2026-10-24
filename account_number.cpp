#include "account.h"

#include <limits>

namespace {

// First number issued when no accounts exist (documented in README).
const int FIRST_ACCOUNT_NUMBER = 100001;

} // namespace

// FR-2 - one more than the highest number in `existing`, counting ACTIVE and CLOSED
// accounts, so no stored number is ever reissued. Returns 0 when no higher number
// fits in an int; FR-1 treats 0 as a failure and saves nothing.
int generateAccountNumber(const std::vector<Account>& existing) {
    int highest = FIRST_ACCOUNT_NUMBER - 1;
    for (const Account& account : existing)
        if (account.accountNumber > highest) highest = account.accountNumber;
    if (highest == std::numeric_limits<int>::max()) return 0;
    return highest + 1;
}
