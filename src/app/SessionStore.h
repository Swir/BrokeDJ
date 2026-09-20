// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>
#include <cmath>

namespace broke::session {

struct DeckState final {
    std::string path;
    double positionSeconds = 0.0;
    float playbackRate = 1.0f;
    float trimDb = 0.0f;
    float channelGain = 0.7f;
    float low = 1.0f;
    float mid = 1.0f;
    float high = 1.0f;
    float echo = 0.0f;
    float drive = 0.0f;
    bool headphoneCue = false;
    bool wholeTrackLoop = false;
    bool wasPlaying = false;
};

struct MixerState final {
    float crossfader = 0.5f;
    float master = 0.5f;
    float headphoneLevel = 0.5f;
};

struct SessionState final {
    static constexpr std::size_t deckCount = 4;
    std::array<DeckState, deckCount> decks{};
    MixerState mixer{};
};

class SessionStore final {
public:
    static constexpr std::uint32_t currentSchemaVersion = 1;
    static constexpr std::size_t maxSessionBytes = 1024 * 1024;
    static constexpr std::size_t maxPathBytes = 64 * 1024;

    [[nodiscard]] bool save(const std::filesystem::path& file,
                            const SessionState& state,
                            std::string* error = nullptr) const noexcept {
        try {
            if (file.empty()) {
                setError(error, "Session path is empty");
                return false;
            }
            if (!valid(state)) {
                setError(error, "Session contains invalid or non-finite state");
                return false;
            }

            std::vector<std::uint8_t> payload;
            payload.reserve(512);
            writeFloat(payload, state.mixer.crossfader);
            writeFloat(payload, state.mixer.master);
            writeFloat(payload, state.mixer.headphoneLevel);
            for (const auto& deck : state.decks) {
                if (!writeString(payload, deck.path)) {
                    setError(error, "Session track path is too large");
                    return false;
                }
                writeDouble(payload, deck.positionSeconds);
                writeFloat(payload, deck.playbackRate);
                writeFloat(payload, deck.trimDb);
                writeFloat(payload, deck.channelGain);
                writeFloat(payload, deck.low);
                writeFloat(payload, deck.mid);
                writeFloat(payload, deck.high);
                writeFloat(payload, deck.echo);
                writeFloat(payload, deck.drive);
                std::uint8_t flags = 0;
                if (deck.headphoneCue) flags |= 0x01u;
                if (deck.wholeTrackLoop) flags |= 0x02u;
                if (deck.wasPlaying) flags |= 0x04u;
                payload.push_back(flags);
            }
            if (payload.size() > maxSessionBytes) {
                setError(error, "Session payload exceeds the bounded size limit");
                return false;
            }

            std::error_code fsError;
            const auto parent = file.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent, fsError);
                if (fsError) {
                    setError(error, "Unable to create session directory");
                    return false;
                }
            }

            auto temp = file;
            temp += ".tmp";
            auto backup = file;
            backup += ".bak";
            std::filesystem::remove(temp, fsError);
            fsError.clear();

            {
                std::ofstream output(temp, std::ios::binary | std::ios::trunc);
                if (!output) {
                    setError(error, "Unable to open temporary session file");
                    return false;
                }
                output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
                writeU32(output, currentSchemaVersion);
                writeU32(output, static_cast<std::uint32_t>(payload.size()));
                writeU64(output, checksum(payload));
                if (!payload.empty()) {
                    output.write(reinterpret_cast<const char*>(payload.data()),
                                 static_cast<std::streamsize>(payload.size()));
                }
                output.flush();
                if (!output) {
                    output.close();
                    std::filesystem::remove(temp, fsError);
                    setError(error, "Unable to write complete session file");
                    return false;
                }
            }

            std::string verifyError;
            if (!loadFile(temp, &verifyError).has_value()) {
                std::filesystem::remove(temp, fsError);
                setError(error, std::string("Temporary session verification failed: ") + verifyError);
                return false;
            }

            const bool hadExisting = std::filesystem::exists(file, fsError) && !fsError;
            fsError.clear();
            if (hadExisting) {
                std::filesystem::remove(backup, fsError);
                fsError.clear();
                std::filesystem::rename(file, backup, fsError);
                if (fsError) {
                    std::filesystem::remove(temp, fsError);
                    setError(error, "Unable to preserve previous session before replacement");
                    return false;
                }
            }

            fsError.clear();
            std::filesystem::rename(temp, file, fsError);
            if (fsError) {
                if (hadExisting) {
                    std::error_code rollbackError;
                    std::filesystem::rename(backup, file, rollbackError);
                }
                std::filesystem::remove(temp, fsError);
                setError(error, "Unable to publish saved session atomically");
                return false;
            }

            if (hadExisting) {
                fsError.clear();
                std::filesystem::remove(backup, fsError);
            }
            return true;
        } catch (...) {
            setError(error, "Unexpected failure while saving session");
            return false;
        }
    }

    [[nodiscard]] std::optional<SessionState> load(
        const std::filesystem::path& file,
        std::string* error = nullptr) const noexcept {
        try {
            return loadFile(file, error);
        } catch (...) {
            setError(error, "Unexpected failure while loading session");
            return std::nullopt;
        }
    }

private:
    static constexpr std::array<char, 8> magic{'B', 'R', 'K', 'S', 'E', 'S', 'S', 'N'};

    static void setError(std::string* output, std::string_view message) {
        if (output != nullptr) output->assign(message);
    }

    [[nodiscard]] static bool finiteInRange(float value, float minimum, float maximum) noexcept {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }

    [[nodiscard]] static bool valid(const SessionState& state) noexcept {
        if (!finiteInRange(state.mixer.crossfader, 0.0f, 1.0f)
            || !finiteInRange(state.mixer.master, 0.0f, 2.0f)
            || !finiteInRange(state.mixer.headphoneLevel, 0.0f, 2.0f)) {
            return false;
        }
        for (const auto& deck : state.decks) {
            if (deck.path.size() > maxPathBytes
                || !std::isfinite(deck.positionSeconds) || deck.positionSeconds < 0.0
                || deck.positionSeconds > 7.0 * 24.0 * 60.0 * 60.0
                || !finiteInRange(deck.playbackRate, 0.05f, 8.0f)
                || !finiteInRange(deck.trimDb, -96.0f, 48.0f)
                || !finiteInRange(deck.channelGain, 0.0f, 8.0f)
                || !finiteInRange(deck.low, 0.0f, 8.0f)
                || !finiteInRange(deck.mid, 0.0f, 8.0f)
                || !finiteInRange(deck.high, 0.0f, 8.0f)
                || !finiteInRange(deck.echo, 0.0f, 2.0f)
                || !finiteInRange(deck.drive, 0.0f, 24.0f)) {
                return false;
            }
        }
        return true;
    }

    static void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    static void appendU64(std::vector<std::uint8_t>& output, std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8)
            output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    static void writeFloat(std::vector<std::uint8_t>& output, float value) {
        appendU32(output, std::bit_cast<std::uint32_t>(value));
    }

    static void writeDouble(std::vector<std::uint8_t>& output, double value) {
        appendU64(output, std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] static bool writeString(std::vector<std::uint8_t>& output,
                                          const std::string& value) {
        if (value.size() > maxPathBytes
            || value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        appendU32(output, static_cast<std::uint32_t>(value.size()));
        output.insert(output.end(), value.begin(), value.end());
        return true;
    }

    static void writeU32(std::ostream& output, std::uint32_t value) {
        std::array<char, 4> bytes{};
        for (int i = 0; i < 4; ++i)
            bytes[static_cast<std::size_t>(i)] = static_cast<char>((value >> (i * 8)) & 0xffu);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    static void writeU64(std::ostream& output, std::uint64_t value) {
        std::array<char, 8> bytes{};
        for (int i = 0; i < 8; ++i)
            bytes[static_cast<std::size_t>(i)] = static_cast<char>((value >> (i * 8)) & 0xffu);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    [[nodiscard]] static bool readU32(const std::vector<std::uint8_t>& bytes,
                                      std::size_t& cursor, std::uint32_t& value) noexcept {
        if (cursor > bytes.size() || bytes.size() - cursor < 4) return false;
        value = 0;
        for (int i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes[cursor++]) << (i * 8);
        return true;
    }

    [[nodiscard]] static bool readU64(const std::vector<std::uint8_t>& bytes,
                                      std::size_t& cursor, std::uint64_t& value) noexcept {
        if (cursor > bytes.size() || bytes.size() - cursor < 8) return false;
        value = 0;
        for (int i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(bytes[cursor++]) << (i * 8);
        return true;
    }

    [[nodiscard]] static bool readFloat(const std::vector<std::uint8_t>& bytes,
                                        std::size_t& cursor, float& value) noexcept {
        std::uint32_t raw = 0;
        if (!readU32(bytes, cursor, raw)) return false;
        value = std::bit_cast<float>(raw);
        return true;
    }

    [[nodiscard]] static bool readDouble(const std::vector<std::uint8_t>& bytes,
                                         std::size_t& cursor, double& value) noexcept {
        std::uint64_t raw = 0;
        if (!readU64(bytes, cursor, raw)) return false;
        value = std::bit_cast<double>(raw);
        return true;
    }

    [[nodiscard]] static bool readString(const std::vector<std::uint8_t>& bytes,
                                         std::size_t& cursor, std::string& value) {
        std::uint32_t size = 0;
        if (!readU32(bytes, cursor, size) || size > maxPathBytes
            || cursor > bytes.size() || bytes.size() - cursor < size) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(bytes.data() + cursor), size);
        cursor += size;
        return true;
    }

    [[nodiscard]] static std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) noexcept {
        std::uint64_t hash = 1469598103934665603ull;
        for (const auto byte : bytes) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    [[nodiscard]] static std::optional<SessionState> loadFile(
        const std::filesystem::path& file, std::string* error) {
        if (file.empty()) {
            setError(error, "Session path is empty");
            return std::nullopt;
        }

        std::error_code fsError;
        const auto size = std::filesystem::file_size(file, fsError);
        constexpr std::size_t headerBytes = magic.size() + 4 + 4 + 8;
        if (fsError || size < headerBytes || size > maxSessionBytes + headerBytes) {
            setError(error, "Session file size is invalid or outside the bounded limit");
            return std::nullopt;
        }

        std::ifstream input(file, std::ios::binary);
        if (!input) {
            setError(error, "Unable to open session file");
            return std::nullopt;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            setError(error, "Unable to read complete session file");
            return std::nullopt;
        }

        std::size_t cursor = 0;
        for (const char expected : magic) {
            if (cursor >= bytes.size() || bytes[cursor++] != static_cast<std::uint8_t>(expected)) {
                setError(error, "Session magic is invalid");
                return std::nullopt;
            }
        }
        std::uint32_t version = 0;
        std::uint32_t payloadSize = 0;
        std::uint64_t expectedChecksum = 0;
        if (!readU32(bytes, cursor, version) || !readU32(bytes, cursor, payloadSize)
            || !readU64(bytes, cursor, expectedChecksum)) {
            setError(error, "Session header is truncated");
            return std::nullopt;
        }
        if (version > currentSchemaVersion) {
            setError(error, "Session was created by a newer BrokeDJ schema");
            return std::nullopt;
        }
        if (version == 0 || payloadSize > maxSessionBytes
            || cursor > bytes.size() || bytes.size() - cursor != payloadSize) {
            setError(error, "Session payload length is invalid");
            return std::nullopt;
        }

        std::vector<std::uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end());
        if (checksum(payload) != expectedChecksum) {
            setError(error, "Session checksum mismatch");
            return std::nullopt;
        }
        cursor = 0;
        SessionState state;
        if (!readFloat(payload, cursor, state.mixer.crossfader)
            || !readFloat(payload, cursor, state.mixer.master)
            || !readFloat(payload, cursor, state.mixer.headphoneLevel)) {
            setError(error, "Session mixer payload is truncated");
            return std::nullopt;
        }
        for (auto& deck : state.decks) {
            if (!readString(payload, cursor, deck.path)
                || !readDouble(payload, cursor, deck.positionSeconds)
                || !readFloat(payload, cursor, deck.playbackRate)
                || !readFloat(payload, cursor, deck.trimDb)
                || !readFloat(payload, cursor, deck.channelGain)
                || !readFloat(payload, cursor, deck.low)
                || !readFloat(payload, cursor, deck.mid)
                || !readFloat(payload, cursor, deck.high)
                || !readFloat(payload, cursor, deck.echo)
                || !readFloat(payload, cursor, deck.drive)
                || cursor >= payload.size()) {
                setError(error, "Session deck payload is truncated");
                return std::nullopt;
            }
            const auto flags = payload[cursor++];
            if ((flags & 0xf8u) != 0) {
                setError(error, "Session deck flags contain unsupported bits");
                return std::nullopt;
            }
            deck.headphoneCue = (flags & 0x01u) != 0;
            deck.wholeTrackLoop = (flags & 0x02u) != 0;
            deck.wasPlaying = (flags & 0x04u) != 0;
        }
        if (cursor != payload.size() || !valid(state)) {
            setError(error, "Session payload contains invalid or trailing state");
            return std::nullopt;
        }
        return state;
    }
};

} // namespace broke::session
