// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "MainComponent.h"
class BrokeDJApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "BrokeDJ"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& arguments) override {
        logger.reset(juce::FileLogger::createDefaultAppLogger("BrokeDJ", "BrokeDJ.log", "BrokeDJ 0.1.0 development log", 256 * 1024));
        juce::Logger::setCurrentLogger(logger.get());
        const bool smokeTest = arguments.contains("--smoke-test");
        const bool keyLockResearch = arguments.contains("--key-lock-research");
        window = std::make_unique<Window>(!smokeTest, keyLockResearch);
        if (smokeTest) juce::Timer::callAfterDelay(1200, [this] { quit(); });
    }
    void shutdown() override { window.reset(); juce::Logger::setCurrentLogger(nullptr); logger.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    class Window final : public juce::DocumentWindow {
    public:
        Window(bool openAudio, bool keyLockResearch)
            : DocumentWindow("BrokeDJ — by Swir", juce::Colour(0xff080e1a), allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(openAudio, keyLockResearch), true);
            setResizable(true, false); setResizeLimits(1050, 800, 3840, 2160);
            centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(BrokeDJApplication)
