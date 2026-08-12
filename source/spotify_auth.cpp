#include "spotify_auth.hpp"

#include <3ds.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr char kUnreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

uint32_t rotr(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32 - count));
}

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t length) {
    static constexpr uint32_t kInit[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    static constexpr uint32_t kTable[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };

    std::array<uint8_t, 32> digest{};
    uint32_t state[8];
    for (size_t index = 0; index < 8; ++index) {
        state[index] = kInit[index];
    }

    uint64_t bitLength = static_cast<uint64_t>(length) * 8;
    size_t paddedLength = ((length + 9 + 63) / 64) * 64;
    std::vector<uint8_t> buffer(paddedLength, 0);
    if (length > 0) {
        memcpy(buffer.data(), data, length);
    }
    buffer[length] = 0x80;
    for (size_t offset = 0; offset < 8; ++offset) {
        buffer[paddedLength - 1 - offset] = static_cast<uint8_t>((bitLength >> (offset * 8)) & 0xFF);
    }

    for (size_t blockOffset = 0; blockOffset < paddedLength; blockOffset += 64) {
        uint32_t schedule[64];
        for (size_t word = 0; word < 16; ++word) {
            size_t i = blockOffset + word * 4;
            schedule[word] = (static_cast<uint32_t>(buffer[i]) << 24)
                | (static_cast<uint32_t>(buffer[i + 1]) << 16)
                | (static_cast<uint32_t>(buffer[i + 2]) << 8)
                | static_cast<uint32_t>(buffer[i + 3]);
        }
        for (size_t word = 16; word < 64; ++word) {
            uint32_t s0 = rotr(schedule[word - 15], 7) ^ rotr(schedule[word - 15], 18) ^ (schedule[word - 15] >> 3);
            uint32_t s1 = rotr(schedule[word - 2], 17) ^ rotr(schedule[word - 2], 19) ^ (schedule[word - 2] >> 10);
            schedule[word] = schedule[word - 16] + s0 + schedule[word - 7] + s1;
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t f = state[5];
        uint32_t g = state[6];
        uint32_t h = state[7];

        for (size_t round = 0; round < 64; ++round) {
            uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t choose = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + s1 + choose + kTable[round] + schedule[round];
            uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    for (size_t index = 0; index < 8; ++index) {
        digest[index * 4] = static_cast<uint8_t>(state[index] >> 24);
        digest[index * 4 + 1] = static_cast<uint8_t>(state[index] >> 16);
        digest[index * 4 + 2] = static_cast<uint8_t>(state[index] >> 8);
        digest[index * 4 + 3] = static_cast<uint8_t>(state[index]);
    }

    return digest;
}

std::string base64UrlEncode(const uint8_t* data, size_t length) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string output;
    output.reserve(((length + 2) / 3) * 4);

    for (size_t offset = 0; offset < length; offset += 3) {
        uint32_t chunk = static_cast<uint32_t>(data[offset]) << 16;
        if (offset + 1 < length) {
            chunk |= static_cast<uint32_t>(data[offset + 1]) << 8;
        }
        if (offset + 2 < length) {
            chunk |= static_cast<uint32_t>(data[offset + 2]);
        }

        output.push_back(kAlphabet[(chunk >> 18) & 63]);
        output.push_back(kAlphabet[(chunk >> 12) & 63]);
        if (offset + 1 < length) {
            output.push_back(kAlphabet[(chunk >> 6) & 63]);
        }
        if (offset + 2 < length) {
            output.push_back(kAlphabet[chunk & 63]);
        }
    }

    return output;
}

std::string urlEncode(const std::string& input) {
    std::string output;
    static constexpr char hex[] = "0123456789ABCDEF";

    for (unsigned char ch : input) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            output.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            output += "%20";
        } else {
            output.push_back('%');
            output.push_back(hex[(ch >> 4) & 0xF]);
            output.push_back(hex[ch & 0xF]);
        }
    }

    return output;
}

std::string generateVerifier(size_t length) {
    std::mt19937 rng(static_cast<uint32_t>(svcGetSystemTick()));
    std::uniform_int_distribution<size_t> distribution(0, sizeof(kUnreserved) - 2);

    std::string verifier;
    verifier.reserve(length);
    for (size_t index = 0; index < length; ++index) {
        verifier.push_back(kUnreserved[distribution(rng)]);
    }
    return verifier;
}
}

SpotifyAuthRequest SpotifyAuth::createRequest(const SpotifyConfig& config) {
    std::string verifier = generateVerifier(64);
    auto hash = sha256(reinterpret_cast<const uint8_t*>(verifier.data()), verifier.size());

    return SpotifyAuthRequest{
        config.clientId.empty() ? "replace-with-your-spotify-client-id" : config.clientId,
        config.redirectUri.empty() ? "spotifyds://callback" : config.redirectUri,
        config.scope.empty() ? "user-read-playback-state user-modify-playback-state playlist-read-private playlist-read-collaborative user-read-private" : config.scope,
        verifier,
        base64UrlEncode(hash.data(), hash.size()),
    };
}

std::string SpotifyAuth::buildAuthorizationUrl(const SpotifyAuthRequest& request) {
    std::string url = "https://accounts.spotify.com/authorize?";
    url += "client_id=" + urlEncode(request.clientId);
    url += "&response_type=code";
    url += "&redirect_uri=" + urlEncode(request.redirectUri);
    url += "&scope=" + urlEncode(request.scope);
    url += "&code_challenge=" + urlEncode(request.codeChallenge);
    url += "&code_challenge_method=S256";
    return url;
}

std::string SpotifyAuth::buildTokenExchangeBody(const SpotifyAuthRequest& request, const std::string& code) {
    std::string body;
    body += "grant_type=authorization_code";
    body += "&client_id=" + urlEncode(request.clientId);
    body += "&code=" + urlEncode(code);
    body += "&redirect_uri=" + urlEncode(request.redirectUri);
    body += "&code_verifier=" + urlEncode(request.codeVerifier);
    return body;
}

std::string SpotifyAuth::buildRefreshBody(const SpotifyAuthRequest& request, const std::string& refreshToken) {
    std::string body;
    body += "grant_type=refresh_token";
    body += "&client_id=" + urlEncode(request.clientId);
    body += "&refresh_token=" + urlEncode(refreshToken);
    return body;
}

std::string SpotifyAuth::buildRefreshBody(const SpotifyAuthRequest& request, const std::string& refreshToken) {
    std::string body;
    body += "grant_type=refresh_token";
    body += "&client_id=" + urlEncode(request.clientId);
    body += "&refresh_token=" + urlEncode(refreshToken);
    return body;
}