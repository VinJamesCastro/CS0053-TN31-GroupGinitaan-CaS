#include "FileStorage.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    cas::BankData data;

    cas::Transaction deposit{};
    deposit.transactionId = "TXN-000001";
    deposit.occurredAt = "2026-11-01T09:00:00Z";
    deposit.type = cas::TransactionType::Deposit;
    deposit.toAccountId = "ACC-000001";
    deposit.amountCents = 10000;

    cas::Transaction transfer{};
    transfer.transactionId = "TXN-000002";
    transfer.occurredAt = "2026-11-02T09:00:00Z";
    transfer.type = cas::TransactionType::Transfer;
    transfer.fromAccountId = "ACC-000001";
    transfer.toAccountId = "ACC-000002";
    transfer.amountCents = 2500;

    cas::Transaction withdrawal{};
    withdrawal.transactionId = "TXN-000003";
    withdrawal.occurredAt = "2026-11-03T09:00:00Z";
    withdrawal.type = cas::TransactionType::Withdrawal;
    withdrawal.fromAccountId = "ACC-000001";
    withdrawal.amountCents = 5000;

    // Deliberately store records out of chronological order.
    data.transactions = {withdrawal, transfer, deposit};

    int failures = 0;

    const auto check = [&failures](bool condition,
                                  const std::string& description) {
        std::cout << (condition ? "PASS: " : "FAIL: ")
                  << description << '\n';

        if (!condition) {
            ++failures;
        }
    };

    cas::TransactionFilter filter{};
    filter.accountId = "ACC-000001";
    filter.startDate = "2026-11-01";
    filter.endDate = "2026-11-02";
    filter.minAmountCents = 2500;
    filter.maxAmountCents = 10000;

    const auto results = cas::filterTransactions(data, filter);

    std::cout << "Date/amount query IDs:";
    for (const auto& transaction : results) {
        std::cout << ' ' << transaction.transactionId;
    }
    std::cout << '\n';

    check(results.size() == 2 &&
              results[0].transactionId == "TXN-000001" &&
              results[1].transactionId == "TXN-000002",
          "Inclusive boundaries and chronological order");

    filter.type = cas::TransactionType::Transfer;
    const auto transfers = cas::filterTransactions(data, filter);

    check(transfers.size() == 1 &&
              transfers[0].transactionId == "TXN-000002",
          "Type filter returns the transfer once");

    cas::TransactionFilter recipient{};
    recipient.accountId = "ACC-000002";
    const auto received = cas::filterTransactions(data, recipient);

    check(received.size() == 1 &&
              received[0].transactionId == "TXN-000002",
          "Transfer appears in recipient history");

    check(data.transactions[0].transactionId == "TXN-000003" &&
              data.transactions[1].transactionId == "TXN-000002" &&
              data.transactions[2].transactionId == "TXN-000001",
          "Original ledger order is unchanged");

    filter.startDate = "2026-02-30";
    bool rejectedInvalidDate = false;

    try {
        cas::filterTransactions(data, filter);
    } catch (const std::invalid_argument&) {
        rejectedInvalidDate = true;
    }

    check(rejectedInvalidDate, "Impossible calendar date is rejected");

    std::cout << "\nFailed checks: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}