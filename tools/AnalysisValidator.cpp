// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#include "app/TrackAnalysis.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace {
struct Reference final {
    juce::String id;
    juce::File source;
    std::optional<double> bpm;
    double bpmTolerance = 0.0;
    std::optional<int> tonic;
    broke::KeyMode mode = broke::KeyMode::unknown;
};

struct Totals final {
    int rows = 0;
    int sourceErrors = 0;
    int bpmReferences = 0;
    int bpmPassed = 0;
    int keyReferences = 0;
    int keyPassed = 0;
    double bpmAbsoluteErrorSum = 0.0;
};

void usage() {
    std::cerr
        << "Usage: brokedj_analysis_validator <manifest.tsv> [--max-seconds N]\n"
        << "Manifest columns: id<TAB>audio<TAB>bpm|-<TAB>tolerance|-<TAB>tonic(0..11)|-<TAB>major|minor|-\n"
        << "Paths may be relative to the manifest. Output prints row IDs, never source paths.\n";
}

bool parseFiniteDouble(const juce::String& text, double& value) {
    if (text.isEmpty()) return false;
    const auto utf8 = text.toRawUTF8();
    char* end = nullptr;
    value = std::strtod(utf8, &end);
    return end != utf8 && *end == '\0' && std::isfinite(value);
}

std::optional<Reference> parseReference(const juce::String& line,
                                        const juce::File& manifest,
                                        juce::String& error) {
    juce::StringArray fields;
    fields.addTokens(line, "\t", "");
    if (fields.size() != 6) {
        error = "expected exactly 6 tab-separated columns";
        return std::nullopt;
    }

    Reference ref;
    ref.id = fields[0].trim();
    const auto pathText = fields[1].trim();
    if (ref.id.isEmpty() || pathText.isEmpty()) {
        error = "id and audio path are required";
        return std::nullopt;
    }
    ref.source = juce::File::isAbsolutePath(pathText)
        ? juce::File(pathText)
        : manifest.getParentDirectory().getChildFile(pathText);

    const auto bpmText = fields[2].trim();
    const auto toleranceText = fields[3].trim();
    if (bpmText != "-") {
        double expected = 0.0;
        double tolerance = 0.0;
        if (!parseFiniteDouble(bpmText, expected) || expected < 30.0 || expected > 300.0
            || !parseFiniteDouble(toleranceText, tolerance) || tolerance <= 0.0 || tolerance > 30.0) {
            error = "BPM must be 30..300 and tolerance must be >0..30, or both fields must use '-'";
            return std::nullopt;
        }
        ref.bpm = expected;
        ref.bpmTolerance = tolerance;
    } else if (toleranceText != "-") {
        error = "BPM tolerance must be '-' when BPM is omitted";
        return std::nullopt;
    }

    const auto tonicText = fields[4].trim();
    const auto modeText = fields[5].trim().toLowerCase();
    if (tonicText != "-") {
        double tonicNumber = 0.0;
        if (!parseFiniteDouble(tonicText, tonicNumber)
            || std::floor(tonicNumber) != tonicNumber || tonicNumber < 0.0 || tonicNumber > 11.0) {
            error = "key tonic must be an integer 0..11, or '-'";
            return std::nullopt;
        }
        if (modeText == "major") ref.mode = broke::KeyMode::major;
        else if (modeText == "minor") ref.mode = broke::KeyMode::minor;
        else {
            error = "key mode must be major/minor when tonic is provided";
            return std::nullopt;
        }
        ref.tonic = static_cast<int>(tonicNumber);
    } else if (modeText != "-") {
        error = "key mode must be '-' when tonic is omitted";
        return std::nullopt;
    }

    if (!ref.bpm && !ref.tonic) {
        error = "each row needs at least a BPM or key reference";
        return std::nullopt;
    }
    return ref;
}

juce::String keyLabel(const broke::MusicalKeyResult& key) {
    if (!key.valid) return "—";
    return juce::String(broke::keyName(key.tonic))
        + (key.mode == broke::KeyMode::major ? " major" : " minor");
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return EXIT_FAILURE;
    }

    const juce::File manifest(juce::String::fromUTF8(argv[1]));
    double maxSeconds = 180.0;
    if (argc == 4 && juce::String::fromUTF8(argv[2]) == "--max-seconds") {
        if (!parseFiniteDouble(juce::String::fromUTF8(argv[3]), maxSeconds)
            || maxSeconds < 5.0 || maxSeconds > 1800.0) {
            std::cerr << "Invalid --max-seconds; expected 5..1800.\n";
            return EXIT_FAILURE;
        }
    } else if (argc != 2) {
        usage();
        return EXIT_FAILURE;
    }

    if (!manifest.existsAsFile() || manifest.getSize() <= 0 || manifest.getSize() > 1024 * 1024) {
        std::cerr << "Manifest is missing, empty or larger than 1 MiB.\n";
        return EXIT_FAILURE;
    }

    juce::StringArray lines;
    lines.addLines(manifest.loadFileAsString());
    Totals totals;
    bool manifestError = false;
    std::atomic<bool> cancelled{false};
    TrackAnalysisOptions options;
    options.beat.maxAnalysisSeconds = maxSeconds;
    options.key.maxAnalysisSeconds = maxSeconds;

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const auto line = lines[lineIndex].trim();
        if (line.isEmpty() || line.startsWithChar('#')) continue;

        juce::String parseError;
        const auto parsed = parseReference(line, manifest, parseError);
        if (!parsed) {
            manifestError = true;
            std::cerr << "ROW " << (lineIndex + 1) << " INVALID: "
                      << parseError.toStdString() << '\n';
            continue;
        }
        const auto& ref = *parsed;
        ++totals.rows;
        if (!ref.source.existsAsFile()) {
            ++totals.sourceErrors;
            std::cout << ref.id.toStdString() << "\tSOURCE=MISSING\n";
            continue;
        }

        const auto result = analyzeTrackRhythm(ref.source, cancelled, options);
        bool rowPass = true;
        std::cout << ref.id.toStdString();
        if (ref.bpm) {
            ++totals.bpmReferences;
            if (result.beat.valid) {
                const double error = std::abs(result.beat.bpm - *ref.bpm);
                totals.bpmAbsoluteErrorSum += error;
                const bool pass = error <= ref.bpmTolerance;
                if (pass) ++totals.bpmPassed;
                rowPass = rowPass && pass;
                std::cout << "\tBPM=" << (pass ? "PASS" : "FAIL")
                          << " detected=" << result.beat.bpm
                          << " error=" << error
                          << " confidence=" << result.beat.confidence;
            } else {
                rowPass = false;
                std::cout << "\tBPM=NO_ESTIMATE";
            }
        }
        if (ref.tonic) {
            ++totals.keyReferences;
            const bool pass = result.key.valid
                && result.key.tonic == *ref.tonic && result.key.mode == ref.mode;
            if (pass) ++totals.keyPassed;
            rowPass = rowPass && pass;
            std::cout << "\tKEY=" << (pass ? "PASS" : "FAIL")
                      << " detected=" << keyLabel(result.key).toStdString()
                      << " confidence=" << result.key.confidence;
        }
        if (result.error.isNotEmpty())
            std::cout << "\tANALYSIS_ERROR=" << result.error.toStdString();
        std::cout << "\tROW=" << (rowPass ? "PASS" : "FAIL") << '\n';
    }

    if (totals.rows == 0) {
        std::cerr << "Manifest contains no validation rows.\n";
        return EXIT_FAILURE;
    }

    std::cout << "SUMMARY\trows=" << totals.rows
              << "\tsource_errors=" << totals.sourceErrors
              << "\tbpm=" << totals.bpmPassed << '/' << totals.bpmReferences
              << "\tkey=" << totals.keyPassed << '/' << totals.keyReferences;
    if (totals.bpmReferences > 0)
        std::cout << "\tbpm_mae=" << (totals.bpmAbsoluteErrorSum / totals.bpmReferences);
    std::cout << '\n';

    const bool passed = !manifestError && totals.sourceErrors == 0
        && totals.bpmPassed == totals.bpmReferences
        && totals.keyPassed == totals.keyReferences;
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
