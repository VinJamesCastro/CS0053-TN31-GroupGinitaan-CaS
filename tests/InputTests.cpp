#include "InputValidator.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

// Temporarily supplies scripted input and captures prompt output.
// The original console streams are restored when this object is destroyed.
class ScriptedConsole {
public:
    explicit ScriptedConsole(const std::string& script)
        : input(script),
          previousInputState(std::cin.rdstate()),
          previousInputBuffer(std::cin.rdbuf(input.rdbuf())),
          previousOutputBuffer(std::cout.rdbuf(output.rdbuf())) {
        std::cin.clear();
    }

    ScriptedConsole(const ScriptedConsole&) = delete;
    ScriptedConsole& operator=(const ScriptedConsole&) = delete;

    ~ScriptedConsole() {
        std::cout.rdbuf(previousOutputBuffer);
        std::cin.rdbuf(previousInputBuffer);
        std::cin.clear(previousInputState);
    }

    std::string messages() const {
        return output.str();
    }

private:
    std::istringstream input;
    std::ostringstream output;
    std::ios::iostate previousInputState;
    std::streambuf* previousInputBuffer;
    std::streambuf* previousOutputBuffer;
};

std::size_t countMessages(const std::string& text,
                          const std::string& message) {
    std::size_t count = 0;
    std::size_t position = 0;

    while ((position = text.find(message, position)) !=
           std::string::npos) {
        ++count;
        position += message.size();
    }

    return count;
}

} // namespace

int main() {
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

    try {
        std::cout << "CaS Member 3 - Input Tests\n\n";

        bool passed = false;

        {
            ScriptedConsole console(
                "abc\n2abc\n0\n10\n999999999999999999999\n1\n");

            const int result = cas::getValidInt("Choice: ", 1, 9);

            passed =
                result == 1 &&
                countMessages(console.messages(),
                              "Enter a whole number") == 5;
        }

        check(passed,
              "Integer prompt rejects five invalid inputs, then returns 1");

        {
            ScriptedConsole console("9\n");
            passed = cas::getValidInt("Choice: ", 1, 9) == 9;
        }

        check(passed, "Integer upper boundary 9 is accepted");

        {
            ScriptedConsole console(
                "abc\n-1.00\n0.001\n0.00\n"
                "92233720368547758.08\n0.01\n");

            const cas::Money result =
                cas::getValidMoney("Amount: ", 1);

            passed =
                result == 1 &&
                countMessages(console.messages(),
                              "Enter a non-negative amount") == 5;
        }

        check(passed,
              "Money prompt rejects five invalid inputs, then returns 1 centavo");

        {
            ScriptedConsole console("92233720368547758.07\n");

            passed =
                cas::getValidMoney("Amount: ", 1) ==
                std::numeric_limits<cas::Money>::max();
        }

        check(passed, "Maximum Money value is accepted exactly");

        {
            ScriptedConsole console(" 0123\n0123\n");

            passed =
                cas::getValidPin("PIN: ") == "0123" &&
                countMessages(console.messages(),
                              "Enter exactly four or six digits.") == 1;
        }

        check(passed,
              "Four-digit PIN preserves its zero and rejects spaces");

        {
            ScriptedConsole console("12a4\n12345\n001234\n");

            passed =
                cas::getValidPin("PIN: ") == "001234" &&
                countMessages(console.messages(),
                              "Enter exactly four or six digits.") == 2;
        }

        check(passed,
              "Six-digit PIN preserves zeroes after invalid entries");

        {
            ScriptedConsole console("");
            passed = false;

            try {
                (void)cas::getValidInt("Choice: ", 1, 9);
            } catch (const std::runtime_error&) {
                passed = true;
            }
        }

        check(passed, "Closed input reports an error instead of looping");

        {
            ScriptedConsole console("abc\n5\n");

            int unused = 0;
            std::cin >> unused; // Reproduce an earlier extraction failure.

            const bool initiallyFailed = std::cin.fail();
            const int result = cas::getValidInt("Choice: ", 1, 9);

            passed = initiallyFailed && result == 5;
        }

        check(passed, "Previously failed input stream recovers and returns 5");

        {
            ScriptedConsole console("");
            passed = false;

            try {
                (void)cas::getValidInt("Choice: ", 9, 1);
            } catch (const std::invalid_argument&) {
                passed = true;
            }
        }

        check(passed, "Invalid caller range is rejected");

        std::cout << "\nFailed checks: " << failures << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "TEST ERROR: " << exception.what() << '\n';
        return 1;
    }
}