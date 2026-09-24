// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstdint>

// Small message-thread-only accent theme owner for the first Beta workstation.
// It intentionally keeps BrokeDJ's dark structural surface unchanged and only
// recolours controls that already use one of BrokeDJ's theme-owned colours.
// Semantic colours (record red, microphone amber, signal green) are preserved.
// No audio/DSP state is read or written here. Settings I/O happens only from the
// message-thread startup/menu path and never from the realtime callback.
class BrokeThemeManager final {
public:
    enum class Theme : int {
        electricBlue = 1,
        ultraviolet = 2,
        ember = 3,
    };

    struct Palette final {
        juce::Colour surfaceRaised;
        juce::Colour outline;
        juce::Colour accent;
        juce::Colour accentAlt;
        juce::Colour accentDeep;
    };

    static Theme currentTheme() noexcept {
        return sanitiseTheme(currentThemeId.load(std::memory_order_acquire));
    }

    static juce::String themeName(Theme theme) {
        switch (theme) {
            case Theme::ultraviolet: return "Ultraviolet";
            case Theme::ember: return "Ember";
            case Theme::electricBlue:
            default: return "Electric Blue";
        }
    }

    static Palette palette(Theme theme) noexcept {
        switch (theme) {
            case Theme::ultraviolet:
                return {juce::Colour{0xff1b1530}, juce::Colour{0xff3b285e},
                        juce::Colour{0xff8a5cff}, juce::Colour{0xffd6b3ff},
                        juce::Colour{0xff503093}};
            case Theme::ember:
                return {juce::Colour{0xff2a1b0d}, juce::Colour{0xff5a3a18},
                        juce::Colour{0xffff9f1c}, juce::Colour{0xffffd166},
                        juce::Colour{0xff9a5510}};
            case Theme::electricBlue:
            default:
                return {juce::Colour{0xff0d1b2a}, juce::Colour{0xff183451},
                        juce::Colour{0xff0088ff}, juce::Colour{0xff62e5ff},
                        juce::Colour{0xff07528d}};
        }
    }

    // User-initiated theme change. Saving is deliberately outside the audio path.
    static void apply(juce::Component& root, Theme theme) {
        theme = sanitiseTheme(static_cast<int>(theme));
        applyInternal(root, theme);
        saveTheme(theme);
    }

    // Startup restore. A missing/corrupt setting fails closed to Electric Blue.
    static void applyPersisted(juce::Component& root) {
        applyInternal(root, loadTheme());
    }

    static void showMenu(juce::Component& anchor) {
        juce::PopupMenu menu;
        menu.addSectionHeader(localText("BrokeDJ accent theme", "Motyw akcentów BrokeDJ"));
        const auto selected = currentTheme();
        menu.addItem(1, "Electric Blue", true, selected == Theme::electricBlue);
        menu.addItem(2, "Ultraviolet", true, selected == Theme::ultraviolet);
        menu.addItem(3, "Ember", true, selected == Theme::ember);

        juce::Component::SafePointer<juce::Component> safeAnchor(&anchor);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&anchor),
                           [safeAnchor](int result) {
            if (safeAnchor == nullptr || result < 1 || result > 3) return;
            auto* top = safeAnchor->getTopLevelComponent();
            if (top == nullptr) top = safeAnchor.getComponent();
            if (top != nullptr)
                apply(*top, sanitiseTheme(result));
        });
    }

private:
    static Theme sanitiseTheme(int raw) noexcept {
        switch (raw) {
            case static_cast<int>(Theme::ultraviolet): return Theme::ultraviolet;
            case static_cast<int>(Theme::ember): return Theme::ember;
            case static_cast<int>(Theme::electricBlue):
            default: return Theme::electricBlue;
        }
    }

    static juce::String localText(const char* english, const char* polish) {
        const bool usePolish = juce::SystemStats::getUserLanguage().startsWithIgnoreCase("pl");
        return juce::String::fromUTF8(usePolish ? polish : english);
    }

    static juce::PropertiesFile::Options settingsOptions() {
        juce::PropertiesFile::Options options;
        options.applicationName = "BrokeDJ";
        options.filenameSuffix = "settings";
        options.folderName = "BrokeDJ";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers = false;
        options.ignoreCaseOfKeyNames = false;
        options.doNotSave = false;
        options.millisecondsBeforeSaving = 0;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        return options;
    }

    static Theme loadTheme() {
        juce::PropertiesFile settings(settingsOptions());
        if (!settings.isValidFile()) return Theme::electricBlue;
        return sanitiseTheme(settings.getIntValue("uiAccentTheme",
                                                  static_cast<int>(Theme::electricBlue)));
    }

    static void saveTheme(Theme theme) {
        juce::PropertiesFile settings(settingsOptions());
        if (!settings.isValidFile()) return;
        settings.setValue("uiAccentTheme", static_cast<int>(theme));
        static_cast<void>(settings.saveIfNeeded());
    }

    static void applyInternal(juce::Component& root, Theme theme) {
        currentThemeId.store(static_cast<int>(theme), std::memory_order_release);
        const auto colours = palette(theme);
        applyLookAndFeelDefaults(root.getLookAndFeel(), colours);
        applyRecursive(root, colours);
        root.repaint();
    }

    static constexpr std::array<std::uint32_t, 15> ownedArgb {
        0xff0d1b2au, 0xff183451u, 0xff0088ffu, 0xff62e5ffu, 0xff07528du,
        0xff1b1530u, 0xff3b285eu, 0xff8a5cffu, 0xffd6b3ffu, 0xff503093u,
        0xff2a1b0du, 0xff5a3a18u, 0xffff9f1cu, 0xffffd166u, 0xff9a5510u,
    };

    static bool themeOwned(juce::Colour colour) noexcept {
        const auto argb = static_cast<std::uint32_t>(colour.getARGB());
        for (const auto owned : ownedArgb)
            if (argb == owned) return true;
        return false;
    }

    static void applyLookAndFeelDefaults(juce::LookAndFeel& lookAndFeel,
                                         const Palette& colours) {
        lookAndFeel.setColour(juce::TextButton::buttonColourId, colours.surfaceRaised);
        lookAndFeel.setColour(juce::TextButton::buttonOnColourId, colours.accentDeep);
        lookAndFeel.setColour(juce::Slider::trackColourId, colours.accent);
        lookAndFeel.setColour(juce::Slider::thumbColourId, colours.accentAlt);
        lookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, colours.accentAlt);
        lookAndFeel.setColour(juce::Slider::rotarySliderOutlineColourId, colours.outline);
        lookAndFeel.setColour(juce::Slider::textBoxOutlineColourId, colours.outline);
        lookAndFeel.setColour(juce::Slider::textBoxHighlightColourId,
                              colours.accent.withAlpha(0.62f));
        lookAndFeel.setColour(juce::ComboBox::backgroundColourId, colours.surfaceRaised);
        lookAndFeel.setColour(juce::ComboBox::outlineColourId, colours.outline);
        lookAndFeel.setColour(juce::ComboBox::arrowColourId, colours.accentAlt);
        lookAndFeel.setColour(juce::ComboBox::focusedOutlineColourId,
                              colours.accentAlt.withAlpha(0.90f));
        lookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId,
                              colours.accentDeep);
        lookAndFeel.setColour(juce::PopupMenu::headerTextColourId, colours.accentAlt);
        lookAndFeel.setColour(juce::TextEditor::highlightColourId,
                              colours.accent.withAlpha(0.62f));
        lookAndFeel.setColour(juce::TextEditor::outlineColourId, colours.outline);
        lookAndFeel.setColour(juce::TextEditor::focusedOutlineColourId,
                              colours.accentAlt.withAlpha(0.90f));
        lookAndFeel.setColour(juce::HyperlinkButton::textColourId, colours.accentAlt);
    }

    static void applyRecursive(juce::Component& component, const Palette& colours) {
        if (auto* button = dynamic_cast<juce::TextButton*>(&component)) {
            if (themeOwned(button->findColour(juce::TextButton::buttonColourId)))
                button->setColour(juce::TextButton::buttonColourId, colours.surfaceRaised);
            if (themeOwned(button->findColour(juce::TextButton::buttonOnColourId)))
                button->setColour(juce::TextButton::buttonOnColourId, colours.accentDeep);
        }

        if (auto* slider = dynamic_cast<juce::Slider*>(&component)) {
            if (themeOwned(slider->findColour(juce::Slider::trackColourId)))
                slider->setColour(juce::Slider::trackColourId, colours.accent);
            if (themeOwned(slider->findColour(juce::Slider::thumbColourId)))
                slider->setColour(juce::Slider::thumbColourId, colours.accentAlt);
            if (themeOwned(slider->findColour(juce::Slider::rotarySliderFillColourId)))
                slider->setColour(juce::Slider::rotarySliderFillColourId, colours.accentAlt);
            if (themeOwned(slider->findColour(juce::Slider::rotarySliderOutlineColourId)))
                slider->setColour(juce::Slider::rotarySliderOutlineColourId, colours.outline);
            if (themeOwned(slider->findColour(juce::Slider::textBoxOutlineColourId)))
                slider->setColour(juce::Slider::textBoxOutlineColourId, colours.outline);
        }

        if (auto* combo = dynamic_cast<juce::ComboBox*>(&component)) {
            if (themeOwned(combo->findColour(juce::ComboBox::backgroundColourId)))
                combo->setColour(juce::ComboBox::backgroundColourId, colours.surfaceRaised);
            if (themeOwned(combo->findColour(juce::ComboBox::outlineColourId)))
                combo->setColour(juce::ComboBox::outlineColourId, colours.outline);
            if (themeOwned(combo->findColour(juce::ComboBox::arrowColourId)))
                combo->setColour(juce::ComboBox::arrowColourId, colours.accentAlt);
            if (themeOwned(combo->findColour(juce::ComboBox::focusedOutlineColourId)))
                combo->setColour(juce::ComboBox::focusedOutlineColourId,
                                 colours.accentAlt.withAlpha(0.90f));
        }

        if (auto* link = dynamic_cast<juce::HyperlinkButton*>(&component))
            link->setColour(juce::HyperlinkButton::textColourId, colours.accentAlt);

        for (int index = 0; index < component.getNumChildComponents(); ++index)
            if (auto* child = component.getChildComponent(index))
                applyRecursive(*child, colours);

        component.repaint();
    }

    inline static std::atomic<int> currentThemeId{static_cast<int>(Theme::electricBlue)};
};
