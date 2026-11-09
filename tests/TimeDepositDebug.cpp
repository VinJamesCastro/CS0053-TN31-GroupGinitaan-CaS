#include "TransactionEngine.h"

#include <iostream>

int main() {
    cas::BankData data;
    const auto sender = cas::createAccount(
        data, "Sender", cas::AccountType::Checking, "test-digest", 100000);
    const auto receiver = cas::createAccount(
        data, "Time deposit", cas::AccountType::TimeDeposit, "test-digest", 100000);
    if (!sender.ok || !receiver.ok) {
        std::cerr << "Could not create the debug accounts.\n";
        return 2;
    }

    std::cout << "Before: sender=" << data.accounts[0].balanceCents
              << ", receiver=" << data.accounts[1].balanceCents
              << ", ledger=" << data.transactions.size() << '\n';

    // The current shared contract says time deposits must not support transfers
    // until a maturity rule is defined. Exercise the receiver side of that rule.
    const auto result = cas::transfer(
        data, sender.referenceId, receiver.referenceId, 10000);

    std::cout << "Expected: ok=0, sender=100000, receiver=100000, ledger=2\n"
              << "Actual: ok=" << result.ok
              << ", sender=" << data.accounts[0].balanceCents
              << ", receiver=" << data.accounts[1].balanceCents
              << ", ledger=" << data.transactions.size() << '\n'
              << "Message: " << result.message << '\n';

    return !result.ok && data.accounts[0].balanceCents == 100000 &&
                   data.accounts[1].balanceCents == 100000 &&
                   data.transactions.size() == 2
               ? 0
               : 1;
}