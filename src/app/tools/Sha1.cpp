// SHA-1：下载校验用，不引第三方库。
#include "app/tools/tools.h"

#include <cstdint>
#include <fstream>
#include <iterator>

namespace {

inline uint32_t Rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void Sha1Block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0, k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = Rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rotl(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}  // namespace

namespace tools {

std::string Sha1Hex(const void* data, size_t size) {
    auto bytes = static_cast<const uint8_t*>(data);
    uint32_t state[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    size_t full = size / 64;
    for (size_t i = 0; i < full; ++i) Sha1Block(state, bytes + i * 64);

    uint8_t tail[128] = {0};
    size_t remainder = size % 64;
    for (size_t i = 0; i < remainder; ++i) tail[i] = bytes[full * 64 + i];
    tail[remainder] = 0x80;
    size_t tail_length = (remainder + 1 + 8 <= 64) ? 64 : 128;
    uint64_t bits = static_cast<uint64_t>(size) * 8;
    for (int i = 0; i < 8; ++i) tail[tail_length - 1 - i] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
    for (size_t offset = 0; offset < tail_length; offset += 64) Sha1Block(state, tail + offset);

    static const char* digits = "0123456789abcdef";
    std::string out(40, '0');
    for (int i = 0; i < 5; ++i) {
        uint32_t value = state[i];
        for (int k = 0; k < 4; ++k) {
            uint8_t byte = static_cast<uint8_t>((value >> ((3 - k) * 8)) & 0xFF);
            out[(i * 4 + k) * 2] = digits[byte >> 4];
            out[(i * 4 + k) * 2 + 1] = digits[byte & 0x0F];
        }
    }
    return out;
}

bool Sha1File(const std::string& path, std::string& hex) {
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return false;
    std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    hex = Sha1Hex(data.data(), data.size());
    return true;
}

}  // namespace tools
