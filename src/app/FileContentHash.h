// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace broke::library {
namespace detail {

class Sha256 final {
public:
    void update(const std::byte* data, std::size_t size) noexcept {
        if (data == nullptr || size == 0) return;
        totalBytes += static_cast<std::uint64_t>(size);
        while (size > 0) {
            const std::size_t copy = std::min(size, block.size() - used);
            for (std::size_t i = 0; i < copy; ++i)
                block[used + i] = std::to_integer<std::uint8_t>(data[i]);
            used += copy;
            data += copy;
            size -= copy;
            if (used == block.size()) {
                transform(block.data());
                used = 0;
            }
        }
    }

    [[nodiscard]] std::string finishHex() noexcept {
        const std::uint64_t bitLength = totalBytes * 8u;
        block[used++] = 0x80u;
        if (used > 56) {
            while (used < block.size()) block[used++] = 0;
            transform(block.data());
            used = 0;
        }
        while (used < 56) block[used++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8)
            block[used++] = static_cast<std::uint8_t>((bitLength >> shift) & 0xffu);
        transform(block.data());
        used = 0;

        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (const auto word : state) output << std::setw(8) << word;
        return output.str();
    }

private:
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

    static constexpr std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
        return (x & y) ^ (~x & z);
    }
    static constexpr std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    static constexpr std::uint32_t sigma0(std::uint32_t x) noexcept {
        return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
    }
    static constexpr std::uint32_t sigma1(std::uint32_t x) noexcept {
        return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
    }
    static constexpr std::uint32_t gamma0(std::uint32_t x) noexcept {
        return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
    }
    static constexpr std::uint32_t gamma1(std::uint32_t x) noexcept {
        return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
    }

    void transform(const std::uint8_t* data) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t offset = i * 4;
            words[i] = (static_cast<std::uint32_t>(data[offset]) << 24)
                     | (static_cast<std::uint32_t>(data[offset + 1]) << 16)
                     | (static_cast<std::uint32_t>(data[offset + 2]) << 8)
                     | static_cast<std::uint32_t>(data[offset + 3]);
        }
        for (std::size_t i = 16; i < words.size(); ++i)
            words[i] = gamma1(words[i - 2]) + words[i - 7] + gamma0(words[i - 15]) + words[i - 16];

        auto a = state[0]; auto b = state[1]; auto c = state[2]; auto d = state[3];
        auto e = state[4]; auto f = state[5]; auto g = state[6]; auto h = state[7];
        for (std::size_t i = 0; i < words.size(); ++i) {
            const auto temp1 = h + sigma1(e) + choose(e, f, g) + constants[i] + words[i];
            const auto temp2 = sigma0(a) + majority(a, b, c);
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    std::array<std::uint32_t, 8> state{
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    std::array<std::uint8_t, 64> block{};
    std::size_t used = 0;
    std::uint64_t totalBytes = 0;
};

} // namespace detail

[[nodiscard]] inline std::optional<std::string> sha256File(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;

    detail::Sha256 hash;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = input.gcount();
        if (read > 0)
            hash.update(reinterpret_cast<const std::byte*>(buffer.data()), static_cast<std::size_t>(read));
    }
    if (input.bad()) return std::nullopt;
    return hash.finishHex();
}

} // namespace broke::library
