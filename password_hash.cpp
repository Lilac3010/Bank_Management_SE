#include "password_hash.h"

#include <climits>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")   // so the cl command lines need no extra library
#endif
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace {

#ifdef _WIN32

bool platformPbkdf2(const std::string& password, const std::string& salt, int iterations,
                    std::vector<unsigned char>& out) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                                    BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        return false;
    // CNG takes non-const pointers but does not modify the password or salt.
    std::vector<unsigned char> key(password.begin(), password.end());
    std::vector<unsigned char> saltBytes(salt.begin(), salt.end());
    NTSTATUS status = BCryptDeriveKeyPBKDF2(
        algorithm, key.empty() ? nullptr : key.data(), static_cast<ULONG>(key.size()),
        saltBytes.empty() ? nullptr : saltBytes.data(), static_cast<ULONG>(saltBytes.size()),
        static_cast<ULONGLONG>(iterations), out.data(), static_cast<ULONG>(out.size()), 0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return BCRYPT_SUCCESS(status);
}

bool platformRandom(std::vector<unsigned char>& out) {
    return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(out.size()),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

#else

bool platformPbkdf2(const std::string& password, const std::string& salt, int iterations,
                    std::vector<unsigned char>& out) {
    return PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                             reinterpret_cast<const unsigned char*>(salt.data()),
                             static_cast<int>(salt.size()), iterations, EVP_sha256(),
                             static_cast<int>(out.size()), out.data()) == 1;
}

bool platformRandom(std::vector<unsigned char>& out) {
    return RAND_bytes(out.data(), static_cast<int>(out.size())) == 1;
}

#endif

std::string toHex(const std::string& bytes) {
    const char* digits = "0123456789abcdef";
    std::string hex;
    for (unsigned char b : bytes) {
        hex += digits[b >> 4];
        hex += digits[b & 0x0f];
    }
    return hex;
}

// Input is already checked to be lowercase hex of even length.
std::string fromHex(const std::string& hex) {
    auto value = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::string bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes += static_cast<char>((value(hex[i]) << 4) | value(hex[i + 1]));
    return bytes;
}

// Compares every byte whatever the contents, so the time taken does not show how many
// leading bytes matched.
bool constantTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char difference = 0;
    for (size_t i = 0; i < a.size(); ++i)
        difference = difference | static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}

} // namespace

bool pbkdf2HmacSha256(const std::string& password, const std::string& salt, int iterations,
                      size_t length, std::string& out) {
    if (iterations < 1 || length == 0 || length > 1024 || password.size() > INT_MAX
        || salt.size() > INT_MAX)
        return false;
    std::vector<unsigned char> derived(length);
    if (!platformPbkdf2(password, salt, iterations, derived)) return false;
    out.assign(derived.begin(), derived.end());
    return true;
}

bool hashPassword(const std::string& password, std::string& stored) {
    std::vector<unsigned char> saltBytes(PASSWORD_SALT_BYTES);
    if (!platformRandom(saltBytes)) return false;
    std::string salt(saltBytes.begin(), saltBytes.end());

    std::string hash;
    if (!pbkdf2HmacSha256(password, salt, PASSWORD_HASH_ITERATIONS, PASSWORD_HASH_BYTES, hash))
        return false;
    stored = PASSWORD_HASH_ALGORITHM + "$" + std::to_string(PASSWORD_HASH_ITERATIONS) + "$"
             + toHex(salt) + "$" + toHex(hash);
    return true;
}

bool verifyPassword(const std::string& password, const std::string& stored) {
    if (!isWellFormedPasswordHash(stored)) return false;
    size_t first = PASSWORD_HASH_ALGORITHM.size() + 1;
    size_t second = stored.find('$', first);
    size_t third = stored.find('$', second + 1);
    int iterations = std::stoi(stored.substr(first, second - first));
    std::string salt = fromHex(stored.substr(second + 1, third - second - 1));
    std::string expected = fromHex(stored.substr(third + 1));

    std::string actual;
    if (!pbkdf2HmacSha256(password, salt, iterations, expected.size(), actual)) return false;
    return constantTimeEqual(actual, expected);
}
