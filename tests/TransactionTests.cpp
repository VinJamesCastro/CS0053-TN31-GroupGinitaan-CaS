#include "TransactionEngine.h"

#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {

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

bool validTimestamp(const std::string& value) {
    std::tm time{};
    std::istringstream input(value);
    input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%SZ");
    return value.size() == 20 && !input.fail();
}

} // namespace

int main() {
    using namespace cas;
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool passed, const std::string& label) {
        ++checks;
        failures += !passed;
        std::cout << (passed ? "PASS: " : "FAIL: ") << label << '\n';
    };

    BankData original;
    const auto first = createAccount(
        original, "Sender", AccountType::Checking, "test-digest", 100000);
    const auto second = createAccount(
        original, "Receiver", AccountType::Checking, "test-digest", 100000);
    if (!first.ok || !second.ok || original.accounts.size() != 2) {
        std::cerr << "Account fixture creation failed.\n";
        return 1;
    }
    const std::string sender = first.referenceId;
    const std::string receiver = second.referenceId;

    BankData sample = original;
    auto result = deposit(sample, sender, 25000);
    const auto& depositRow = sample.transactions.back();
    check(result.ok && result.referenceId == "TXN-000003" &&
              sample.accounts[0].balanceCents == 125000 &&
              sample.accounts[1].balanceCents == 100000 &&
              sample.transactions.size() == 3 &&
              depositRow.transactionId == result.referenceId &&
              depositRow.type == TransactionType::Deposit &&
              depositRow.amountCents == 25000 && depositRow.fromAccountId.empty() &&
              depositRow.toAccountId == sender && depositRow.toBalanceAfterCents == 125000 &&
              depositRow.fromBalanceAfterCents == 0 && validTimestamp(depositRow.occurredAt),
          "Deposit PHP 250.00: balance PHP 1250.00 and one complete ledger row");

    sample = original;
    result = withdraw(sample, sender, 50000);
    const auto& withdrawalRow = sample.transactions.back();
    check(result.ok && sample.accounts[0].balanceCents == 50000 &&
              sample.accounts[1].balanceCents == 100000 && sample.transactions.size() == 3 &&
              withdrawalRow.transactionId == result.referenceId &&
              withdrawalRow.type == TransactionType::Withdrawal &&
              withdrawalRow.amountCents == 50000 && withdrawalRow.fromAccountId == sender &&
              withdrawalRow.toAccountId.empty() && withdrawalRow.fromBalanceAfterCents == 50000 &&
              withdrawalRow.toBalanceAfterCents == 0 && validTimestamp(withdrawalRow.occurredAt),
          "Withdraw PHP 500.00: exact minimum retained and one complete ledger row");

    sample = original;
    result = transfer(sample, sender, receiver, 25000);
    const auto& transferRow = sample.transactions.back();
    check(result.ok && sample.accounts[0].balanceCents == 75000 &&
              sample.accounts[1].balanceCents == 125000 && sample.transactions.size() == 3 &&
              transferRow.transactionId == result.referenceId &&
              transferRow.type == TransactionType::Transfer &&
              transferRow.amountCents == 25000 && transferRow.fromAccountId == sender &&
              transferRow.toAccountId == receiver && transferRow.fromBalanceAfterCents == 75000 &&
              transferRow.toBalanceAfterCents == 125000 && validTimestamp(transferRow.occurredAt),
          "Transfer PHP 250.00: sender PHP 750.00, receiver PHP 1250.00, one row");
    check(sample.accounts[0].balanceCents + sample.accounts[1].balanceCents == 200000,
          "Transfer preserves the combined account balance");
    BankData previous = sample;
    previous.accounts[0].balanceCents = original.accounts[0].balanceCents;
    previous.accounts[1].balanceCents = original.accounts[1].balanceCents;
    previous.transactions.pop_back();
    check(snapshot(previous) == snapshot(original),
          "Successful transfer preserves existing ledger rows and account metadata");

    using Action = std::function<OperationResult(BankData&)>;
    const auto reject = [&](BankData data, const Action& action, const std::string& label) {
        const std::string before = snapshot(data);
        const auto rejected = action(data);
        check(!rejected.ok && rejected.referenceId.empty() && !rejected.message.empty() &&
                  snapshot(data) == before,
              label + "; both balances and all stored fields unchanged");
    };
    const Action depositOne = [&](BankData& d) { return deposit(d, sender, 1); };
    const Action withdrawOne = [&](BankData& d) { return withdraw(d, sender, 1); };
    const Action transferOne = [&](BankData& d) { return transfer(d, sender, receiver, 1); };

    for (Money amount : {Money{0}, Money{-1}, std::numeric_limits<Money>::min()}) {
        reject(original, [&](BankData& d) { return deposit(d, sender, amount); },
               "Reject deposit amount " + std::to_string(amount));
        reject(original, [&](BankData& d) { return withdraw(d, sender, amount); },
               "Reject withdrawal amount " + std::to_string(amount));
        reject(original, [&](BankData& d) { return transfer(d, sender, receiver, amount); },
               "Reject transfer amount " + std::to_string(amount));
    }
    reject(original, [&](BankData& d) { return deposit(d, "missing", 1); }, "Unknown deposit ID");
    reject(original, [&](BankData& d) { return withdraw(d, "missing", 1); }, "Unknown withdrawal ID");
    reject(original, [&](BankData& d) { return transfer(d, "missing", receiver, 1); }, "Unknown sender");
    reject(original, [&](BankData& d) { return transfer(d, sender, "missing", 1); }, "Unknown receiver");
    reject(original, [&](BankData& d) { return deposit(d, "", 1); }, "Empty account ID");
    reject(original, [&](BankData& d) { return transfer(d, sender, sender, 1); }, "Self-transfer");

    for (const auto status : {AccountStatus::Frozen, AccountStatus::Closed}) {
        sample = original;
        sample.accounts[0].status = status;
        reject(sample, depositOne, "Inactive deposit target");
        reject(sample, withdrawOne, "Inactive withdrawal target");
        reject(sample, transferOne, "Inactive sender");
        sample = original;
        sample.accounts[1].status = status;
        reject(sample, transferOne, "Inactive receiver cannot debit sender");
    }
    sample = original;
    sample.accounts[0].role = Role::Admin;
    reject(sample, depositOne, "Admin deposit target");
    reject(sample, withdrawOne, "Admin withdrawal target");
    reject(sample, transferOne, "Admin sender");
    sample = original;
    sample.accounts[1].role = Role::Admin;
    reject(sample, transferOne, "Admin receiver");

    sample = original;
    sample.accounts[0].type = AccountType::TimeDeposit;
    reject(sample, withdrawOne, "Time-deposit withdrawal without maturity rule");
    reject(sample, transferOne, "Outgoing time-deposit transfer without maturity rule");
    result = deposit(sample, sender, 1);
    check(result.ok && sample.accounts[0].balanceCents == 100001,
          "Active time-deposit account accepts a deposit");

    reject(original, [&](BankData& d) { return withdraw(d, sender, 50001); }, "Withdrawal below floor");
    reject(original, [&](BankData& d) { return transfer(d, sender, receiver, 50001); }, "Transfer below floor");
    reject(original, [&](BankData& d) { return withdraw(d, sender, std::numeric_limits<Money>::max()); },
           "Withdrawal exceeds available funds without subtraction overflow");
    reject(original, [&](BankData& d) { return transfer(d, sender, receiver, std::numeric_limits<Money>::max()); },
           "Transfer exceeds available funds without subtraction overflow");
    sample = original;
    result = transfer(sample, sender, receiver, 50000);
    check(result.ok && sample.accounts[0].balanceCents == 50000 &&
              sample.accounts[1].balanceCents == 150000,
          "Transfer accepts exact sender minimum boundary");

    sample = original;
    sample.accounts[0].balanceCents = std::numeric_limits<Money>::max();
    reject(sample, depositOne, "Deposit overflow");
    sample = original;
    sample.accounts[1].balanceCents = std::numeric_limits<Money>::max();
    reject(sample, transferOne, "Receiver overflow cannot debit sender");
    sample = original;
    sample.accounts[0].balanceCents = std::numeric_limits<Money>::max() - 1;
    result = deposit(sample, sender, 1);
    check(result.ok && sample.accounts[0].balanceCents == std::numeric_limits<Money>::max(),
          "Deposit accepts exact maximum Money boundary");

    for (int invalidCase = 0; invalidCase < 4; ++invalidCase) {
        sample = original;
        if (invalidCase == 0) sample.accounts[0].balanceCents = -1;
        if (invalidCase == 1) sample.accounts[0].minimumBalanceCents = -1;
        if (invalidCase == 2) sample.accounts[0].type = static_cast<AccountType>(99);
        if (invalidCase == 3) sample.accounts.push_back(sample.accounts[0]);
        reject(sample, depositOne, "Invalid account data: deposit");
        reject(sample, withdrawOne, "Invalid account data: withdrawal");
        reject(sample, transferOne, "Invalid account data: transfer");
    }

    for (int invalidCase = 0; invalidCase < 3; ++invalidCase) {
        sample = original;
        if (invalidCase == 0) sample.transactions.push_back(sample.transactions[0]);
        if (invalidCase == 1) sample.transactions[0].transactionId = "TXN-18446744073709551615";
        if (invalidCase == 2) sample.transactions[0].transactionId = "TXN-bad";
        reject(sample, depositOne, "Unallocatable ledger ID: deposit");
        reject(sample, withdrawOne, "Unallocatable ledger ID: withdrawal");
        reject(sample, transferOne, "Unallocatable ledger ID: transfer");
    }

    sample = original;
    sample.transactions[1].transactionId = "TXN-000042";
    const auto a = deposit(sample, sender, 1);
    const auto b = withdraw(sample, sender, 1);
    const auto c = transfer(sample, sender, receiver, 1);
    check(a.ok && b.ok && c.ok && a.referenceId == "TXN-000043" &&
              b.referenceId == "TXN-000044" && c.referenceId == "TXN-000045" &&
              sample.transactions.size() == 5,
          "Mixed operations allocate successive IDs above the loaded maximum");

    std::cout << "\n" << checks << " checks; " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}