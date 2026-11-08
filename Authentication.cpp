#include "Authentication.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#error "Authentication.cpp currently requires Windows CNG."
#endif

using namespace std;

namespace cas {
namespace {

constexpr char kDigestPrefix[] = "cas-pbkdf2-sha256-v1";
constexpr uint64_t kIterations = 150000;
constexpr size_t kSaltSize = 16;
constexpr size_t kDerivedSize = 32;

bool validPin(const string& pin) {
    if (pin.size() != 4 && pin.size() != 6) return false;
    for (unsigned char digit : pin) {
        if (digit < '0' || digit > '9') return false;
    }
    return true;
}

char hexDigit(unsigned int value) {
    return "0123456789abcdef"[value];
}

template <size_t N>
string toHex(const array<unsigned char, N>& bytes) {
    string output;
    output.reserve(N * 2);
    for (unsigned char byte : bytes) {
        output += hexDigit(byte >> 4);
        output += hexDigit(byte & 0x0f);
    }
    return output;
}

int fromHex(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

template <size_t N>
bool parseHex(const string& text, array<unsigned char, N>& bytes) {
    if (text.size() != N * 2) return false;
    for (size_t i = 0; i < N; ++i) {
        const int high = fromHex(text[i * 2]);
        const int low = fromHex(text[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        bytes[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return true;
}

array<unsigned char, kDerivedSize> derive(
    const string& pin, const array<unsigned char, kSaltSize>& salt) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) {
        throw runtime_error("Could not initialize PIN hashing");
    }

    vector<unsigned char> password(pin.begin(), pin.end());
    auto saltCopy = salt;
    array<unsigned char, kDerivedSize> derived{};
    const NTSTATUS status = BCryptDeriveKeyPBKDF2(
        algorithm, password.data(), static_cast<ULONG>(password.size()),
        saltCopy.data(), static_cast<ULONG>(saltCopy.size()), kIterations,
        derived.data(), static_cast<ULONG>(derived.size()), 0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw runtime_error("Could not hash PIN");
    return derived;
}

bool verifyPin(const string& pin, const string& stored) {
    const string prefix = string(kDigestPrefix) + "$" +
                          to_string(kIterations) + "$";
    array<unsigned char, kSaltSize> salt{};
    array<unsigned char, kDerivedSize> expected{};
    bool formatValid = false;
    if (stored.compare(0, prefix.size(), prefix) == 0) {
        const size_t separator = stored.find('$', prefix.size());
        if (separator != string::npos) {
            formatValid =
                parseHex(stored.substr(prefix.size(), separator - prefix.size()), salt) &&
                parseHex(stored.substr(separator + 1), expected);
        }
    }

    const auto actual = derive(pin, salt);
    unsigned char difference = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        difference |= actual[i] ^ expected[i];
    }
    return formatValid && difference == 0;
}

} // namespace

string digestPin(const string& pin) {
    if (!validPin(pin)) throw invalid_argument("PIN must have four or six digits");
    array<unsigned char, kSaltSize> salt{};
    if (BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        throw runtime_error("Could not generate PIN salt");
    }
    return string(kDigestPrefix) + "$" + to_string(kIterations) +
           "$" + toHex(salt) + "$" + toHex(derive(pin, salt));
}

LoginResult login(const BankData& data, const string& accountId,
                  const string& pin) {
    LoginResult result;
    result.message = "Invalid account ID or PIN.";
    if (!validPin(pin)) return result;

    const Account* matchedAccount = nullptr;
    for (const Account& account : data.accounts) {
        if (account.accountId == accountId) {
            matchedAccount = &account;
            break;
        }
    }
    const bool pinMatches = verifyPin(
        pin, matchedAccount ? matchedAccount->pinDigest : "");
    if (!matchedAccount || !pinMatches) return result;

    result.ok = true;
    result.role = matchedAccount->role;
    result.accountId = matchedAccount->accountId;
    result.message = "Login successful.";
    return result;
}

} // namespace cas
