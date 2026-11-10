#include "TransactionEngine.h"

#include <array>
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
        original, "Charles", AccountType::Checking, "test-digest", 100000);
    const auto second = createAccount(
        original, "Recipient", AccountType::Savings, "other-digest", 100000);
    if (!first.ok || !second.ok || original.accounts.size() != 2) {
        std::cerr << "Account fixture creation failed.\n";
        return 1;
    }
    const std::string id = first.referenceId;
    const std::array<AccountStatus, 3> statuses{
        AccountStatus::Active, AccountStatus::Frozen, AccountStatus::Closed};
    const std::array<std::string, 3> statusNames{"Active", "Frozen", "Closed"};
    for (std::size_t from = 0; from < statuses.size(); ++from) {
        for (std::size_t to = 0; to < statuses.size(); ++to) {
            BankData data = original;
            data.accounts[0].status = statuses[from];
            BankData expected = data;
            const bool allowed = from != 2 && from != to;
            if (allowed) {
                expected.accounts[0].status = statuses[to];
            }
            const auto result = updateAccountStatus(data, id, statuses[to]);
            check(result.ok == allowed && !result.message.empty() &&
                      result.referenceId.empty() && snapshot(data) == snapshot(expected),
                  statusNames[from] + " -> " + statusNames[to] +
                      (allowed ? ": status only changes; 2 ledger rows retained"
                               : ": rejected; complete snapshot unchanged"));
        }
    }

    const auto rejectStatus = [&](BankData data, const std::string& target,
                                  AccountStatus next, const std::string& label) {
        const std::string before = snapshot(data);
        const auto result = updateAccountStatus(data, target, next);
        check(!result.ok && result.referenceId.empty() && !result.message.empty() &&
                  snapshot(data) == before,
              label + "; complete snapshot unchanged");
    };
    rejectStatus(original, "", AccountStatus::Frozen, "Empty status target");
    rejectStatus(original, "ACC-missing", AccountStatus::Frozen, "Missing status target");
    rejectStatus(original, id, static_cast<AccountStatus>(99), "Invalid requested status 99");
    rejectStatus(original, id, static_cast<AccountStatus>(-1), "Invalid requested status -1");
    BankData invalid = original;
    invalid.accounts[0].status = static_cast<AccountStatus>(99);
    rejectStatus(invalid, id, AccountStatus::Active, "Invalid stored status");
    invalid = original;
    invalid.accounts[0].role = Role::Admin;
    rejectStatus(invalid, id, AccountStatus::Frozen, "Admin account target");
    invalid = original;
    invalid.accounts.push_back(invalid.accounts[0]);
    rejectStatus(invalid, id, AccountStatus::Frozen, "Duplicate status target ID");

    BankData flow = original;
    auto statusResult = updateAccountStatus(flow, id, AccountStatus::Frozen);
    const std::string frozen = snapshot(flow);
    check(statusResult.ok && !deposit(flow, id, 1).ok && snapshot(flow) == frozen,
          "Frozen account rejects deposit without mutation");
    check(!withdraw(flow, id, 1).ok && snapshot(flow) == frozen,
          "Frozen account rejects withdrawal without mutation");
    check(!transfer(flow, id, second.referenceId, 1).ok && snapshot(flow) == frozen,
          "Frozen sender rejects transfer without mutation");
    check(!transfer(flow, second.referenceId, id, 1).ok && snapshot(flow) == frozen,
          "Frozen receiver rejects transfer without mutation");
    statusResult = updateAccountStatus(flow, id, AccountStatus::Active);
    check(statusResult.ok && deposit(flow, id, 1).ok &&
              flow.accounts[0].balanceCents == 100001 && flow.transactions.size() == 3,
          "Unfreezing restores banking: deposit yields 100001 centavos and 3 ledger rows");
    statusResult = updateAccountStatus(flow, id, AccountStatus::Closed);
    const std::string closed = snapshot(flow);
    check(statusResult.ok && !deposit(flow, id, 1).ok &&
              !withdraw(flow, id, 1).ok && !transfer(flow, id, second.referenceId, 1).ok &&
              !transfer(flow, second.referenceId, id, 1).ok && snapshot(flow) == closed,
          "Closed account rejects every financial operation without mutation");

    const std::array<Currency, 4> currencies{
        Currency::PHP, Currency::USD, Currency::EUR, Currency::JPY};
    const std::array<std::string, 4> codes{"PHP", "USD", "EUR", "JPY"};
    const auto expectQuote = [&](Money amount, Currency from, Currency to,
                                 Money expected, const std::string& label) {
        const auto quote = convertCurrency(amount, from, to);
        check(quote.ok && quote.inputMinorUnits == amount &&
                  quote.from == from && quote.to == to && quote.outputMinorUnits == expected &&
                  quote.rateDescription.find("demo") != std::string::npos &&
                  quote.rateDescription.find("halves round up") != std::string::npos,
              label + ": input=" + std::to_string(amount) +
                  ", expected=" + std::to_string(expected) +
                  ", actual=" + std::to_string(quote.outputMinorUnits));
    };
    const auto rejectQuote = [&](Money amount, Currency from, Currency to,
                                 const std::string& label) {
        const auto quote = convertCurrency(amount, from, to);
        check(!quote.ok && quote.inputMinorUnits == amount && quote.from == from &&
                  quote.to == to && quote.outputMinorUnits == 0 && !quote.rateDescription.empty(),
              label + ": ok=false, output=0, explanatory description");
    };

    // Independent expected values for 10000 source minor units, rows/columns
    // in PHP, USD, EUR, JPY order. Every pair is checked, not just a PHP pivot.
    const std::array<std::array<Money, 4>, 4> expected{{
        {{10000, 179, 167, 250}},
        {{560000, 10000, 9333, 14000}},
        {{600000, 10714, 10000, 15000}},
        {{400000, 7143, 6667, 10000}}
    }};
    for (std::size_t from = 0; from < currencies.size(); ++from) {
        for (std::size_t to = 0; to < currencies.size(); ++to) {
            expectQuote(10000, currencies[from], currencies[to], expected[from][to],
                        codes[from] + " -> " + codes[to]);
            expectQuote(0, currencies[from], currencies[to], 0,
                        "Zero " + codes[from] + " -> " + codes[to]);
        }
    }

    expectQuote(100, Currency::USD, Currency::PHP, 5600, "USD 1.00 -> PHP 56.00");
    expectQuote(5600, Currency::PHP, Currency::USD, 100, "PHP 56.00 -> USD 1.00");
    expectQuote(1, Currency::JPY, Currency::PHP, 40, "JPY uses zero decimal places");
    expectQuote(1, Currency::PHP, Currency::USD, 0, "Tiny positive quote rounds to zero");
    expectQuote(19, Currency::PHP, Currency::JPY, 0, "JPY below half");
    expectQuote(20, Currency::PHP, Currency::JPY, 1, "JPY exact half rounds up");
    expectQuote(21, Currency::PHP, Currency::JPY, 1, "JPY above half");
    expectQuote(60, Currency::PHP, Currency::JPY, 2, "JPY 1.5 rounds to 2");
    expectQuote(100, Currency::PHP, Currency::JPY, 3, "PHP 1.00 -> JPY 3");
    expectQuote(27, Currency::PHP, Currency::USD, 0, "USD below half-cent");
    expectQuote(28, Currency::PHP, Currency::USD, 1, "USD exact half-cent");
    expectQuote(29, Currency::PHP, Currency::USD, 1, "USD above half-cent");
    expectQuote(7, Currency::EUR, Currency::USD, 8, "EUR to USD rounds only once");

    rejectQuote(-1, Currency::PHP, Currency::USD, "Negative FX input");
    rejectQuote(std::numeric_limits<Money>::min(), Currency::USD, Currency::JPY,
                "Minimum signed Money input");
    for (const auto bad : {static_cast<Currency>(-1), static_cast<Currency>(99)}) {
        rejectQuote(100, bad, Currency::PHP, "Invalid source currency");
        rejectQuote(100, Currency::PHP, bad, "Invalid destination currency");
        rejectQuote(0, bad, bad, "Invalid equal currencies with zero input");
    }

    // These large expectations were computed separately using exact rational
    // arithmetic. -1 denotes overflow, not a supported negative output.
    constexpr Money maximum = std::numeric_limits<Money>::max();
    const std::array<std::array<Money, 4>, 4> maximumExpected{{
        {{maximum, 164703072086692425, 153722867280912930, 230584300921369395}},
        {{-1, maximum, 8608480567731124087, -1}},
        {{-1, -1, maximum, -1}},
        {{-1, 6588122883467697005, 6148914691236517205, maximum}}
    }};
    for (std::size_t from = 0; from < currencies.size(); ++from) {
        for (std::size_t to = 0; to < currencies.size(); ++to) {
            const std::string label = "Maximum input " + codes[from] + " -> " + codes[to];
            if (maximumExpected[from][to] < 0) {
                rejectQuote(maximum, currencies[from], currencies[to], label);
            } else {
                expectQuote(maximum, currencies[from], currencies[to],
                            maximumExpected[from][to], label);
            }
        }
    }
    constexpr Money largestUsdToPhp = maximum / 56;
    expectQuote(largestUsdToPhp, Currency::USD, Currency::PHP, largestUsdToPhp * 56,
                "Largest fitting USD -> PHP input");
    rejectQuote(largestUsdToPhp + 1, Currency::USD, Currency::PHP,
                "One minor unit beyond USD -> PHP limit");

    std::cout << "\n" << checks << " checks; " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}