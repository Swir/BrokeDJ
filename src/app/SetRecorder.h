// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// App-side set recorder. The audio callback only copies the already-rendered
// master 1/2 signal into a fixed-capacity SPSC FIFO. File creation, WAV encoding,
// flush/finalize and filesystem moves happen on the writer/message threads.
class SetRecorder final : private juce::Thread {
public:
    struct Snapshot final {
        bool recording = false;
        bool finalized = false;
        std::uint64_t writtenFrames = 0;
        std::uint64_t droppedFrames = 0;
        std::uint64_t dropoutEvents = 0;
        juce::File destination;
        juce::File recoveryFile;
        juce::String error;
    };

    explicit SetRecorder(int fifoFrames = 262144)
        : juce::Thread("BrokeDJ set recorder"),
          fifo(std::max(4096, fifoFrames)),
          left(static_cast<std::size_t>(std::max(4096, fifoFrames))),
          right(static_cast<std::size_t>(std::max(4096, fifoFrames))) {}

    ~SetRecorder() override { stop(); }

    SetRecorder(const SetRecorder&) = delete;
    SetRecorder& operator=(const SetRecorder&) = delete;

    [[nodiscard]] bool start(const juce::File& requestedDestination, double sampleRate) {
        stop();
        if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0) {
            setError("Recording rejected: unsupported sample rate.");
            return false;
        }

        auto target = requestedDestination;
        if (target.getFileExtension().toLowerCase() != ".wav") target = target.withFileExtension("wav");
        if (target.exists()) target = target.getNonexistentSibling(false);
        if (!target.getParentDirectory().isDirectory()
            && !target.getParentDirectory().createDirectory()) {
            setError("Recording rejected: destination directory is unavailable.");
            return false;
        }

        auto recovery = target.getSiblingFile(target.getFileName() + ".part");
        if (recovery.exists()) recovery = recovery.getNonexistentSibling(false);
        auto stream = recovery.createOutputStream();
        if (!stream) {
            setError("Recording rejected: could not create the recovery file.");
            return false;
        }

        juce::WavAudioFormat wav;
        auto* rawStream = stream.get();
        std::unique_ptr<juce::AudioFormatWriter> nextWriter(
            wav.createWriterFor(rawStream, sampleRate, 2u, 24, {}, 0));
        if (!nextWriter) {
            setError("Recording rejected: WAV writer could not be created.");
            return false;
        }
        static_cast<void>(stream.release()); // ownership transferred to the writer

        fifo.reset();
        writtenFrames.store(0, std::memory_order_relaxed);
        droppedFrames.store(0, std::memory_order_relaxed);
        dropoutEvents.store(0, std::memory_order_relaxed);
        activeCaptures.store(0, std::memory_order_relaxed);
        finalized.store(false, std::memory_order_relaxed);
        writerFailed.store(false, std::memory_order_relaxed);
        {
            const juce::ScopedLock lock(stateLock);
            destination = target;
            recoveryFile = recovery;
            lastError.clear();
            writer = std::move(nextWriter);
        }

        accepting.store(true, std::memory_order_release);
        if (!startThread(juce::Thread::Priority::normal)) {
            accepting.store(false, std::memory_order_release);
            {
                const juce::ScopedLock lock(stateLock);
                writer.reset();
            }
            static_cast<void>(recovery.deleteFile());
            setError("Recording rejected: background writer thread could not start.");
            return false;
        }
        return true;
    }

    // May be called while audio continues. Publishing is disabled first, then we
    // wait only for a capture that was already inside the bounded callback handoff.
    // Disk finalization remains on the writer thread; the FIFO/storage lifetime is
    // stable for the whole component lifetime.
    void stop() {
        accepting.store(false, std::memory_order_release);

        constexpr int captureDrainTimeoutMs = 2000;
        int waitedMs = 0;
        while (activeCaptures.load(std::memory_order_acquire) != 0
               && waitedMs < captureDrainTimeoutMs) {
            juce::Thread::sleep(1);
            ++waitedMs;
        }
        if (activeCaptures.load(std::memory_order_acquire) != 0) {
            writerFailed.store(true, std::memory_order_release);
            setError("Recording callback did not quiesce within the shutdown bound; the .part recovery file was retained.");
        }

        signalThreadShouldExit();
        notify();
        if (isThreadRunning() && !stopThread(10000)) {
            writerFailed.store(true, std::memory_order_release);
            setError("Recording writer did not stop cleanly within the shutdown bound; the .part recovery file was retained.");
        }
    }

    // Realtime entry point. No I/O, mutex, allocation, condition-variable notify
    // or unbounded retry. The writer polls the FIFO from its background thread.
    // A full FIFO drops the unavailable tail and records explicit evidence.
    void capture(const float* masterLeft, const float* masterRight, int frames) noexcept {
        if (frames <= 0) return;

        // Increment before observing `accepting`. This closes the stop/capture race:
        // stop() disables new work, then waits for any callback that had already
        // published itself here before allowing the writer to finalize.
        activeCaptures.fetch_add(1, std::memory_order_acq_rel);
        if (!accepting.load(std::memory_order_acquire)) {
            activeCaptures.fetch_sub(1, std::memory_order_release);
            return;
        }

        if (masterLeft == nullptr || masterRight == nullptr) {
            noteDrop(static_cast<std::uint64_t>(frames));
            activeCaptures.fetch_sub(1, std::memory_order_release);
            return;
        }

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(frames, start1, size1, start2, size2);
        const int accepted = size1 + size2;
        if (size1 > 0) {
            std::copy_n(masterLeft, size1, left.data() + start1);
            std::copy_n(masterRight, size1, right.data() + start1);
        }
        if (size2 > 0) {
            std::copy_n(masterLeft + size1, size2, left.data() + start2);
            std::copy_n(masterRight + size1, size2, right.data() + start2);
        }
        fifo.finishedWrite(accepted);
        if (accepted < frames) noteDrop(static_cast<std::uint64_t>(frames - accepted));
        activeCaptures.fetch_sub(1, std::memory_order_release);
    }

    [[nodiscard]] bool isRecording() const noexcept {
        return accepting.load(std::memory_order_acquire);
    }

    [[nodiscard]] Snapshot snapshot() const {
        Snapshot result;
        result.recording = isRecording();
        result.finalized = finalized.load(std::memory_order_acquire);
        result.writtenFrames = writtenFrames.load(std::memory_order_relaxed);
        result.droppedFrames = droppedFrames.load(std::memory_order_relaxed);
        result.dropoutEvents = dropoutEvents.load(std::memory_order_relaxed);
        const juce::ScopedLock lock(stateLock);
        result.destination = destination;
        result.recoveryFile = recoveryFile;
        result.error = lastError;
        return result;
    }

private:
    void noteDrop(std::uint64_t frames) noexcept {
        droppedFrames.fetch_add(frames, std::memory_order_relaxed);
        dropoutEvents.fetch_add(1, std::memory_order_relaxed);
    }

    void setError(const juce::String& message) {
        const juce::ScopedLock lock(stateLock);
        lastError = message;
    }

    void run() override {
        constexpr int drainBlock = 2048;
        std::array<float, drainBlock> drainLeft{};
        std::array<float, drainBlock> drainRight{};

        for (;;) {
            const int available = fifo.getNumReady();
            if (available <= 0) {
                if (threadShouldExit()) break;
                wait(10);
                continue;
            }

            const int requested = std::min(drainBlock, available);
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead(requested, start1, size1, start2, size2);
            int copied = 0;
            if (size1 > 0) {
                std::copy_n(left.data() + start1, size1, drainLeft.data());
                std::copy_n(right.data() + start1, size1, drainRight.data());
                copied += size1;
            }
            if (size2 > 0) {
                std::copy_n(left.data() + start2, size2, drainLeft.data() + copied);
                std::copy_n(right.data() + start2, size2, drainRight.data() + copied);
                copied += size2;
            }

            bool ok = false;
            {
                // Only the writer thread dereferences writer while recording.
                // stateLock is never acquired by capture().
                const juce::ScopedLock lock(stateLock);
                if (writer) {
                    const float* channels[2] {drainLeft.data(), drainRight.data()};
                    ok = writer->writeFromFloatArrays(channels, 2, copied);
                }
            }
            fifo.finishedRead(copied);
            if (!ok) {
                writerFailed.store(true, std::memory_order_release);
                accepting.store(false, std::memory_order_release);
                setError("Recording writer failed; the .part recovery file was retained.");
                break;
            }
            writtenFrames.fetch_add(static_cast<std::uint64_t>(copied), std::memory_order_relaxed);
        }

        juce::File finalTarget;
        juce::File recovery;
        bool flushOk = true;
        {
            const juce::ScopedLock lock(stateLock);
            if (writer) flushOk = writer->flush();
            writer.reset();
            finalTarget = destination;
            recovery = recoveryFile;
        }
        if (!flushOk) {
            writerFailed.store(true, std::memory_order_release);
            setError("Recording flush failed; the .part recovery file was retained.");
        }

        if (!writerFailed.load(std::memory_order_acquire) && recovery.existsAsFile()) {
            // Never replace a file that appeared after recording started. A
            // concurrent creator wins; BrokeDJ keeps the complete recovery file.
            if (finalTarget.exists()) {
                writerFailed.store(true, std::memory_order_release);
                setError("Recording destination appeared during capture; it was not overwritten and the .part recovery file was retained.");
            } else if (recovery.moveFileTo(finalTarget)) {
                finalized.store(true, std::memory_order_release);
            } else {
                setError("Recording data was written, but final rename failed; the .part recovery file was retained.");
            }
        }
    }

    juce::AbstractFifo fifo;
    std::vector<float> left;
    std::vector<float> right;
    std::atomic<bool> accepting{false};
    std::atomic<bool> writerFailed{false};
    std::atomic<bool> finalized{false};
    std::atomic<int> activeCaptures{0};
    std::atomic<std::uint64_t> writtenFrames{0};
    std::atomic<std::uint64_t> droppedFrames{0};
    std::atomic<std::uint64_t> dropoutEvents{0};

    mutable juce::CriticalSection stateLock;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::File destination;
    juce::File recoveryFile;
    juce::String lastError;
};
