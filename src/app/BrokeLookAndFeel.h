// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

// BrokeDJ's native visual system. The class deliberately stays presentation-only:
// it owns no transport/audio state and performs no file I/O. All drawing happens
// on JUCE's message thread and reuses the controls' existing colour IDs so deck
// state (for example active Hot Cues) remains visible.
class BrokeLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    BrokeLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, background());
        setColour(juce::Label::textColourId, primaryText());
        setColour(juce::HyperlinkButton::textColourId, cyan());

        setColour(juce::TextButton::buttonColourId, surfaceRaised());
        setColour(juce::TextButton::buttonOnColourId, accentDeep());
        setColour(juce::TextButton::textColourOffId, primaryText());
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        setColour(juce::Slider::backgroundColourId, surface());
        setColour(juce::Slider::trackColourId, accentBlue().withAlpha(0.82f));
        setColour(juce::Slider::thumbColourId, cyan());
        setColour(juce::Slider::rotarySliderFillColourId, cyan());
        setColour(juce::Slider::rotarySliderOutlineColourId, outline());
        setColour(juce::Slider::textBoxBackgroundColourId, surface());
        setColour(juce::Slider::textBoxTextColourId, primaryText());
        setColour(juce::Slider::textBoxOutlineColourId, outline());
        setColour(juce::Slider::textBoxHighlightColourId, accentBlue().withAlpha(0.65f));

        setColour(juce::ComboBox::backgroundColourId, surfaceRaised());
        setColour(juce::ComboBox::textColourId, primaryText());
        setColour(juce::ComboBox::outlineColourId, outline());
        setColour(juce::ComboBox::arrowColourId, cyan());
        setColour(juce::ComboBox::focusedOutlineColourId, cyan().withAlpha(0.85f));

        setColour(juce::PopupMenu::backgroundColourId, surface());
        setColour(juce::PopupMenu::textColourId, primaryText());
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentDeep());
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, cyan());

        setColour(juce::TextEditor::backgroundColourId, surface());
        setColour(juce::TextEditor::textColourId, primaryText());
        setColour(juce::TextEditor::highlightColourId, accentBlue().withAlpha(0.65f));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId, outline());
        setColour(juce::TextEditor::focusedOutlineColourId, cyan().withAlpha(0.85f));
        setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

        setColour(juce::TooltipWindow::backgroundColourId, surfaceRaised());
        setColour(juce::TooltipWindow::textColourId, primaryText());
        setColour(juce::TooltipWindow::outlineColourId, cyan().withAlpha(0.45f));

        setColour(juce::AlertWindow::backgroundColourId, surface());
        setColour(juce::AlertWindow::textColourId, primaryText());
        setColour(juce::AlertWindow::outlineColourId, cyan().withAlpha(0.55f));
    }

    static juce::Colour background() noexcept { return juce::Colour(0xff02050a); }
    static juce::Colour surface() noexcept { return juce::Colour(0xff07111c); }
    static juce::Colour surfaceRaised() noexcept { return juce::Colour(0xff0d1b2a); }
    static juce::Colour accentBlue() noexcept { return juce::Colour(0xff0088ff); }
    static juce::Colour cyan() noexcept { return juce::Colour(0xff62e5ff); }
    static juce::Colour primaryText() noexcept { return juce::Colour(0xfff4faff); }
    static juce::Colour mutedText() noexcept { return juce::Colour(0xff8da8b8); }
    static juce::Colour outline() noexcept { return juce::Colour(0xff183451); }
    static juce::Colour accentDeep() noexcept { return juce::Colour(0xff07528d); }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        const auto size = juce::jlimit(10.5f, 13.5f, static_cast<float>(buttonHeight) * 0.34f);
        return juce::Font(juce::FontOptions(size).withStyle("Bold"));
    }

    void drawButtonBackground(juce::Graphics& graphics, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.75f);
        const auto enabled = button.isEnabled();
        const auto active = button.getToggleState();

        auto fill = active ? accentDeep() : backgroundColour;
        if (!enabled) fill = fill.withAlpha(0.34f);
        else if (down) fill = fill.brighter(0.16f);
        else if (highlighted) fill = fill.brighter(0.08f);

        graphics.setColour(juce::Colours::black.withAlpha(enabled ? 0.28f : 0.16f));
        graphics.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 6.0f);
        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds, 6.0f);

        const auto border = active ? cyan().withAlpha(enabled ? 0.86f : 0.30f)
                                   : outline().withAlpha(enabled ? 0.95f : 0.42f);
        graphics.setColour(border);
        graphics.drawRoundedRectangle(bounds, 6.0f, active ? 1.5f : 1.0f);

        if (active && enabled) {
            auto glow = bounds.reduced(4.0f);
            glow.setY(glow.getBottom() - 2.0f);
            glow.setHeight(2.0f);
            graphics.setColour(cyan().withAlpha(0.82f));
            graphics.fillRoundedRectangle(glow, 1.0f);
        }

        if (button.hasKeyboardFocus(true) && enabled) {
            graphics.setColour(cyan().withAlpha(0.7f));
            graphics.drawRoundedRectangle(bounds.reduced(2.0f), 4.0f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& graphics, juce::TextButton& button,
                        bool, bool) override {
        graphics.setFont(getTextButtonFont(button, button.getHeight()));
        auto colour = button.findColour(button.getToggleState()
                                            ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId);
        if (!button.isEnabled()) colour = colour.withAlpha(0.38f);
        graphics.setColour(colour);
        graphics.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(7, 2),
                                juce::Justification::centred, 1, 0.76f);
    }

    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));
        const auto textBoxSpace = slider.getTextBoxPosition() == juce::Slider::TextBoxBelow
            ? std::min(22.0f, bounds.getHeight() * 0.24f) : 0.0f;
        bounds = bounds.withTrimmedBottom(textBoxSpace).reduced(5.0f);

        const auto radius = std::max(4.0f, std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f);
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        graphics.strokePath(track, juce::PathStrokeType(3.4f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

        if (slider.isEnabled()) {
            juce::Path value;
            value.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, angle, true);
            graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            graphics.strokePath(value, juce::PathStrokeType(3.4f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        }

        const auto knobRadius = radius * 0.60f;
        graphics.setColour(surfaceRaised());
        graphics.fillEllipse(centreX - knobRadius, centreY - knobRadius,
                             knobRadius * 2.0f, knobRadius * 2.0f);
        graphics.setColour(outline());
        graphics.drawEllipse(centreX - knobRadius, centreY - knobRadius,
                             knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        const auto indicatorLength = knobRadius * 0.72f;
        const auto inner = knobRadius * 0.22f;
        const auto sinAngle = std::sin(angle);
        const auto cosAngle = std::cos(angle);
        graphics.setColour(slider.isEnabled() ? cyan() : mutedText().withAlpha(0.35f));
        graphics.drawLine(centreX + inner * sinAngle, centreY - inner * cosAngle,
                          centreX + indicatorLength * sinAngle,
                          centreY - indicatorLength * cosAngle, 2.0f);
        graphics.fillEllipse(centreX - 1.5f, centreY - 1.5f, 3.0f, 3.0f);
    }

    void drawLinearSlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override {
        const bool vertical = style == juce::Slider::LinearVertical
            || style == juce::Slider::LinearBarVertical
            || style == juce::Slider::TwoValueVertical
            || style == juce::Slider::ThreeValueVertical;

        auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(width), static_cast<float>(height));
        const auto trackThickness = vertical
            ? std::max(3.0f, std::min(6.0f, area.getWidth() * 0.16f))
            : std::max(3.0f, std::min(6.0f, area.getHeight() * 0.20f));

        juce::Rectangle<float> track;
        if (vertical)
            track = {area.getCentreX() - trackThickness * 0.5f, area.getY() + 5.0f,
                     trackThickness, std::max(1.0f, area.getHeight() - 10.0f)};
        else
            track = {area.getX() + 5.0f, area.getCentreY() - trackThickness * 0.5f,
                     std::max(1.0f, area.getWidth() - 10.0f), trackThickness};

        graphics.setColour(outline());
        graphics.fillRoundedRectangle(track, trackThickness * 0.5f);

        auto valueTrack = track;
        if (vertical) {
            const auto clamped = juce::jlimit(track.getY(), track.getBottom(), sliderPos);
            valueTrack.setY(clamped);
            valueTrack.setHeight(track.getBottom() - clamped);
        } else {
            const auto clamped = juce::jlimit(track.getX(), track.getRight(), sliderPos);
            valueTrack.setWidth(std::max(0.0f, clamped - track.getX()));
        }
        graphics.setColour(slider.isEnabled()
                               ? slider.findColour(juce::Slider::trackColourId)
                               : mutedText().withAlpha(0.24f));
        graphics.fillRoundedRectangle(valueTrack, trackThickness * 0.5f);

        const auto thumbRadius = slider.isEnabled() ? 6.0f : 5.0f;
        const auto thumbX = vertical ? track.getCentreX()
                                     : juce::jlimit(track.getX(), track.getRight(), sliderPos);
        const auto thumbY = vertical ? juce::jlimit(track.getY(), track.getBottom(), sliderPos)
                                     : track.getCentreY();
        graphics.setColour(juce::Colours::black.withAlpha(0.32f));
        graphics.fillEllipse(thumbX - thumbRadius + 1.0f, thumbY - thumbRadius + 1.0f,
                             thumbRadius * 2.0f, thumbRadius * 2.0f);
        graphics.setColour(slider.findColour(juce::Slider::thumbColourId)
                               .withAlpha(slider.isEnabled() ? 1.0f : 0.30f));
        graphics.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius,
                             thumbRadius * 2.0f, thumbRadius * 2.0f);
        graphics.setColour(primaryText().withAlpha(slider.isEnabled() ? 0.72f : 0.18f));
        graphics.drawEllipse(thumbX - thumbRadius, thumbY - thumbRadius,
                             thumbRadius * 2.0f, thumbRadius * 2.0f, 1.0f);
    }

    void drawComboBox(juce::Graphics& graphics, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                             static_cast<float>(height)).reduced(0.75f);
        auto fill = box.findColour(juce::ComboBox::backgroundColourId);
        if (isButtonDown) fill = fill.brighter(0.10f);
        if (!box.isEnabled()) fill = fill.withAlpha(0.34f);
        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds, 6.0f);
        graphics.setColour(box.hasKeyboardFocus(true)
                               ? box.findColour(juce::ComboBox::focusedOutlineColourId)
                               : box.findColour(juce::ComboBox::outlineColourId));
        graphics.drawRoundedRectangle(bounds, 6.0f, box.hasKeyboardFocus(true) ? 1.5f : 1.0f);

        const auto arrowArea = juce::Rectangle<float>(static_cast<float>(buttonX),
                                                      static_cast<float>(buttonY),
                                                      static_cast<float>(buttonW),
                                                      static_cast<float>(buttonH)).reduced(7.0f, 9.0f);
        juce::Path arrow;
        arrow.startNewSubPath(arrowArea.getX(), arrowArea.getY());
        arrow.lineTo(arrowArea.getCentreX(), arrowArea.getBottom());
        arrow.lineTo(arrowArea.getRight(), arrowArea.getY());
        graphics.setColour(box.findColour(juce::ComboBox::arrowColourId)
                               .withAlpha(box.isEnabled() ? 0.92f : 0.28f));
        graphics.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(9, 1, std::max(1, box.getWidth() - 30), std::max(1, box.getHeight() - 2));
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override {
        const auto size = juce::jlimit(10.5f, 13.0f, static_cast<float>(box.getHeight()) * 0.36f);
        return juce::Font(juce::FontOptions(size).withStyle("Bold"));
    }
};
