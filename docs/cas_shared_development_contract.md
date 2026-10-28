# CaS shared development contract

Read this with [architecture and member roles](cas_project_architecture_roles.md). CaS is a C++17 command-line program: these are C++ function interfaces, **not HTTP endpoints**. Freeze the common model and public signatures before creating the four feature branches. The declarations below specify headers to implement; they do not claim the source files already exist.

## Shared model

Place the common declarations in one `BankTypes.hpp` on the baseline branch. All module headers include it; feature branches must not redefine the types. Private helpers can remain inside their owner modules. Use `std::int64_t` minor units for money so balance changes avoid floating-point drift. For PHP, USD, and EUR, 100 minor units equal one currency unit; JPY has zero decimal places.

```cpp
namespace cas {
using Money = std::int64_t;
enum class Role { Client, Admin };
enum class AccountType { Checking, Savings, TimeDeposit };
enum class AccountStatus { Active, Frozen, Closed };
enum class TransactionType { OpeningDeposit, Deposit, Withdrawal, Transfer };
enum class Currency { PHP, USD, EUR, JPY };

struct Account {
    std::string accountId;       // ACC-000001 for a client
    std::string holderName;
    std::string pinDigest;       // Opaque to storage and business logic
    Role role;
    AccountType type;
    AccountStatus status;
    Money balanceCents;          // PHP minor units
    Money minimumBalanceCents;   // PHP minor units
    int annualInterestBps;       // 100 basis points = 1%
};
struct Transaction {
    std::string transactionId;   // TXN-000001
    std::string occurredAt;      // ISO 8601 UTC: YYYY-MM-DDThh:mm:ssZ
    TransactionType type;
    std::string fromAccountId;   // Empty for opening deposit/deposit
    std::string toAccountId;     // Empty for withdrawal
    Money amountCents;           // Always positive, in PHP minor units
    Money fromBalanceAfterCents; // Meaningful when fromAccountId is set
    Money toBalanceAfterCents;   // Meaningful when toAccountId is set
};
struct AuditEntry {
    std::string occurredAt;      // ISO 8601 UTC
    std::string adminAccountId, targetAccountId;
    AccountStatus previousStatus, nextStatus;
};
struct BankData {
    std::vector<Account> accounts;
    std::vector<Transaction> transactions;
};
struct OperationResult {
    bool ok;
    std::string message;
    std::string referenceId; // New account ID or transaction ID; empty on failure
};
struct AccountSummary {
    bool found;
    std::string accountId;
    AccountType type;
    AccountStatus status;
    Money balanceCents;
    Money minimumBalanceCents;
    int annualInterestBps;
};
struct FxQuote {
    bool ok;
    Currency from, to;
    Money inputMinorUnits, outputMinorUnits;
    std::string rateDescription;
};
struct LoadResult { bool ok; BankData data; std::string message; };
struct SaveResult { bool ok; std::string message; };
struct ReportResult { bool ok; std::string message; };
struct TransactionFilter {
    std::string accountId; // Required; caller enforces authorization
    std::optional<std::string> startDate, endDate; // Inclusive YYYY-MM-DD
    std::optional<TransactionType> type;
    std::optional<Money> minAmountCents, maxAmountCents; // Inclusive
};
struct LoginResult {
    bool ok;
    Role role;
    std::string accountId;
    std::string message;
};
}
```

Add the required standard-library includes in the actual header. Default-initialize enum and numeric fields in the implementation header so a failed result never exposes indeterminate data. `AccountSummary::found == false` means the ID does not exist. Keep account and transaction IDs stable across restarts by finding the highest existing numeric suffix before allocating the next one; reject overflow or duplicate IDs.

An admin may be stored as an `Account` with `role == Admin`, but is not a deposit/withdrawal target. Member 1 documents and implements a first-run admin bootstrap path in its PR; it must not ship an undisclosed hardcoded credential.

## Public module interfaces

Use these names in `namespace cas`. A rejected state-changing operation returns `ok == false`, a useful message, an empty reference ID, and **does not modify** its input snapshot. A successful balance change appends its ledger row before returning. None of Member 2's functions perform file or console I/O.

```cpp
// Member 2 — TransactionEngine.hpp
OperationResult createAccount(BankData& data, const std::string& holderName,
                              AccountType type, const std::string& pinDigest,
                              Money initialDepositCents);
AccountSummary getAccountSummary(const BankData& data, const std::string& accountId);
OperationResult deposit(BankData& data, const std::string& accountId, Money amountCents);
OperationResult withdraw(BankData& data, const std::string& accountId, Money amountCents);
OperationResult transfer(BankData& data, const std::string& senderId,
                         const std::string& receiverId, Money amountCents);
OperationResult updateAccountStatus(BankData& data, const std::string& accountId,
                                    AccountStatus nextStatus);
FxQuote convertCurrency(Money inputMinorUnits, Currency from, Currency to);

// Member 3 — FileStorage.hpp and InputValidator.hpp
LoadResult loadSnapshot(const std::filesystem::path& path);
SaveResult saveSnapshot(const std::filesystem::path& path, const BankData& data);
std::vector<Transaction> filterTransactions(const BankData& data,
                                             const TransactionFilter& filter);
SaveResult appendAuditLog(const std::filesystem::path& path, const AuditEntry& entry);
std::vector<AuditEntry> readAuditLogs(const std::filesystem::path& path);
int getValidInt(const std::string& prompt, int minInclusive, int maxInclusive);
Money getValidMoney(const std::string& prompt, Money minInclusive);
std::string getValidPin(const std::string& prompt);

// Member 4 — ReportGenerator.hpp
ReportResult renderAsciiBalanceChart(const BankData& data, const std::string& accountId,
                                     std::ostream& output);
ReportResult exportStatement(const BankData& data, const std::string& accountId,
                             const std::filesystem::path& outputPath);
ReportResult generateReceipt(const BankData& data, const std::string& transactionId,
                             const std::filesystem::path& outputPath);

// Member 1 — Authentication.hpp
std::string digestPin(const std::string& pin);
LoginResult login(const BankData& data, const std::string& accountId,
                  const std::string& pin);
```

Member 1 verifies the PIN digest and checks the role before calling a protected function. PIN input is a four- or six-digit string, including possible leading zeroes. A failed login must not reveal whether the account ID or PIN was wrong. Member 1 chooses and documents the digest format before persistent accounts are created; Member 3 merely round-trips the opaque value.

## Required behavior and handoffs

| Flow | Contract |
| --- | --- |
| Registration | Member 1 validates the name, product, PIN, and initial deposit; `createAccount` assigns the ID, product constants, and an `OpeningDeposit` transaction when the deposit is positive. Success is shown only after Member 3 saves the candidate snapshot. |
| Deposit | An active client account and a positive amount are required. Check for `Money` overflow. Add one `Deposit` row with the new balance. |
| Withdrawal | An active client account and a positive amount are required. The resulting balance must be at least `minimumBalanceCents`. Add one `Withdrawal` row. |
| Transfer | Sender and receiver must be different active client accounts. Validate both IDs, amount, overflow, and sender floor before changing either balance. Add one `Transfer` row with both IDs and both resulting balances. |
| Account status | Member 1 checks admin authorization. Member 2 allows `Active` to `Frozen` or `Closed`, and `Frozen` to `Active` or `Closed`; `Closed` is terminal. A status change adds no financial ledger row. After saving the snapshot, Member 1 asks Member 3 to append an `AuditEntry`. |
| FX quote | Member 2 uses named fixed demo rates, rounds once to the destination currency's minor unit, and returns a quote only. It must not change `BankData`. |
| History | A transaction matches an account if either account ID field matches. A transfer appears once in each participant's history. Return chronological results; inclusive filters are optional. |
| Chart and exports | Member 4 reads the active snapshot only. Client requests are scoped by Member 1 to the authenticated ID. Missing IDs and write failures return a failed `ReportResult`. |

The account product constants (minimum opening deposit, minimum ongoing balance, and annual interest basis points) and FX demo rates must be named and documented in Member 2's PR before acceptance. A summary reports the stored rate; it does not silently credit interest. Time deposits must not support withdrawal or transfer until a maturity rule is defined in a reviewed contract update.

For each state change, Member 1 uses this sequence:

```text
candidate = copy(currentBankData)
result = Member 2 operation(candidate, ...)
if !result.ok: show error; keep currentBankData
save = Member 3 saveSnapshot(candidate)
if !save.ok: show save error; keep currentBankData
currentBankData = move(candidate); show success
```

## Files and persistence

`data/bank_state.txt` is the **single authoritative snapshot** for both accounts and transactions. It starts with a version header and contains `ACCOUNT` and `TRANSACTION` records. Member 3 defines text-field escaping beside the serializer and round-trips every shared-model field exactly. Unknown versions, invalid enums, duplicate IDs, and truncated records are load errors; a missing file means a new installation. Do not overwrite a malformed existing file with an empty state.

`saveSnapshot` writes a complete temporary file in the same directory, closes and checks it, then replaces the authoritative snapshot. A failed temporary write leaves the old snapshot readable. Member 3 appends status-change audit entries to `data/audit.log` when Member 1 calls `appendAuditLog` after a successful snapshot save. If audit append fails, Member 1 reports that the status change was saved but its audit entry was not; it must not claim the status change was rolled back. Reports go to `reports/Statement_<accountId>.txt` and `reports/Receipt_<transactionId>.txt`; Member 4 creates that directory as needed. No module embeds an absolute machine-specific path.

## Ready to branch

Commit this contract and a matching `BankTypes.hpp` to `main`, then have all four members branch from that revision. That small baseline is the only sequential handoff. If the team changes a shared model, signature, file rule, or product constant, review the contract update and notify every affected branch owner before code depends on it.
