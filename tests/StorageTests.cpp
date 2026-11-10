#include "FileStorage.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {

bool sameAccount(const cas::Account& left, const cas::Account& right) {
    return std::tie(left.accountId, left.holderName, left.pinDigest,
                    left.role, left.type, left.status, left.balanceCents,
                    left.minimumBalanceCents, left.annualInterestBps) ==
           std::tie(right.accountId, right.holderName, right.pinDigest,
                    right.role, right.type, right.status, right.balanceCents,
                    right.minimumBalanceCents, right.annualInterestBps);
}

bool sameTransaction(const cas::Transaction& left,
                     const cas::Transaction& right) {
    return std::tie(left.transactionId, left.occurredAt, left.type,
                    left.fromAccountId, left.toAccountId, left.amountCents,
                    left.fromBalanceAfterCents, left.toBalanceAfterCents) ==
           std::tie(right.transactionId, right.occurredAt, right.type,
                    right.fromAccountId, right.toAccountId, right.amountCents,
                    right.fromBalanceAfterCents, right.toBalanceAfterCents);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Cannot read test file.");
    }

    std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    if (input.bad()) {
        throw std::runtime_error("Reading test file failed.");
    }

    return contents;
}

void writeFile(const std::filesystem::path& path,
               const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        throw std::runtime_error("Cannot open test file.");
    }

    output << contents;
    output.close();

    if (output.fail()) {
        throw std::runtime_error("Writing test file failed.");
    }
}

int runTests(const std::filesystem::path& directory) {
    int failures = 0;

    const auto check = [&failures](bool condition,
                                  const std::string& description) {
        std::cout << (condition ? "PASS: " : "FAIL: ")
                  << description << '\n';

        if (!condition) {
            ++failures;
        }
    };

    const auto path = directory / "bank_state.txt";

    const cas::LoadResult missing = cas::loadSnapshot(path);
    check(missing.ok && missing.data.accounts.empty() &&
              missing.data.transactions.empty(),
          "Missing snapshot returns an empty installation");

    cas::BankData original;

    cas::Account account{};
    account.accountId = "ACC-000001";
    account.holderName = "Jillian | Test\nAccount";
    account.pinDigest = "opaque-test-digest";
    account.role = cas::Role::Client;
    account.type = cas::AccountType::Savings;
    account.status = cas::AccountStatus::Active;
    account.balanceCents = 125050;
    account.minimumBalanceCents = 10000;
    account.annualInterestBps = 150;
    original.accounts.push_back(account);

    cas::Transaction transaction{};
    transaction.transactionId = "TXN-000001";
    transaction.occurredAt = "2026-11-08T05:00:00Z";
    transaction.type = cas::TransactionType::OpeningDeposit;
    transaction.fromAccountId = "";
    transaction.toAccountId = account.accountId;
    transaction.amountCents = account.balanceCents;
    transaction.fromBalanceAfterCents = 0;
    transaction.toBalanceAfterCents = account.balanceCents;
    original.transactions.push_back(transaction);

    const cas::SaveResult firstSave = cas::saveSnapshot(path, original);
    check(firstSave.ok, "Initial snapshot saves successfully");

    if (!firstSave.ok) {
        std::cout << "Save error: " << firstSave.message << '\n';
        return failures;
    }

    const cas::LoadResult loaded = cas::loadSnapshot(path);
    const bool exactRoundTrip =
        loaded.ok &&
        loaded.data.accounts.size() == 1 &&
        loaded.data.transactions.size() == 1 &&
        sameAccount(original.accounts[0], loaded.data.accounts[0]) &&
        sameTransaction(original.transactions[0],
                        loaded.data.transactions[0]);

    check(exactRoundTrip,
          "Every account and transaction field survives save/reload");

    // Change a non-financial field to test replacing an existing snapshot.
    cas::BankData updated = original;
    updated.accounts[0].holderName = "Updated Test Account";

    const cas::SaveResult replacement = cas::saveSnapshot(path, updated);
    const cas::LoadResult reloaded = cas::loadSnapshot(path);

    check(replacement.ok && reloaded.ok &&
              reloaded.data.accounts.size() == 1 &&
              reloaded.data.accounts[0].holderName ==
                  "Updated Test Account",
          "An existing valid snapshot can be replaced");

    const std::string validContents = readFile(path);

    cas::BankData duplicate = updated;
    duplicate.accounts.push_back(duplicate.accounts[0]);

    const cas::SaveResult duplicateSave =
        cas::saveSnapshot(path, duplicate);

    check(!duplicateSave.ok && readFile(path) == validContents,
          "Duplicate account ID is rejected; saved file is unchanged");

    cas::BankData invalidEnum = updated;
    invalidEnum.accounts[0].status =
        static_cast<cas::AccountStatus>(99);

    const cas::SaveResult enumSave =
        cas::saveSnapshot(path, invalidEnum);

    check(!enumSave.ok && readFile(path) == validContents,
          "Invalid enum is rejected; saved file is unchanged");

    // Remove the required END record to simulate truncation.
    const auto endPosition = validContents.rfind("END ");

    if (endPosition == std::string::npos) {
        throw std::runtime_error("Valid snapshot has no END record.");
    }

    writeFile(path, validContents.substr(0, endPosition));

    const std::string damagedContents = readFile(path);
    const cas::LoadResult damaged = cas::loadSnapshot(path);

    check(!damaged.ok && damaged.data.accounts.empty() &&
              damaged.data.transactions.empty(),
          "Truncated snapshot is rejected without partial data");

    std::cout << "Truncated-load message: " << damaged.message << '\n';

    const cas::SaveResult overwrite =
        cas::saveSnapshot(path, cas::BankData{});

    check(!overwrite.ok && readFile(path) == damagedContents,
          "Malformed existing snapshot is not overwritten");

    // A directory cannot be opened as our temporary output file.
    const auto blockedPath = directory / "blocked_state.txt";
    std::filesystem::copy_file(path, blockedPath);
    auto blockedTemporary = blockedPath;
    blockedTemporary += ".tmp";
    std::filesystem::create_directory(blockedTemporary);

    // Restore valid data first so failure is caused by the blocked
    // temporary path, rather than by malformed existing contents.
    writeFile(blockedPath, validContents);

    const cas::SaveResult blockedSave =
        cas::saveSnapshot(blockedPath, updated);

    check(!blockedSave.ok &&
              readFile(blockedPath) == validContents &&
              cas::loadSnapshot(blockedPath).ok,
          "Blocked temporary path preserves the readable old snapshot");

    std::cout << "\nFailed checks: " << failures << '\n';
    return failures;
}

} // namespace

int main() {
    std::filesystem::path directory;
    bool ownsDirectory = false;

    try {
        const auto stamp =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count();

        directory = std::filesystem::temp_directory_path() /
                    ("cas-storage-test-" + std::to_string(stamp));

        ownsDirectory = std::filesystem::create_directory(directory);

        if (!ownsDirectory) {
            throw std::runtime_error(
                "Could not create a unique test directory.");
        }

        std::cout << "CaS Member 3 - Storage Tests\n\n";
        const int failures = runTests(directory);

        std::filesystem::remove_all(directory);
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "TEST ERROR: " << error.what() << '\n';

        if (ownsDirectory) {
            std::cerr << "Test files retained at: "
                      << directory.string() << '\n';
        }

        return 1;
    }
}