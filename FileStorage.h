#ifndef CAS_FILE_STORAGE_H
#define CAS_FILE_STORAGE_H

#include "BankModel.h"

#include <filesystem>
#include <vector>

namespace cas {

LoadResult loadSnapshot(const std::filesystem::path& path);

SaveResult saveSnapshot(const std::filesystem::path& path,
                        const BankData& data);

std::vector<Transaction> filterTransactions(
    const BankData& data,
    const TransactionFilter& filter);

SaveResult appendAuditLog(const std::filesystem::path& path,
                          const AuditEntry& entry);

std::vector<AuditEntry> readAuditLogs(
    const std::filesystem::path& path);

}

#endif
