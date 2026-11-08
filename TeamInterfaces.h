#pragma once

#include "BankModel.h"

#include <filesystem>
#include <ostream>

namespace cas {

// Declarations from the shared development contract. Teammates own the definitions.
OperationResult createAccount(BankData&, const std::string&, AccountType,
                              const std::string&, Money);
AccountSummary getAccountSummary(const BankData&, const std::string&);
OperationResult deposit(BankData&, const std::string&, Money);
OperationResult withdraw(BankData&, const std::string&, Money);
OperationResult transfer(BankData&, const std::string&, const std::string&, Money);
OperationResult updateAccountStatus(BankData&, const std::string&, AccountStatus);
FxQuote convertCurrency(Money, Currency, Currency);

LoadResult loadSnapshot(const std::filesystem::path&);
SaveResult saveSnapshot(const std::filesystem::path&, const BankData&);
std::vector<Transaction> filterTransactions(const BankData&, const TransactionFilter&);
SaveResult appendAuditLog(const std::filesystem::path&, const AuditEntry&);
std::vector<AuditEntry> readAuditLogs(const std::filesystem::path&);
int getValidInt(const std::string&, int, int);
Money getValidMoney(const std::string&, Money);
std::string getValidPin(const std::string&);

ReportResult renderAsciiBalanceChart(const BankData&, const std::string&, std::ostream&);
ReportResult exportStatement(const BankData&, const std::string&,
                             const std::filesystem::path&);
ReportResult generateReceipt(const BankData&, const std::string&,
                             const std::filesystem::path&);

} // namespace cas
