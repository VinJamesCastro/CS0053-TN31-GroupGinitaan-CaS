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

bool sameEntry(const cas::AuditEntry& left,
               const cas::AuditEntry& right) {
    return std::tie(left.occurredAt,
                    left.adminAccountId,
                    left.targetAccountId,
                    left.previousStatus,
                    left.nextStatus) ==
           std::tie(right.occurredAt,
                    right.adminAccountId,
                    right.targetAccountId,
                    right.previousStatus,
                    right.nextStatus);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Cannot open test file.");
    }

    std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    if (input.bad()) {
        throw std::runtime_error("Cannot read test file.");
    }

    return contents;
}

void writeFile(const std::filesystem::path& path,
               const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents;
    output.close();

    if (output.fail()) {
        throw std::runtime_error("Cannot write test file.");
    }
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
                    ("cas-audit-test-" + std::to_string(stamp));

        ownsDirectory = std::filesystem::create_directory(directory);

        if (!ownsDirectory) {
            throw std::runtime_error(
                "Could not create a unique test directory.");
        }

        const auto path = directory / "audit.log";
        int failures = 0;

        const auto check = [&failures](
                               bool condition,
                               const std::string& description) {
            std::cout << (condition ? "PASS: " : "FAIL: ")
                      << description << '\n';

            if (!condition) {
                ++failures;
            }
        };

        std::cout << "CaS Member 3 - Audit Tests\n\n";

        check(cas::readAuditLogs(path).empty(),
              "Missing audit log returns no entries");

        writeFile(path, "");

        check(cas::readAuditLogs(path).empty(),
              "Empty audit log returns no entries");

        cas::AuditEntry first{};
        first.occurredAt = "2026-11-08T09:00:00Z";
        first.adminAccountId = "ADMIN-000001";
        first.targetAccountId = "ACC-000001";
        first.previousStatus = cas::AccountStatus::Active;
        first.nextStatus = cas::AccountStatus::Frozen;

        const auto firstSave = cas::appendAuditLog(path, first);
        const auto firstRead = cas::readAuditLogs(path);

        check(firstSave.ok && firstRead.size() == 1 &&
                  sameEntry(first, firstRead[0]),
              "All audit fields survive append/read");

        cas::AuditEntry second = first;
        second.occurredAt = "2026-11-08T10:00:00Z";
        second.previousStatus = cas::AccountStatus::Frozen;
        second.nextStatus = cas::AccountStatus::Active;

        const auto secondSave = cas::appendAuditLog(path, second);
        const auto secondRead = cas::readAuditLogs(path);

        check(secondSave.ok && secondRead.size() == 2 &&
                  sameEntry(first, secondRead[0]) &&
                  sameEntry(second, secondRead[1]),
              "Appending preserves the earlier entry");

        const std::string validContents = readFile(path);

        cas::AuditEntry invalid = first;
        invalid.occurredAt = "2026-02-30T09:00:00Z";

        const auto invalidSave = cas::appendAuditLog(path, invalid);

        check(!invalidSave.ok && readFile(path) == validContents,
              "Invalid entry is rejected without changing the log");

        // A complete final record may lack a newline.
        std::string withoutNewline = validContents;

        if (!withoutNewline.empty() &&
            withoutNewline.back() == '\n') {
            withoutNewline.pop_back();
        }

        writeFile(path, withoutNewline);

        const auto newlineSave = cas::appendAuditLog(path, first);
        const auto newlineRead = cas::readAuditLogs(path);

        check(newlineSave.ok && newlineRead.size() == 3 &&
                  sameEntry(first, newlineRead[0]) &&
                  sameEntry(second, newlineRead[1]) &&
                  sameEntry(first, newlineRead[2]),
              "Appending handles a missing final newline");

        writeFile(path, "AUDIT 1 incomplete\n");
        const std::string damagedContents = readFile(path);

        bool rejectedMalformed = false;

        try {
            (void)cas::readAuditLogs(path);
        } catch (const std::runtime_error&) {
            rejectedMalformed = true;
        }

        check(rejectedMalformed,
              "Malformed audit record reports an error");

        const auto damagedSave = cas::appendAuditLog(path, first);

        check(!damagedSave.ok &&
                  readFile(path) == damagedContents,
              "Malformed existing log is not extended");

        std::cout << "\nFailed checks: " << failures << '\n';

        if (failures == 0) {
            std::filesystem::remove_all(directory);
        } else {
            std::cout << "Test files retained at: "
                      << directory.string() << '\n';
        }

        return failures == 0 ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "TEST ERROR: " << exception.what() << '\n';

        if (ownsDirectory) {
            std::cerr << "Test files retained at: "
                      << directory.string() << '\n';
        }

        return 1;
    }
}