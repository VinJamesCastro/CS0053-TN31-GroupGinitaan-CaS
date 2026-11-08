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

// Fixed simulation rates expressed as PHP centavos per major currency unit.
// These are demo settings, not live market rates.
constexpr std::uint64_t phpCentsPerPhp = 100;
constexpr std::uint64_t phpCentsPerUsd = 5600;
constexpr std::uint64_t phpCentsPerEur = 6000;
constexpr std::uint64_t phpCentsPerJpy = 40;

struct CurrencyRules {
    const char* code;
    std::uint64_t phpCentsPerMajor;
    std::uint64_t minorUnitsPerMajor;
};

constexpr CurrencyRules phpCurrency{"PHP", phpCentsPerPhp, 100};
constexpr CurrencyRules usdCurrency{"USD", phpCentsPerUsd, 100};
constexpr CurrencyRules eurCurrency{"EUR", phpCentsPerEur, 100};
constexpr CurrencyRules jpyCurrency{"JPY", phpCentsPerJpy, 1};

// The remainder multiplication in convertCurrency() is bounded by these
// settings. Revisit this bound when changing the supported demo rates.
constexpr std::uint64_t largestPhpCentsPerMajor = 6000;
constexpr std::uint64_t largestMinorUnitsPerMajor = 100;
constexpr std::uint64_t largestConversionFactor =
    largestPhpCentsPerMajor * largestMinorUnitsPerMajor;
static_assert(phpCentsPerPhp > 0 && phpCentsPerUsd > 0 &&
              phpCentsPerEur > 0 && phpCentsPerJpy > 0 &&
              phpCentsPerPhp <= largestPhpCentsPerMajor &&
              phpCentsPerUsd <= largestPhpCentsPerMajor &&
              phpCentsPerEur <= largestPhpCentsPerMajor &&
              phpCentsPerJpy <= largestPhpCentsPerMajor);
static_assert(phpCurrency.minorUnitsPerMajor > 0 &&
              usdCurrency.minorUnitsPerMajor > 0 &&
              eurCurrency.minorUnitsPerMajor > 0 &&
              jpyCurrency.minorUnitsPerMajor > 0 &&
              phpCurrency.minorUnitsPerMajor <= largestMinorUnitsPerMajor &&
              usdCurrency.minorUnitsPerMajor <= largestMinorUnitsPerMajor &&
              eurCurrency.minorUnitsPerMajor <= largestMinorUnitsPerMajor &&
              jpyCurrency.minorUnitsPerMajor <= largestMinorUnitsPerMajor);
static_assert(largestConversionFactor <=
              std::numeric_limits<std::uint64_t>::max() / largestConversionFactor);

const CurrencyRules* findCurrencyRules(Currency currency) {
    switch (currency) {
    case Currency::PHP:
        return &phpCurrency;
    case Currency::USD:
        return &usdCurrency;
    case Currency::EUR:
        return &eurCurrency;
    case Currency::JPY:
        return &jpyCurrency;
    }
    return nullptr;
}

bool isValidStatus(AccountStatus status) {
    return status == AccountStatus::Active || status == AccountStatus::Frozen ||
           status == AccountStatus::Closed;
}

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

bool findClientAccount(const BankData& data, const std::string& accountId,
                       std::size_t& index, std::string& error) {
    if (accountId.empty()) {
        error = "Account ID is required.";
        return false;
    }

    bool found = false;
    for (std::size_t position = 0; position < data.accounts.size(); ++position) {
        if (data.accounts[position].accountId == accountId) {
            if (found) {
                error = "Duplicate account ID in banking data.";
                return false;
            }
            index = position;
            found = true;
        }
    }
    if (!found) {
        error = "Account not found: " + accountId;
        return false;
    }

    const Account& account = data.accounts[index];
    if (account.role != Role::Client) {
        error = "Operation requires a client account.";
        return false;
    }
    return true;
}

bool findActiveClient(const BankData& data, const std::string& accountId,
                      std::size_t& index, std::string& error) {
    if (!findClientAccount(data, accountId, index, error)) {
        return false;
    }
    const Account& account = data.accounts[index];
    if (account.status != AccountStatus::Active) {
        error = "Account must be active.";
        return false;
    }
    if (findProductRules(account.type) == nullptr || account.balanceCents < 0 ||
        account.minimumBalanceCents < 0) {
        error = "Account contains invalid product or balance data.";
        return false;
    }
    return true;
}

// Call only after validating the accounts and calculating safe final balances.
// Stage the ledger row and balance updates together before publishing the copy.
OperationResult commitTransaction(BankData& data, Transaction transaction,
                                  const std::string& message) {
    std::vector<std::string> transactionIds;
    for (const Transaction& existing : data.transactions) {
        transactionIds.push_back(existing.transactionId);
    }
    if (!makeNextId(transactionIds, "TXN-", transaction.transactionId)) {
        return {false, "Cannot allocate transaction ID: duplicate, invalid, or exhausted IDs.", ""};
    }
    if (!currentUtcTimestamp(transaction.occurredAt)) {
        return {false, "Cannot obtain the transaction timestamp.", ""};
    }

    OperationResult result{true, message, transaction.transactionId};
    BankData candidate = data;
    for (Account& account : candidate.accounts) {
        if (!transaction.fromAccountId.empty() &&
            account.accountId == transaction.fromAccountId) {
            account.balanceCents = transaction.fromBalanceAfterCents;
        }
        if (!transaction.toAccountId.empty() &&
            account.accountId == transaction.toAccountId) {
            account.balanceCents = transaction.toBalanceAfterCents;
        }
    }
    candidate.transactions.push_back(std::move(transaction));
    data.accounts.swap(candidate.accounts);
    data.transactions.swap(candidate.transactions);
    return result;
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

OperationResult deposit(
    BankData& data,
    const std::string& accountId,
    Money amountCents) {

    if (amountCents <= 0) {
        return {false, "Deposit amount must be positive.", ""};
    }
    std::size_t index = 0;
    std::string error;
    if (!findActiveClient(data, accountId, index, error)) {
        return {false, error, ""};
    }

    const Account& account = data.accounts[index];
    if (amountCents > std::numeric_limits<Money>::max() - account.balanceCents) {
        return {false, "Deposit would exceed the supported balance limit.", ""};
    }

    Transaction transaction{};
    transaction.type = TransactionType::Deposit;
    transaction.toAccountId = accountId;
    transaction.amountCents = amountCents;
    transaction.toBalanceAfterCents = account.balanceCents + amountCents;
    return commitTransaction(data, std::move(transaction), "Deposit completed.");
}

OperationResult withdraw(
    BankData& data,
    const std::string& accountId,
    Money amountCents) {

    if (amountCents <= 0) {
        return {false, "Withdrawal amount must be positive.", ""};
    }
    std::size_t index = 0;
    std::string error;
    if (!findActiveClient(data, accountId, index, error)) {
        return {false, error, ""};
    }

    const Account& account = data.accounts[index];
    if (account.type == AccountType::TimeDeposit) {
        return {false, "Time-deposit withdrawals require an approved maturity rule.", ""};
    }
    if (amountCents > account.balanceCents ||
        account.balanceCents - amountCents < account.minimumBalanceCents) {
        return {false, "Withdrawal would violate the account's minimum balance.", ""};
    }

    Transaction transaction{};
    transaction.type = TransactionType::Withdrawal;
    transaction.fromAccountId = accountId;
    transaction.amountCents = amountCents;
    transaction.fromBalanceAfterCents = account.balanceCents - amountCents;
    return commitTransaction(data, std::move(transaction), "Withdrawal completed.");
}

OperationResult transfer(
    BankData& data,
    const std::string& senderId,
    const std::string& receiverId,
    Money amountCents) {

    if (amountCents <= 0) {
        return {false, "Transfer amount must be positive.", ""};
    }
    if (senderId == receiverId) {
        return {false, "Sender and receiver must be different accounts.", ""};
    }

    std::size_t senderIndex = 0;
    std::size_t receiverIndex = 0;
    std::string error;
    if (!findActiveClient(data, senderId, senderIndex, error)) {
        return {false, "Sender: " + error, ""};
    }
    if (!findActiveClient(data, receiverId, receiverIndex, error)) {
        return {false, "Receiver: " + error, ""};
    }

    const Account& sender = data.accounts[senderIndex];
    const Account& receiver = data.accounts[receiverIndex];
    if (sender.type == AccountType::TimeDeposit) {
        return {false, "Outgoing time-deposit transfers require an approved maturity rule.", ""};
    }
    if (amountCents > sender.balanceCents ||
        sender.balanceCents - amountCents < sender.minimumBalanceCents) {
        return {false, "Transfer would violate the sender's minimum balance.", ""};
    }
    if (amountCents > std::numeric_limits<Money>::max() - receiver.balanceCents) {
        return {false, "Transfer would exceed the receiver's balance limit.", ""};
    }

    Transaction transaction{};
    transaction.type = TransactionType::Transfer;
    transaction.fromAccountId = senderId;
    transaction.toAccountId = receiverId;
    transaction.amountCents = amountCents;
    transaction.fromBalanceAfterCents = sender.balanceCents - amountCents;
    transaction.toBalanceAfterCents = receiver.balanceCents + amountCents;
    return commitTransaction(data, std::move(transaction), "Transfer completed.");
}

OperationResult updateAccountStatus(
    BankData& data,
    const std::string& accountId,
    AccountStatus nextStatus) {

    // The caller must verify admin authorization before calling this function.
    if (!isValidStatus(nextStatus)) {
        return {false, "Invalid requested account status.", ""};
    }
    std::size_t index = 0;
    std::string error;
    if (!findClientAccount(data, accountId, index, error)) {
        return {false, error, ""};
    }

    const AccountStatus current = data.accounts[index].status;
    if (!isValidStatus(current)) {
        return {false, "Account contains an invalid current status.", ""};
    }
    if (current == AccountStatus::Closed) {
        return {false, "Closed accounts cannot change status.", ""};
    }
    if (current == nextStatus) {
        return {false, "Account already has the requested status.", ""};
    }

    const bool allowed =
        (current == AccountStatus::Active &&
         (nextStatus == AccountStatus::Frozen || nextStatus == AccountStatus::Closed)) ||
        (current == AccountStatus::Frozen &&
         (nextStatus == AccountStatus::Active || nextStatus == AccountStatus::Closed));
    if (!allowed) {
        return {false, "Account status transition is not allowed.", ""};
    }

    // Construct the result before mutation. No new account/transaction ID is
    // allocated. The caller handles snapshot saving and the subsequent audit.
    OperationResult result{true, "Account status updated.", ""};
    data.accounts[index].status = nextStatus;
    return result;
}

FxQuote convertCurrency(Money inputMinorUnits, Currency from, Currency to) {
    FxQuote quote{};
    quote.from = from;
    quote.to = to;
    quote.inputMinorUnits = inputMinorUnits;

    if (inputMinorUnits < 0) {
        quote.rateDescription = "FX input amount must be nonnegative.";
        return quote;
    }
    const CurrencyRules* source = findCurrencyRules(from);
    const CurrencyRules* destination = findCurrencyRules(to);
    if (source == nullptr || destination == nullptr) {
        quote.rateDescription = "Invalid source or destination currency.";
        return quote;
    }

    // Exact rational form of the major-unit conversion, with no intermediate
    // rounding: outputMinor = round(inputMinor * numerator / denominator).
    const std::uint64_t numerator =
        source->phpCentsPerMajor * destination->minorUnitsPerMajor;
    const std::uint64_t denominator =
        source->minorUnitsPerMajor * destination->phpCentsPerMajor;
    const auto input = static_cast<std::uint64_t>(inputMinorUnits);
    constexpr auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<Money>::max());

    // Split the input before multiplication so even Money::max() can be
    // converted when its destination value fits, including same-currency quotes.
    const std::uint64_t whole = input / denominator;
    if (whole > maximum / numerator) {
        quote.rateDescription = "Converted amount exceeds the supported Money range.";
        return quote;
    }
    const std::uint64_t wholeOutput = whole * numerator;
    const std::uint64_t scaledRemainder = (input % denominator) * numerator;
    std::uint64_t fractionalOutput = scaledRemainder / denominator;
    const std::uint64_t roundingRemainder = scaledRemainder % denominator;

    // Nearest destination minor unit; exact halves round up for nonnegative input.
    if (roundingRemainder >= denominator - roundingRemainder) {
        ++fractionalOutput;
    }
    if (fractionalOutput > maximum - wholeOutput) {
        quote.rateDescription = "Converted amount exceeds the supported Money range.";
        return quote;
    }

    std::ostringstream description;
    description << "Fixed demo rates: 1 " << source->code << " = PHP "
                << source->phpCentsPerMajor / 100 << '.' << std::setfill('0')
                << std::setw(2) << source->phpCentsPerMajor % 100
                << "; 1 " << destination->code << " = PHP "
                << destination->phpCentsPerMajor / 100 << '.'
                << std::setw(2) << destination->phpCentsPerMajor % 100
                << "; nearest destination minor unit, halves round up.";
    quote.rateDescription = description.str();
    quote.outputMinorUnits = static_cast<Money>(wholeOutput + fractionalOutput);
    quote.ok = true;
    return quote;
}

} // namespace cas