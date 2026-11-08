#include "FileStorage.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <iomanip>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cas {
namespace {

std::string encodeText(const std::string& value) {
    if (value.empty()) {
        return "-";
    }

    constexpr char digits[] = "0123456789ABCDEF";
    std::string encoded;

    for (unsigned char character : value) {
        encoded.push_back(digits[character >> 4]);
        encoded.push_back(digits[character & 0x0F]);
    }

    return encoded;
}

struct PathCleanup {
    std::filesystem::path path;

    explicit PathCleanup(std::filesystem::path ownedPath)
        : path(std::move(ownedPath)) {}

    PathCleanup(const PathCleanup&) = delete;
    PathCleanup& operator=(const PathCleanup&) = delete;

    ~PathCleanup() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

bool writeSnapshot(std::ostream& output, const BankData& data) {
    output << "CAS_SNAPSHOT 1\n";

    for (const Account& account : data.accounts) {
        output << "ACCOUNT "
               << encodeText(account.accountId) << ' '
               << encodeText(account.holderName) << ' '
               << encodeText(account.pinDigest) << ' '
               << static_cast<int>(account.role) << ' '
               << static_cast<int>(account.type) << ' '
               << static_cast<int>(account.status) << ' '
               << account.balanceCents << ' '
               << account.minimumBalanceCents << ' '
               << account.annualInterestBps << '\n';
    }

    for (const Transaction& transaction : data.transactions) {
        output << "TRANSACTION "
               << encodeText(transaction.transactionId) << ' '
               << encodeText(transaction.occurredAt) << ' '
               << static_cast<int>(transaction.type) << ' '
               << encodeText(transaction.fromAccountId) << ' '
               << encodeText(transaction.toAccountId) << ' '
               << transaction.amountCents << ' '
               << transaction.fromBalanceAfterCents << ' '
               << transaction.toBalanceAfterCents << '\n';
    }

    output << "END " << data.accounts.size() << ' '
           << data.transactions.size() << '\n';

    return static_cast<bool>(output);
}

bool replaceSnapshot(const std::filesystem::path& temporaryPath,
                     const std::filesystem::path& destinationPath,
                     std::string& message) {
#ifdef _WIN32
    if (!MoveFileExW(temporaryPath.c_str(),
                     destinationPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING |
                         MOVEFILE_WRITE_THROUGH)) {
        const auto code = static_cast<int>(GetLastError());
        message = std::error_code(code, std::system_category()).message();
        return false;
    }
#else
    std::error_code error;
    std::filesystem::rename(temporaryPath, destinationPath, error);

    if (error) {
        message = error.message();
        return false;
    }
#endif

    return true;
}

bool readText(std::istream& input, std::string& value) {
    std::string encoded;
    if (!(input >> encoded)) {
        return false;
    }

    if (encoded == "-") {
        value.clear();
        return true;
    }

    if (encoded.size() % 2 != 0) {
        return false;
    }

    const auto hexDigit = [](char character) -> int {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        return -1;
    };

    std::string decoded;
    decoded.reserve(encoded.size() / 2);

    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const int high = hexDigit(encoded[index]);
        const int low = hexDigit(encoded[index + 1]);

        if (high < 0 || low < 0) {
            return false;
        }

        decoded.push_back(static_cast<char>(high * 16 + low));
    }

    value = std::move(decoded);
    return true;
}

template <typename Integer>
bool readInteger(std::istream& input, Integer& value) {
    std::string token;
    if (!(input >> token)) {
        return false;
    }

    Integer parsed{};
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto result = std::from_chars(begin, end, parsed);

    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }

    value = parsed;
    return true;
}

template <typename Enum>
bool readEnum(std::istream& input, int maximum, Enum& value) {
    int number = 0;

    if (!readInteger(input, number) || number < 0 || number > maximum) {
        return false;
    }

    value = static_cast<Enum>(number);
    return true;
}

bool hasNoExtraFields(std::istream& input) {
    input >> std::ws;
    return input.eof();
}

bool parseAccount(std::istream& input, Account& account) {
    return readText(input, account.accountId) &&
           readText(input, account.holderName) &&
           readText(input, account.pinDigest) &&
           readEnum(input, 1, account.role) &&
           readEnum(input, 2, account.type) &&
           readEnum(input, 2, account.status) &&
           readInteger(input, account.balanceCents) &&
           readInteger(input, account.minimumBalanceCents) &&
           readInteger(input, account.annualInterestBps) &&
           hasNoExtraFields(input);
}

bool parseTransaction(std::istream& input, Transaction& transaction) {
    return readText(input, transaction.transactionId) &&
           readText(input, transaction.occurredAt) &&
           readEnum(input, 3, transaction.type) &&
           readText(input, transaction.fromAccountId) &&
           readText(input, transaction.toAccountId) &&
           readInteger(input, transaction.amountCents) &&
           readInteger(input, transaction.fromBalanceAfterCents) &&
           readInteger(input, transaction.toBalanceAfterCents) &&
           hasNoExtraFields(input);
}

LoadResult loadError(std::size_t lineNumber, const std::string& reason) {
    return {false, {}, "Snapshot line " + std::to_string(lineNumber) +
                           ": " + reason};
}

}

LoadResult loadSnapshot(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);

    if (error) {
        return {false, {}, "Cannot inspect snapshot: " + error.message()};
    }

    if (!exists) {
        return {true, {}, "No snapshot found; starting a new installation."};
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {false, {}, "Cannot open the existing snapshot."};
    }

    std::string line;
    if (!std::getline(input, line)) {
        return {false, {}, "Existing snapshot is empty or unreadable."};
    }

    {
        std::istringstream header(line);
        std::string marker;
        int version = 0;

        if (!(header >> marker) || marker != "CAS_SNAPSHOT" ||
            !readInteger(header, version) || version != 1 ||
            !hasNoExtraFields(header)) {
            return loadError(1, "Invalid header or unsupported version.");
        }
    }

    BankData candidate;
    std::unordered_set<std::string> accountIds;
    std::unordered_set<std::string> transactionIds;
    std::size_t lineNumber = 1;
    bool foundEnd = false;

    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream record(line);
        std::string recordType;

        if (!(record >> recordType)) {
            return loadError(lineNumber, "Empty record.");
        }

        if (foundEnd) {
            return loadError(lineNumber, "Unexpected content after END.");
        }

        if (recordType == "ACCOUNT") {
            Account account{};

            if (!parseAccount(record, account)) {
                return loadError(lineNumber, "Invalid ACCOUNT record.");
            }

            if (account.accountId.empty() ||
                !accountIds.insert(account.accountId).second) {
                return loadError(lineNumber, "Empty or duplicate account ID.");
            }

            candidate.accounts.push_back(std::move(account));
        } else if (recordType == "TRANSACTION") {
            Transaction transaction{};

            if (!parseTransaction(record, transaction)) {
                return loadError(lineNumber, "Invalid TRANSACTION record.");
            }

            if (transaction.transactionId.empty() ||
                !transactionIds.insert(transaction.transactionId).second) {
                return loadError(lineNumber,
                                 "Empty or duplicate transaction ID.");
            }

            candidate.transactions.push_back(std::move(transaction));
        } else if (recordType == "END") {
            std::size_t accountCount = 0;
            std::size_t transactionCount = 0;

            if (!readInteger(record, accountCount) ||
                !readInteger(record, transactionCount) ||
                !hasNoExtraFields(record) ||
                accountCount != candidate.accounts.size() ||
                transactionCount != candidate.transactions.size()) {
                return loadError(lineNumber, "Invalid END record or counts.");
            }

            foundEnd = true;
        } else {
            return loadError(lineNumber, "Unknown record type.");
        }
    }

    if (input.bad() || (!input.eof() && input.fail())) {
        return {false, {}, "An error occurred while reading the snapshot."};
    }

    if (!foundEnd) {
        return {false, {}, "Snapshot is incomplete: missing END record."};
    }

    return {true, std::move(candidate), "Snapshot loaded successfully."};
}

SaveResult saveSnapshot(const std::filesystem::path& path,
                        const BankData& data) {
    if (path.empty() || path.filename().empty()) {
        return {false, "A snapshot filename is required."};
    }

    const std::filesystem::path parent = path.parent_path();
    std::error_code error;

    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);

        if (error) {
            return {false,
                    "Cannot create snapshot directory: " + error.message()};
        }
    }

    std::filesystem::path lockPath = path;
    lockPath += ".save-lock";

    const bool acquiredLock =
        std::filesystem::create_directory(lockPath, error);

    if (error) {
        return {false, "Cannot acquire save lock: " + error.message()};
    }

    if (!acquiredLock) {
        return {false,
                "Snapshot save is locked. Another save may be running."};
    }

    const PathCleanup lockCleanup(lockPath);

    const LoadResult existing = loadSnapshot(path);

    if (!existing.ok) {
        return {false,
                "Existing snapshot was not replaced: " + existing.message};
    }

    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";

    const bool temporaryExists =
        std::filesystem::exists(temporaryPath, error);

    if (error) {
        return {false,
                "Cannot inspect temporary file: " + error.message()};
    }

    if (temporaryExists) {
        return {false,
                "Temporary snapshot already exists; inspect it before retrying."};
    }

    const PathCleanup temporaryCleanup(temporaryPath);

    {
        std::ofstream output(
            temporaryPath,
            std::ios::binary | std::ios::trunc);

        if (!output) {
            return {false, "Cannot open temporary snapshot for writing."};
        }

        if (!writeSnapshot(output, data)) {
            return {false, "Writing the temporary snapshot failed."};
        }

        output.flush();

        if (!output) {
            return {false, "Flushing the temporary snapshot failed."};
        }

        output.close();

        if (output.fail()) {
            return {false, "Closing the temporary snapshot failed."};
        }
    }

    const LoadResult verification = loadSnapshot(temporaryPath);

    if (!verification.ok) {
        return {false,
                "Temporary snapshot failed validation: " +
                    verification.message};
    }

    std::string replacementError;

    if (!replaceSnapshot(temporaryPath, path, replacementError)) {
        return {false,
                "Cannot replace snapshot: " + replacementError};
    }

    return {true, "Snapshot saved successfully."};
}

}