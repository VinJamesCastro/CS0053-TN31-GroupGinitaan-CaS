#include "InputValidator.h"

#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace cas {
namespace {

bool isDigit(char character) {
    return character >= '0' && character <= '9';
}

std::string trim(const std::string& text) {
    constexpr char whitespace[] = " \t\r\n\f\v";

    const auto first = text.find_first_not_of(whitespace);

    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::string readLine(const std::string& prompt) {
    if (std::cin.bad()) {
        throw std::runtime_error("Input stream is unavailable.");
    }

    if (std::cin.eof()) {
        throw std::runtime_error("Input was closed.");
    }

    // Recover if an earlier formatted extraction failed.
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << prompt << std::flush;

    std::string text;

    if (!std::getline(std::cin, text)) {
        throw std::runtime_error("Input was closed or could not be read.");
    }

    return text;
}

bool parseInteger(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }

    int parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);

    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }

    value = parsed;
    return true;
}

bool parseMoney(const std::string& text, Money& value) {
    if (text.empty()) {
        return false;
    }

    constexpr Money maximum = std::numeric_limits<Money>::max();

    Money amount = 0;
    bool foundDecimal = false;
    int wholeDigits = 0;
    int fractionalDigits = 0;

    for (char character : text) {
        if (character == '.') {
            if (foundDecimal || wholeDigits == 0) {
                return false;
            }

            foundDecimal = true;
            continue;
        }

        if (!isDigit(character)) {
            return false;
        }

        if (foundDecimal) {
            ++fractionalDigits;

            if (fractionalDigits > 2) {
                return false;
            }
        } else {
            // Only presence matters, so avoid an unbounded counter.
            wholeDigits = 1;
        }

        const Money digit = character - '0';

        if (amount > (maximum - digit) / 10) {
            return false;
        }

        amount = amount * 10 + digit;
    }

    if (wholeDigits == 0 ||
        (foundDecimal && fractionalDigits == 0)) {
        return false;
    }

    // Add missing decimal places without using floating point.
    const int missingDigits = 2 - fractionalDigits;

    for (int index = 0; index < missingDigits; ++index) {
        if (amount > maximum / 10) {
            return false;
        }

        amount *= 10;
    }

    value = amount;
    return true;
}

} // namespace

int getValidInt(const std::string& prompt,
                int minInclusive,
                int maxInclusive) {
    if (minInclusive > maxInclusive) {
        throw std::invalid_argument(
            "Integer minimum must not exceed maximum.");
    }

    while (true) {
        const std::string text = trim(readLine(prompt));
        int value = 0;

        if (parseInteger(text, value) &&
            value >= minInclusive &&
            value <= maxInclusive) {
            return value;
        }

        std::cout << "Enter a whole number from "
                  << minInclusive << " to "
                  << maxInclusive << ".\n";
    }
}

Money getValidMoney(const std::string& prompt,
                    Money minInclusive) {
    if (minInclusive < 0) {
        throw std::invalid_argument(
            "Money minimum must be non-negative.");
    }

    while (true) {
        const std::string text = trim(readLine(prompt));
        Money value = 0;

        if (parseMoney(text, value) && value >= minInclusive) {
            return value;
        }

        std::cout
            << "Enter a non-negative amount with at most two decimal "
               "places, without commas or currency symbols.\n"
            << "Minimum: " << minInclusive << " centavos.\n";
    }
}

std::string getValidPin(const std::string& prompt) {
    while (true) {
        // Do not trim PINs: spaces are not valid PIN characters.
        const std::string pin = readLine(prompt);
        bool valid = pin.size() == 4 || pin.size() == 6;

        if (valid) {
            for (char character : pin) {
                if (!isDigit(character)) {
                    valid = false;
                    break;
                }
            }
        }

        if (valid) {
            return pin;
        }

        std::cout << "Enter exactly four or six digits.\n";
    }
}

}