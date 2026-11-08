#include "TransactionEngine.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace cas {

// Helpers and product settings inside this namespace belong to this file.
namespace {

struct ProductRules {
    Money minimumOpeningCents;
    Money minimumBalanceCents;
    int annualInterestBps;
};

// Proposed simulation settings for team review, used by createAccount().
// Amounts are in PHP centavos; 100 interest basis points equal 1%.
// Checking: PHP 500.00 opening/minimum balance, 0% annual interest.
constexpr ProductRules checkingRules{50000, 50000, 0};

// Savings: PHP 100.00 opening/minimum balance, 1.5% annual interest.
constexpr ProductRules savingsRules{10000, 10000, 150};

// Time deposit: PHP 1,000.00 opening/minimum balance, 3% annual interest.
constexpr ProductRules timeDepositRules{100000, 100000, 300};

const ProductRules* findProductRules(AccountType type) {
    switch (type) {
    case AccountType::Checking:
        return &checkingRules;
    case AccountType::Savings:
        return &savingsRules;
    case AccountType::TimeDeposit:
        return &timeDepositRules;
    }
    return nullptr;
}

// IDs use at least six digits. Scan existing IDs rather than using record count,
// so gaps in IDs and loading a previous snapshot cannot cause ID reuse.
bool makeNextId(const std::vector<std::string>& existingIds,
                const std::string& prefix,
                std::string& nextId) {
    std::uint64_t highest = 0;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    std::unordered_set<std::string> seen;

    for (const std::string& id : existingIds) {
        if (!seen.insert(id).second) {
            return false;
        }

        // Account data can also contain admin IDs such as ADM-000001.
        if (id.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        if (id.size() == prefix.size()) {
            return false;
        }

        std::uint64_t suffix = 0;
        for (std::size_t index = prefix.size(); index < id.size(); ++index) {
            const char character = id[index];
            if (character < '0' || character > '9') {
                return false;
            }
            const auto digit = static_cast<std::uint64_t>(character - '0');
            if (suffix > (maximum - digit) / 10) {
                return false;
            }
            suffix = suffix * 10 + digit;
        }

        if (suffix == 0) {
            return false;
        }
        highest = std::max(highest, suffix);
    }

    if (highest == maximum) {
        return false;
    }

    std::ostringstream formatted;
    formatted << prefix << std::setfill('0') << std::setw(6) << highest + 1;
    nextId = formatted.str();
    return seen.count(nextId) == 0;
}

bool currentUtcTimestamp(std::string& timestamp) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm* utc = std::gmtime(&time);
    if (utc == nullptr) {
        return false;
    }

    std::ostringstream formatted;
    formatted << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
    if (!formatted) {
        return false;
    }
    timestamp = formatted.str();
    return true;
}

} // namespace

// Public function definitions go after the private namespace above.
OperationResult createAccount(
    BankData& data,
    const std::string& holderName,
    AccountType type,
    const std::string& pinDigest,
    Money initialDepositCents) {

    const bool hasName = std::any_of(
        holderName.begin(), holderName.end(),
        [](unsigned char character) { return !std::isspace(character); });
    if (!hasName) {
        return {false, "Account holder name is required.", ""};
    }
    if (pinDigest.empty()) {
        return {false, "A PIN digest is required.", ""};
    }

    const ProductRules* rules = findProductRules(type);
    if (rules == nullptr) {
        return {false, "Invalid account type.", ""};
    }
    if (initialDepositCents < rules->minimumOpeningCents ||
        initialDepositCents < rules->minimumBalanceCents) {
        return {false, "Opening deposit is below the product minimum.", ""};
    }

    std::vector<std::string> accountIds;
    for (const Account& account : data.accounts) {
        accountIds.push_back(account.accountId);
    }
    std::string accountId;
    if (!makeNextId(accountIds, "ACC-", accountId)) {
        return {false, "Cannot allocate account ID: duplicate, invalid, or exhausted IDs.", ""};
    }

    Account account{};
    account.accountId = accountId;
    account.holderName = holderName;
    account.pinDigest = pinDigest;
    account.role = Role::Client;
    account.type = type;
    account.status = AccountStatus::Active;
    account.balanceCents = initialDepositCents;
    account.minimumBalanceCents = rules->minimumBalanceCents;
    account.annualInterestBps = rules->annualInterestBps;

    Transaction opening{};
    if (initialDepositCents > 0) {
        std::vector<std::string> transactionIds;
        for (const Transaction& transaction : data.transactions) {
            transactionIds.push_back(transaction.transactionId);
        }
        if (!makeNextId(transactionIds, "TXN-", opening.transactionId)) {
            return {false, "Cannot allocate transaction ID: duplicate, invalid, or exhausted IDs.", ""};
        }
        if (!currentUtcTimestamp(opening.occurredAt)) {
            return {false, "Cannot obtain the transaction timestamp.", ""};
        }
        opening.type = TransactionType::OpeningDeposit;
        opening.toAccountId = accountId;
        opening.amountCents = initialDepositCents;
        opening.toBalanceAfterCents = initialDepositCents;
    }

    // Prepare both records on a copy. Publish them together only after every
    // check and allocation has succeeded, preserving data on rejection.
    OperationResult result{true, "Account created.", accountId};
    BankData candidate = data;
    candidate.accounts.push_back(std::move(account));
    if (initialDepositCents > 0) {
        candidate.transactions.push_back(std::move(opening));
    }
    data.accounts.swap(candidate.accounts);
    data.transactions.swap(candidate.transactions);
    return result;
}

AccountSummary getAccountSummary(
    const BankData& data,
    const std::string& accountId) {

    AccountSummary summary{};

    for (const Account& account : data.accounts) {
        if (account.accountId == accountId) {
            summary.found = true;
            summary.accountId = account.accountId;
            summary.type = account.type;
            summary.status = account.status;
            summary.balanceCents = account.balanceCents;
            summary.minimumBalanceCents = account.minimumBalanceCents;
            summary.annualInterestBps = account.annualInterestBps;
            return summary;
        }
    }

    // The default summary has found == false when no account matches.
    return summary;
}

// Subsequent steps implement deposit(), withdraw(), transfer(),
// updateAccountStatus(), and convertCurrency() here, inside namespace cas.

} // namespace cas