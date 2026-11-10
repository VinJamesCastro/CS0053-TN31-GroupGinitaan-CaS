#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cas {

using Money = std::int64_t;

enum class Role { Client, Admin };
enum class AccountType { Checking, Savings, TimeDeposit };
enum class AccountStatus { Active, Frozen, Closed };
enum class TransactionType { OpeningDeposit, Deposit, Withdrawal, Transfer };
enum class Currency { PHP, USD, EUR, JPY };

struct Account {
    std::string accountId;
    std::string holderName;
    std::string pinDigest;
    Role role = Role::Client;
    AccountType type = AccountType::Checking;
    AccountStatus status = AccountStatus::Active;
    Money balanceCents = 0;
    Money minimumBalanceCents = 0;
    int annualInterestBps = 0;
};

struct Transaction {
    std::string transactionId;
    std::string occurredAt;
    TransactionType type = TransactionType::Deposit;
    std::string fromAccountId;
    std::string toAccountId;
    Money amountCents = 0;
    Money fromBalanceAfterCents = 0;
    Money toBalanceAfterCents = 0;
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
    std::string referenceId;
};

struct AccountSummary {
    bool found = false;
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

struct LoadResult { bool ok = false; BankData data; std::string message; };
struct SaveResult { bool ok = false; std::string message; };
struct ReportResult { bool ok = false; std::string message; };

struct TransactionFilter {
    std::string accountId;
    std::optional<std::string> startDate;
    std::optional<std::string> endDate;
    std::optional<TransactionType> type;
    std::optional<Money> minAmountCents;
    std::optional<Money> maxAmountCents;
};

struct LoginResult {
    bool ok = false;
    Role role = Role::Client;
    std::string accountId;
    std::string message;
};

} // namespace cas
