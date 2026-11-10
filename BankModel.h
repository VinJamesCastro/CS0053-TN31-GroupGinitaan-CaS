#ifndef CAS_BANK_MODEL_H
#define CAS_BANK_MODEL_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cas {

// PHP, USD, and EUR use 100 minor units per currency unit; JPY uses 1.
using Money = std::int64_t;

enum class Role { Client, Admin };
enum class AccountType { Checking, Savings, TimeDeposit };
enum class AccountStatus { Active, Frozen, Closed };
enum class TransactionType { OpeningDeposit, Deposit, Withdrawal, Transfer };
enum class Currency { PHP, USD, EUR, JPY };

struct Account {
    std::string accountId; // Client IDs use ACC-000001, ACC-000002, etc.
    std::string holderName;
    std::string pinDigest; // Supplied by authentication; the engine stores it unchanged.
    Role role = Role::Client;
    AccountType type = AccountType::Checking;
    AccountStatus status = AccountStatus::Active;
    Money balanceCents = 0; // PHP centavos.
    Money minimumBalanceCents = 0;
    int annualInterestBps = 0; // 100 basis points = 1%.
};

struct Transaction {
    std::string transactionId; // TXN-000001, TXN-000002, etc.
    std::string occurredAt; // UTC timestamp: YYYY-MM-DDThh:mm:ssZ.
    TransactionType type = TransactionType::OpeningDeposit;
    std::string fromAccountId; // Empty for opening deposits and deposits.
    std::string toAccountId; // Empty for withdrawals.
    Money amountCents = 0; // Must be positive for an actual ledger entry.
    Money fromBalanceAfterCents = 0; // Used when fromAccountId is set.
    Money toBalanceAfterCents = 0; // Used when toAccountId is set.
};

struct AuditEntry {
    std::string occurredAt;
    std::string adminAccountId;
    std::string targetAccountId;
    AccountStatus previousStatus = AccountStatus::Active;
    AccountStatus nextStatus = AccountStatus::Active;
};

struct BankData {
    std::vector<Account> accounts;
    std::vector<Transaction> transactions;
};

struct OperationResult {
    bool ok = false;
    std::string message;
    std::string referenceId; // New account/transaction ID; empty on failure.
};

struct AccountSummary {
    bool found = false; // False means the requested account was not found.
    std::string accountId;
    AccountType type = AccountType::Checking;
    AccountStatus status = AccountStatus::Active;
    Money balanceCents = 0;
    Money minimumBalanceCents = 0;
    int annualInterestBps = 0;
};

struct FxQuote {
    bool ok = false;
    Currency from = Currency::PHP;
    Currency to = Currency::PHP;
    Money inputMinorUnits = 0;
    Money outputMinorUnits = 0;
    std::string rateDescription;
};

struct LoadResult {
    bool ok = false;
    BankData data;
    std::string message;
};

struct SaveResult {
    bool ok = false;
    std::string message;
};

struct ReportResult {
    bool ok = false;
    std::string message;
};

struct TransactionFilter {
    std::string accountId; // Required; the caller enforces authorization.
    std::optional<std::string> startDate; // Inclusive YYYY-MM-DD.
    std::optional<std::string> endDate; // Inclusive YYYY-MM-DD.
    std::optional<TransactionType> type;
    std::optional<Money> minAmountCents; // Inclusive.
    std::optional<Money> maxAmountCents; // Inclusive.
};

struct LoginResult {
    bool ok = false;
    Role role = Role::Client;
    std::string accountId;
    std::string message;
};

} // namespace cas

#endif // CAS_BANK_MODEL_H
