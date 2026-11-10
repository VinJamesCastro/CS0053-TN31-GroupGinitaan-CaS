#include "TransactionEngine.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {

// Capture every model field so rejection checks detect changes to existing data.
std::string snapshot(const cas::BankData& data) {
    std::ostringstream out;
    for (const auto& a : data.accounts) {
        out << "A " << std::quoted(a.accountId) << ' '
            << std::quoted(a.holderName) << ' ' << std::quoted(a.pinDigest) << ' '
            << static_cast<int>(a.role) << ' ' << static_cast<int>(a.type) << ' '
            << static_cast<int>(a.status) << ' ' << a.balanceCents << ' '
            << a.minimumBalanceCents << ' ' << a.annualInterestBps << '\n';
    }
    for (const auto& t : data.transactions) {
        out << "T " << std::quoted(t.transactionId) << ' '
            << std::quoted(t.occurredAt) << ' ' << static_cast<int>(t.type) << ' '
            << std::quoted(t.fromAccountId) << ' ' << std::quoted(t.toAccountId)
            << ' ' << t.amountCents << ' ' << t.fromBalanceAfterCents << ' '
            << t.toBalanceAfterCents << '\n';
    }
    return out.str();
}

} // namespace

int main() {
    using namespace cas;
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool passed, const std::string& description) {
        ++checks;
        if (!passed) {
            ++failures;
        }
        std::cout << (passed ? "PASS: " : "FAIL: ") << description << '\n';
    };

    BankData data;
    const auto first = createAccount(
        data, "Charles Taasin", AccountType::Checking, "opaque-test-digest", 100000);
    check(first.ok && first.referenceId == "ACC-000001" &&
              data.accounts.size() == 1 && data.transactions.size() == 1,
          "Create first account: ACC-000001, one account, one ledger row");
    if (!first.ok || data.accounts.empty() || data.transactions.empty()) {
        return 1;
    }

    const auto& account = data.accounts.front();
    check(account.holderName == "Charles Taasin" &&
              account.pinDigest == "opaque-test-digest" &&
              account.role == Role::Client && account.status == AccountStatus::Active &&
              account.type == AccountType::Checking && account.balanceCents == 100000 &&
              account.minimumBalanceCents == 50000 && account.annualInterestBps == 0,
          "Checking stores PHP 1000.00, PHP 500.00 floor, 0 bps, unchanged digest");

    const auto& opening = data.transactions.front();
    check(opening.transactionId == "TXN-000001" &&
              opening.type == TransactionType::OpeningDeposit &&
              opening.fromAccountId.empty() && opening.toAccountId == first.referenceId &&
              opening.amountCents == 100000 && opening.fromBalanceAfterCents == 0 &&
              opening.toBalanceAfterCents == 100000,
          "Opening ledger row contains correct ID, type, amount, and balances");
    std::tm parsed{};
    std::istringstream timestamp(opening.occurredAt);
    timestamp >> std::get_time(&parsed, "%Y-%m-%dT%H:%M:%SZ");
    check(!timestamp.fail() && opening.occurredAt.size() == 20,
          "Opening timestamp has valid YYYY-MM-DDThh:mm:ssZ format");

    const std::string beforeSummary = snapshot(data);
    const auto summary = getAccountSummary(data, first.referenceId);
    check(summary.found && summary.accountId == first.referenceId &&
              summary.type == account.type && summary.status == account.status &&
              summary.balanceCents == 100000 && summary.minimumBalanceCents == 50000 &&
              summary.annualInterestBps == 0 && snapshot(data) == beforeSummary,
          "Summary reports stored account details without changing data");
    const auto missing = getAccountSummary(data, "ACC-999999");
    check(!missing.found && missing.accountId.empty() && missing.balanceCents == 0 &&
              snapshot(data) == beforeSummary,
          "Missing summary returns found=false and initialized fields");

    const auto rejected = [&](BankData sample, const std::string& name,
                              AccountType type, const std::string& digest,
                              Money amount, const std::string& description) {
        const std::string before = snapshot(sample);
        const auto result = createAccount(sample, name, type, digest, amount);
        check(!result.ok && !result.message.empty() && result.referenceId.empty() &&
                  snapshot(sample) == before,
              description + "; failed result and complete snapshot unchanged");
    };

    rejected(data, "", AccountType::Checking, "digest", 100000, "Reject empty name");
    rejected(data, " \t\n", AccountType::Checking, "digest", 100000,
             "Reject whitespace-only name");
    rejected(data, "Charles", AccountType::Checking, "", 100000, "Reject empty digest");
    rejected(data, "Charles", static_cast<AccountType>(99), "digest", 100000,
             "Reject unknown account type");
    rejected(data, "Charles", AccountType::Checking, "digest", 49999,
             "Reject opening amount one centavo below minimum");
    rejected(data, "Charles", AccountType::Checking, "digest", -1,
             "Reject negative opening amount");
    rejected(data, "Charles", AccountType::Checking, "digest", 0,
             "Reject zero opening amount for current products");

    BankData boundaries;
    const auto checking = createAccount(
        boundaries, "Checking", AccountType::Checking, "digest", 50000);
    const auto savings = createAccount(
        boundaries, "Savings", AccountType::Savings, "digest", 10000);
    const auto timeDeposit = createAccount(
        boundaries, "Time", AccountType::TimeDeposit, "digest", 100000);
    check(checking.ok && savings.ok && timeDeposit.ok && boundaries.accounts.size() == 3,
          "All three products accept their exact minimum opening deposits");
    if (boundaries.accounts.size() == 3) {
        check(boundaries.accounts[1].minimumBalanceCents == 10000 &&
                  boundaries.accounts[1].annualInterestBps == 150 &&
                  boundaries.accounts[2].minimumBalanceCents == 100000 &&
                  boundaries.accounts[2].annualInterestBps == 300 &&
                  savings.referenceId == "ACC-000002" &&
                  timeDeposit.referenceId == "ACC-000003" &&
                  boundaries.transactions.size() == 3,
              "Products store correct floors/rates and allocate successive IDs");
    }

    BankData loaded = data;
    loaded.accounts[0].accountId = "ACC-000042";
    loaded.transactions[0].transactionId = "TXN-000088";
    loaded.transactions[0].toAccountId = "ACC-000042";
    Account admin{};
    admin.accountId = "ADM-000001";
    admin.role = Role::Admin;
    loaded.accounts.push_back(admin);
    const auto next = createAccount(
        loaded, "Next", AccountType::Checking, "digest", 50000);
    check(next.ok && next.referenceId == "ACC-000043" &&
              loaded.transactions.back().transactionId == "TXN-000089" &&
              loaded.accounts.front().accountId == "ACC-000042",
          "Loaded ID gaps/admin IDs: next account 43 and transaction 89");

    BankData invalid = data;
    invalid.accounts.push_back(invalid.accounts.front());
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject duplicate existing account IDs");
    invalid = data;
    invalid.transactions.push_back(invalid.transactions.front());
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject duplicate existing transaction IDs without adding an account");
    invalid = data;
    invalid.accounts[0].accountId = "ACC-18446744073709551615";
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject exhausted account ID suffix");
    invalid = data;
    invalid.transactions[0].transactionId = "TXN-18446744073709551615";
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject exhausted transaction ID suffix without adding an account");
    invalid = data;
    invalid.accounts[0].accountId = "ACC-18446744073709551616";
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject account suffix beyond integer range");
    invalid = data;
    invalid.transactions[0].transactionId = "TXN-invalid";
    rejected(invalid, "Next", AccountType::Checking, "digest", 50000,
             "Reject malformed transaction suffix");

    BankData maximum;
    const auto largest = createAccount(
        maximum, "Maximum", AccountType::Checking, "digest",
        std::numeric_limits<Money>::max());
    check(largest.ok && maximum.accounts[0].balanceCents ==
                              std::numeric_limits<Money>::max() &&
              maximum.transactions[0].amountCents == std::numeric_limits<Money>::max(),
          "Largest Money opening value is stored exactly without arithmetic overflow");

    std::cout << "\n" << checks << " checks; " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}