#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include <string>

// FR-10 - employee password hashing with PBKDF2-HMAC-SHA256, implemented in password_hash.cpp
// with the operating system's own crypto library (no hashing code of our own):
//   Windows: CNG (BCryptDeriveKeyPBKDF2, BCryptGenRandom), linked with bcrypt.lib.
//   Linux:   OpenSSL libcrypto (PKCS5_PBKDF2_HMAC, RAND_bytes), linked with -lcrypto.
//
// Stored form (one text field, lowercase hex):
//   pbkdf2-sha256$<iterations>$<16-byte salt: 32 hex>$<32-byte hash: 64 hex>
// e.g. pbkdf2-sha256$600000$00112233445566778899aabbccddeeff$<64 hex digits>

const std::string PASSWORD_HASH_ALGORITHM = "pbkdf2-sha256";
const int PASSWORD_HASH_ITERATIONS = 600000;        // OWASP recommendation for PBKDF2-SHA256
const int PASSWORD_HASH_MAX_ITERATIONS = 10000000;  // upper bound accepted when reading
const size_t PASSWORD_SALT_BYTES = 16;
const size_t PASSWORD_HASH_BYTES = 32;

// Raw PBKDF2-HMAC-SHA256: derives `length` bytes into `out`. Returns false if the platform
// library reports an error or the arguments are out of range (iterations < 1, length 0).
bool pbkdf2HmacSha256(const std::string& password, const std::string& salt, int iterations,
                      size_t length, std::string& out);

// Hashes `password` with a fresh random salt and PASSWORD_HASH_ITERATIONS into the stored
// form above. Returns false (and leaves `stored` unchanged) if the random salt or the hash
// cannot be produced.
bool hashPassword(const std::string& password, std::string& stored);

// True only if `stored` is well formed and `password` produces the same hash. The hashes are
// compared in constant time. A malformed `stored` value never matches.
bool verifyPassword(const std::string& password, const std::string& stored);

// Strict format check of a stored hash: exact algorithm name, iteration count written in
// plain decimal (no sign or leading zero) between PASSWORD_HASH_ITERATIONS and
// PASSWORD_HASH_MAX_ITERATIONS, and exactly 32 + 64 lowercase hex digits.
// Header-only, so storage.cpp can validate records without linking the crypto library.
inline bool isWellFormedPasswordHash(const std::string& stored) {
    auto isLowerHex = [](const std::string& s, size_t length) {
        if (s.size() != length) return false;
        for (char c : s)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        return true;
    };
    const std::string prefix = PASSWORD_HASH_ALGORITHM + "$";
    if (stored.compare(0, prefix.size(), prefix) != 0) return false;
    size_t first = prefix.size();
    size_t second = stored.find('$', first);
    if (second == std::string::npos) return false;
    size_t third = stored.find('$', second + 1);
    if (third == std::string::npos) return false;

    std::string iterations = stored.substr(first, second - first);
    if (iterations.empty() || iterations.size() > 8 || iterations[0] == '0') return false;
    long long count = 0;
    for (char c : iterations) {
        if (c < '0' || c > '9') return false;
        count = count * 10 + (c - '0');
    }
    if (count < PASSWORD_HASH_ITERATIONS || count > PASSWORD_HASH_MAX_ITERATIONS) return false;

    return isLowerHex(stored.substr(second + 1, third - second - 1), PASSWORD_SALT_BYTES * 2)
           && isLowerHex(stored.substr(third + 1), PASSWORD_HASH_BYTES * 2);
}

#endif
