#include "../ReportGenerator.cpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>

int main() {
    using namespace cas;
    BankData data;
    data.accounts.push_back({"ACC-000001", "Maya", "", Role::Client,
                             AccountType::Checking, AccountStatus::Active, 12500, 0, 0});
    data.accounts.push_back({"ACC-000002", "Noel", "", Role::Client,
                             AccountType::Savings, AccountStatus::Active, 7500, 0, 0});
    data.accounts.push_back({"ACC-000003", "Empty", "", Role::Client,
                             AccountType::Checking, AccountStatus::Active, 0, 0, 0});
    data.transactions.push_back({"TXN-000002", "2026-10-02T10:00:00Z", TransactionType::Transfer,
                                 "ACC-000001", "ACC-000002", 2500, 12500, 7500});
    data.transactions.push_back({"TXN-000001", "2026-10-01T10:00:00Z", TransactionType::OpeningDeposit,
                                 "", "ACC-000001", 15000, 0, 15000});

    std::ostringstream chart;
    assert(renderAsciiBalanceChart(data, "ACC-000001", chart).ok);
    const auto chartText = chart.str();
    assert(chartText.find("2026-10-01") < chartText.find("2026-10-02"));
    assert(chartText.find("PHP 125.00") != std::string::npos);
    std::istringstream lines(chartText);
    std::string line;
    while (std::getline(lines, line)) {
        const auto bar = line.find(" | ");
        if (bar != std::string::npos) {
            const auto end = line.find(' ', bar + 3);
            assert(end != std::string::npos && end - (bar + 3) <= 40);
        }
    }
    std::ostringstream emptyChart;
    assert(renderAsciiBalanceChart(data, "ACC-000003", emptyChart).ok);
    assert(emptyChart.str().find("No transactions") != std::string::npos);
    assert(!renderAsciiBalanceChart(data, "MISSING", chart).ok);

    const auto directory = std::filesystem::temp_directory_path() / "cas_member4_report_test";
    std::filesystem::create_directories(directory);
    const auto statementPath = directory / "Statement_ACC-000001.txt";
    const auto receiptPath = directory / "Receipt_TXN-000002.txt";
    assert(exportStatement(data, "ACC-000001", statementPath).ok);
    assert(generateReceipt(data, "TXN-000002", receiptPath).ok);
    std::ifstream statement(statementPath);
    std::stringstream statementText;
    statementText << statement.rdbuf();
    assert(statementText.str().find("Opening balance: PHP 0.00") != std::string::npos);
    assert(statementText.str().find("Closing balance: PHP 125.00") != std::string::npos);
    assert(statementText.str().find("-PHP 25.00") != std::string::npos);
    assert(statementText.str().find("Total credits: PHP 150.00") != std::string::npos);
    assert(statementText.str().find("Total debits: PHP 25.00") != std::string::npos);
    std::ifstream receipt(receiptPath);
    std::stringstream receiptText;
    receiptText << receipt.rdbuf();
    assert(receiptText.str().find("From: ACC-000001") != std::string::npos);
    assert(receiptText.str().find("To: ACC-000002") != std::string::npos);
    assert(receiptText.str().find("PHP 25.00") != std::string::npos);
    assert(!exportStatement(data, "MISSING", directory / "missing.txt").ok);
    assert(!generateReceipt(data, "MISSING", directory / "missing.txt").ok);
    assert(!exportStatement(data, "ACC-000001", directory).ok);
    const auto emptyStatementPath = directory / "empty.txt";
    assert(exportStatement(data, "ACC-000003", emptyStatementPath).ok);
    std::ifstream emptyStatement(emptyStatementPath);
    std::stringstream emptyStatementText;
    emptyStatementText << emptyStatement.rdbuf();
    assert(emptyStatementText.str().find("No transactions") != std::string::npos);
    emptyStatement.close();
    data.accounts[0].balanceCents = std::numeric_limits<Money>::max();
    data.transactions[1].toBalanceAfterCents = std::numeric_limits<Money>::max();
    std::ostringstream largeChart;
    assert(renderAsciiBalanceChart(data, "ACC-000001", largeChart).ok);
    assert(largeChart.str().find(std::string(41, '#')) == std::string::npos);
    assert(data.transactions.size() == 2);
    assert(data.accounts[0].balanceCents == std::numeric_limits<Money>::max());
    statement.close();
    receipt.close();
    std::filesystem::remove_all(directory);
}
