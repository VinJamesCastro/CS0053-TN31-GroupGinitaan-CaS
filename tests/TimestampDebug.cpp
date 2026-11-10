#include "FileStorage.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    std::filesystem::path directory;
    bool ownsDirectory = false;

    try {
        const auto stamp =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count();

        directory = std::filesystem::temp_directory_path() /
                    ("cas-timestamp-debug-" + std::to_string(stamp));

        ownsDirectory = std::filesystem::create_directory(directory);

        if (!ownsDirectory) {
            throw std::runtime_error(
                "Cannot create a unique test directory.");
        }

        const auto path = directory / "bank_state.txt";

        // Handwritten malformed snapshot:
        // transaction timestamp decodes to 2026-11-08T99:00:00Z.
        // All remaining fields and record counts are valid.
        {
            std::ofstream output(path, std::ios::binary);

            output
                << "CAS_SNAPSHOT 1\n"
                << "ACCOUNT "
                << "4143432D303030303031 "
                << "54657374 "
                << "646967657374 "
                << "0 0 0 100 0 0\n"
                << "TRANSACTION "
                << "54584E2D303030303031 "
                << "323032362D31312D30385439393A30303A30305A "
                << "0 - 4143432D303030303031 100 0 100\n"
                << "END 1 1\n";

            output.close();

            if (output.fail()) {
                throw std::runtime_error("Cannot write test snapshot.");
            }
        }

        const cas::LoadResult result = cas::loadSnapshot(path);

        std::cout << "Timestamp: 2026-11-08T99:00:00Z\n";
        std::cout << "Expected load ok: false\n";
        std::cout << "Actual load ok: "
                  << std::boolalpha << result.ok << '\n';
        std::cout << "Message: " << result.message << '\n';

        const bool passed = !result.ok;

        std::cout
            << (passed
                    ? "PASS: Invalid timestamp rejected.\n"
                    : "FAIL: Invalid timestamp accepted.\n");

        std::filesystem::remove_all(directory);
        return passed ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "TEST ERROR: " << exception.what() << '\n';

        if (ownsDirectory) {
            std::cerr << "Test files retained at: "
                      << directory.string() << '\n';
        }

        return 1;
    }
}