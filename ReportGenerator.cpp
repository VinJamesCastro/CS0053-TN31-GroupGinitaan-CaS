#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace cas
{
    using Money = std::int64_t;
    enum class Role
    {
        Client,
        Admin
    };
    enum class AccountType
    {
        Checking,
        Savings,
        TimeDeposit
    };
    enum class AccountStatus
    {
        Active,
        Frozen,
        Closed
    };
    enum class TransactionType
    {
        OpeningDeposit,
        Deposit,
        Withdrawal,
        Transfer
    };
    enum class Currency
    {
        PHP,
        USD,
        EUR,
        JPY
    };

    struct Account
    {
        std::string accountId;
        std::string holderName;
        std::string pinDigest;
        Role role{};
        AccountType type{};
        AccountStatus status{};
        Money balanceCents{};
        Money minimumBalanceCents{};
        int annualInterestBps{};
    };
    struct Transaction
    {
        std::string transactionId;
        std::string occurredAt;
        TransactionType type{};
        std::string fromAccountId;
        std::string toAccountId;
        Money amountCents{};
        Money fromBalanceAfterCents{};
        Money toBalanceAfterCents{};
    };
    struct BankData
    {
        std::vector<Account> accounts;
        std::vector<Transaction> transactions;
    };
    struct ReportResult
    {
        bool ok{};
        std::string message;
    };

    namespace
    {
        constexpr std::size_t chartWidth = 40;

        const Account *findAccount(const BankData &data, const std::string &id)
        {
            const auto it = std::find_if(data.accounts.begin(), data.accounts.end(),
                                         [&](const Account &account)
                                         { return account.accountId == id; });
            return it == data.accounts.end() ? nullptr : &*it;
        }

        std::vector<const Transaction *> accountHistory(const BankData &data, const std::string &id)
        {
            std::vector<const Transaction *> rows;
            for (const auto &transaction : data.transactions)
            {
                if (transaction.fromAccountId == id || transaction.toAccountId == id)
                    rows.push_back(&transaction);
            }
            std::stable_sort(rows.begin(), rows.end(), [](const Transaction *a, const Transaction *b)
                             { return a->occurredAt < b->occurredAt; });
            return rows;
        }

        Money balanceAfter(const Transaction &transaction, const std::string &id)
        {
            return transaction.fromAccountId == id ? transaction.fromBalanceAfterCents
                                                   : transaction.toBalanceAfterCents;
        }

        std::string moneyText(Money cents)
        {
            const bool negative = cents < 0;
            const std::uint64_t magnitude = negative ? std::uint64_t(-(cents + 1)) + 1
                                                     : std::uint64_t(cents);
            std::ostringstream out;
            if (negative)
                out << '-';
            out << "PHP " << magnitude / 100 << '.' << std::setw(2) << std::setfill('0') << magnitude % 100;
            return out.str();
        }

        const char *typeText(AccountType type)
        {
            switch (type)
            {
            case AccountType::Checking:
                return "Checking";
            case AccountType::Savings:
                return "Savings";
            case AccountType::TimeDeposit:
                return "Time Deposit";
            }
            return "Unknown";
        }

        const char *typeText(TransactionType type)
        {
            switch (type)
            {
            case TransactionType::OpeningDeposit:
                return "Opening deposit";
            case TransactionType::Deposit:
                return "Deposit";
            case TransactionType::Withdrawal:
                return "Withdrawal";
            case TransactionType::Transfer:
                return "Transfer";
            }
            return "Unknown";
        }

        ReportResult writeFile(const std::filesystem::path &path, const std::string &content)
        {
            if (path.empty() || path.filename().empty())
                return {false, "Invalid report output path"};
            std::error_code error;
            if (!path.parent_path().empty())
                std::filesystem::create_directories(path.parent_path(), error);
            if (error)
                return {false, "Cannot create report directory: " + error.message()};
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
                return {false, "Cannot open report output: " + path.string()};
            output << content;
            output.close();
            if (!output)
                return {false, "Cannot write report output: " + path.string()};
            return {true, "Report written: " + path.string()};
        }
    } // namespace

    ReportResult renderAsciiBalanceChart(const BankData &data, const std::string &accountId,
                                         std::ostream &output)
    {
        const Account *account = findAccount(data, accountId);
        if (!account)
            return {false, "Account not found: " + accountId};
        const auto rows = accountHistory(data, accountId);
        std::ostringstream chart;
        chart << "Balance chart: " << account->accountId << " (" << account->holderName << ")\n";
        if (rows.empty())
        {
            chart << "No transactions. Current balance: " << moneyText(account->balanceCents) << '\n';
        }
        else
        {
            Money maximum = 0;
            for (const auto *row : rows)
                maximum = std::max(maximum, balanceAfter(*row, accountId));
            for (const auto *row : rows)
            {
                const Money balance = balanceAfter(*row, accountId);
                const std::size_t width = maximum <= 0 || balance <= 0 ? 0 : static_cast<std::size_t>((static_cast<long double>(balance) / maximum) * chartWidth);
                chart << row->occurredAt.substr(0, 10) << " | "
                      << std::string(std::min(width, chartWidth), '#') << " "
                      << moneyText(balance) << " | " << row->transactionId << '\n';
            }
        }
        output << chart.str();
        if (!output)
            return {false, "Cannot write balance chart"};
        return {true, "Balance chart rendered"};
    }

    ReportResult exportStatement(const BankData &data, const std::string &accountId,
                                 const std::filesystem::path &outputPath)
    {
        const Account *account = findAccount(data, accountId);
        if (!account)
            return {false, "Account not found: " + accountId};
        const auto rows = accountHistory(data, accountId);
        std::ostringstream report;
        report << "Cents and Sensibility - Account Statement\n"
               << "Account: " << account->accountId << '\n'
               << "Holder: " << account->holderName << '\n'
               << "Type: " << typeText(account->type) << '\n';
        if (rows.empty())
        {
            report << "Period: No transactions\n"
                   << "Opening balance: " << moneyText(account->balanceCents) << '\n'
                   << "Closing balance: " << moneyText(account->balanceCents) << '\n'
                   << "No transactions.\n";
        }
        else
        {
            const Transaction &first = *rows.front();
            const Money after = balanceAfter(first, accountId);
            const bool incoming = first.toAccountId == accountId;
            if (first.amountCents < 0 ||
                (!incoming && after > std::numeric_limits<Money>::max() - first.amountCents))
                return {false, "Invalid transaction amount or balance"};
            const Money opening = incoming ? after - first.amountCents : after + first.amountCents;
            Money credits = 0;
            Money debits = 0;
            report << "Period: " << first.occurredAt.substr(0, 10) << " to "
                   << rows.back()->occurredAt.substr(0, 10) << '\n'
                   << "Opening balance: " << moneyText(opening) << '\n'
                   << "Closing balance: " << moneyText(account->balanceCents) << "\n\n"
                   << "Transactions:\n";
            for (const auto *row : rows)
            {
                const bool credit = row->toAccountId == accountId;
                if (row->amountCents < 0 || row->amountCents >
                                                std::numeric_limits<Money>::max() - (credit ? credits : debits))
                    return {false, "Transaction totals exceed the money range"};
                (credit ? credits : debits) += row->amountCents;
                report << row->occurredAt << " | " << row->transactionId << " | "
                       << typeText(row->type) << " | " << (credit ? '+' : '-')
                       << moneyText(row->amountCents) << " | Balance "
                       << moneyText(balanceAfter(*row, accountId)) << '\n';
            }
            report << "\nTotal credits: " << moneyText(credits) << '\n'
                   << "Total debits: " << moneyText(debits) << '\n';
        }
        return writeFile(outputPath, report.str());
    }

    ReportResult generateReceipt(const BankData &data, const std::string &transactionId,
                                 const std::filesystem::path &outputPath)
    {
        const auto it = std::find_if(data.transactions.begin(), data.transactions.end(),
                                     [&](const Transaction &transaction)
                                     {
                                         return transaction.transactionId == transactionId;
                                     });
        if (it == data.transactions.end())
            return {false, "Transaction not found: " + transactionId};
        std::ostringstream report;
        report << "Cents and Sensibility - Transaction Receipt\n"
               << "Reference: " << it->transactionId << '\n'
               << "Date (UTC): " << it->occurredAt << '\n'
               << "Type: " << typeText(it->type) << '\n'
               << "Amount: " << moneyText(it->amountCents) << '\n';
        if (!it->fromAccountId.empty())
        {
            report << "From: " << it->fromAccountId << '\n'
                   << "From balance after: " << moneyText(it->fromBalanceAfterCents) << '\n';
        }
        if (!it->toAccountId.empty())
        {
            report << "To: " << it->toAccountId << '\n'
                   << "To balance after: " << moneyText(it->toBalanceAfterCents) << '\n';
        }
        return writeFile(outputPath, report.str());
    }
} 
