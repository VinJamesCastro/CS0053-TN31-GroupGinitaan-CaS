#ifndef CAS_TRANSACTION_ENGINE_H
#define CAS_TRANSACTION_ENGINE_H

#include "BankModel.h"

namespace cas {

OperationResult createAccount(
    BankData& data,
    const std::string& holderName,
    AccountType type,
    const std::string& pinDigest,
    Money initialDepositCents);

AccountSummary getAccountSummary(
    const BankData& data,
    const std::string& accountId);

OperationResult deposit(
    BankData& data,
    const std::string& accountId,
    Money amountCents);

OperationResult withdraw(
    BankData& data,
    const std::string& accountId,
    Money amountCents);

OperationResult transfer(
    BankData& data,
    const std::string& senderId,
    const std::string& receiverId,
    Money amountCents);

OperationResult updateAccountStatus(
    BankData& data,
    const std::string& accountId,
    AccountStatus nextStatus);

FxQuote convertCurrency(
    Money inputMinorUnits,
    Currency from,
    Currency to);

} // namespace cas

#endif // CAS_TRANSACTION_ENGINE_H