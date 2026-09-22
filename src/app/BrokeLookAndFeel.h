// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

// BrokeDJ's native visual system. Presentation only: no transport ownership,
// file I/O, decoding or realtime work lives here. The palette deliberately
// follows the SWIR dark blue -> cyan family while still honouring per-control
// colour overrides (for example active Hot Cues and recording state).
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
        setColour(juce::Slider::trackColourId, accentBlue().withAlpha(0.90f));
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
        setColour(juce::ComboBox::focusedOutlineColourId, cyan().withAlpha(0.88f));

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
        setColour(juce::TextEditor::focusedOutlineColourId, cyan().withAlpha(0.88f));
        setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

        setColour(juce::TooltipWindow::backgroundColourId, surfaceRaised());
        setColour(juce::TooltipWindow::textColourId, primaryText());
        setColour(juce::TooltipWindow::outlineColourId, cyan().withAlpha(0.42f));

        setColour(juce::AlertWindow::backgroundColourId, surface());
        setColour(juce::AlertWindow::textColourId, primaryText());
        setColour(juce::AlertWindow::outlineColourId, cyan().withAlpha(0.55f));
    }

    static juce::Colour background() noexcept { return juce::Colour(0xff02050a); }
    static juce::Colour surface() noexcept { return juce::Colour(0xff07111c); }
    static juce::Colour surfaceRaised() noexcept { return juce::Colour(0xff0d1b2a); }
    static juce::Colour surfaceHighlight() noexcept { return juce::Colour(0xff14263a); }
    static juce::Colour accentBlue() noexcept { return juce::Colour(0xff0088ff); }
    static juce::Colour cyan() noexcept { return juce::Colour(0xff62e5ff); }
    static juce::Colour primaryText() noexcept { return juce::Colour(0xfff4faff); }
    static juce::Colour mutedText() noexcept { return juce::Colour(0xff8da8b8); }
    static juce::Colour outline() noexcept { return juce::Colour(0xff183451); }
    static juce::Colour accentDeep() noexcept { return juce::Colour(0xff07528d); }
    static juce::Colour recordRed() noexcept { return juce::Colour(0xffff496f); }
    static juce::Colour signalGreen() noexcept { return juce::Colour(0xff49e6a3); }
    static juce::Colour warningAmber() noexcept { return juce::Colour(0xffffb84d); }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        const auto size = juce::jlimit(10.5f, 13.5f, static_cast<float>(buttonHeight) * 0.34f);
        return juce::Font(juce::FontOptions(size).withStyle("Bold")).withExtraKerningFactor(0.025f);
    }

    void drawButtonBackground(juce::Graphics& graphics, juce::Button& button,
                              const juce::Colour& suppliedColour,
                              bool highlighted, bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.75f);
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) return;

        const bool enabled = button.isEnabled();
        const bool active = button.getToggleState();
        const auto offColour = suppliedColour.isTransparent() ? surfaceRaised() : suppliedColour;
        const auto onColour = button.findColour(juce::TextButton::buttonOnColourId);
        auto base = active ? onColour : offColour;

        if (!enabled) base = base.withAlpha(0.34f);
        else if (down) base = base.brighter(0.16f);
        else if (highlighted) base = base.brighter(0.08f);

        // Very small drop shadow + restrained vertical material gradient.
        graphics.setColour(juce::Colours::black.withAlpha(enabled ? 0.34f : 0.18f));
        graphics.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 6.5f);

        const auto top = base.brighter(active ? 0.11f : 0.05f);
        const auto bottom = base.darker(active ? 0.14f : 0.08f);
        juce::ColourGradient gradient(top, bounds.getTopLeft(), bottom, bounds.getBottomLeft(), false);
        graphics.setGradientFill(gradient);
        graphics.fillRoundedRectangle(bounds, 6.5f);

        const auto border = active
            ? cyan().interpolatedWith(onColour, 0.28f).withAlpha(enabled ? 0.92f : 0.28f)
            : outline().withAlpha(enabled ? (highlighted ? 1.0f : 0.86f) : 0.34f);
        graphics.setColour(border);
        graphics.drawRoundedRectangle(bounds, 6.5f, active ? 1.55f : 1.0f);

        // A thin luminous status rail reads like hardware illumination without
        // flooding the entire control with neon.
        if ((active || highlighted) && enabled) {
            auto rail = bounds.reduced(5.0f, 0.0f);
            rail.setY(bounds.getBottom() - (active ? 2.2f : 1.6f));
            rail.setHeight(active ? 1.6f : 1.0f);
            graphics.setColour((active ? cyan() : accentBlue()).withAlpha(active ? 0.92f : 0.46f));
            graphics.fillRoundedRectangle(rail, 0.8f);
        }

        if (button.hasKeyboardFocus(true) && enabled) {
            graphics.setColour(cyan().withAlpha(0.78f));
            graphics.drawRoundedRectangle(bounds.reduced(2.25f), 4.6f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& graphics, juce::TextButton& button,
                        bool highlighted, bool) override {
        auto font = getTextButtonFont(button, button.getHeight());
        graphics.setFont(font);
        auto colour = button.findColour(button.getToggleState()
                                            ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId);
        if (!button.isEnabled()) colour = colour.withAlpha(0.34f);
        else if (highlighted) colour = colour.brighter(0.08f);
        graphics.setColour(colour);
        graphics.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(7, 2),
                                juce::Justification::centred, 1, 0.74f);
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
        const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.30f;

        // Recessed control well.
        graphics.setColour(juce::Colours::black.withAlpha(0.36f));
        graphics.fillEllipse(centreX - radius - 2.0f, centreY - radius - 1.0f,
                             (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);
        graphics.setColour(surface());
        graphics.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Sparse hardware-style tick marks improve position reading without clutter.
        constexpr int tickCount = 11;
        for (int tick = 0; tick < tickCount; ++tick) {
            const auto proportion = static_cast<float>(tick) / static_cast<float>(tickCount - 1);
            const auto tickAngle = startAngle + proportion * (endAngle - startAngle);
            const auto outer = radius + 1.0f;
            const auto inner = radius - (tick == tickCount / 2 ? 4.5f : 3.0f);
            const auto sx = std::sin(tickAngle);
            const auto cy = std::cos(tickAngle);
            graphics.setColour((tick == tickCount / 2 ? mutedText() : outline()).withAlpha(0.54f * enabledAlpha));
            graphics.drawLine(centreX + inner * sx, centreY - inner * cy,
                              centreX + outer * sx, centreY - outer * cy,
                              tick == tickCount / 2 ? 1.4f : 1.0f);
        }

        juce::Path track;
        track.addCentredArc(centreX, centreY, radius - 4.0f, radius - 4.0f,
                            0.0f, startAngle, endAngle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId)
                               .withAlpha(0.92f * enabledAlpha));
        graphics.strokePath(track, juce::PathStrokeType(3.1f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc(centreX, centreY, radius - 4.0f, radius - 4.0f,
                            0.0f, startAngle, angle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
                               .withAlpha(0.96f * enabledAlpha));
        graphics.strokePath(value, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

        const auto knobRadius = radius * 0.57f;
        const auto knobBounds = juce::Rectangle<float>(centreX - knobRadius, centreY - knobRadius,
                                                       knobRadius * 2.0f, knobRadius * 2.0f);
        juce::ColourGradient knobGradient(surfaceHighlight(), knobBounds.getTopLeft(),
                                          surfaceRaised().darker(0.24f), knobBounds.getBottomLeft(), false);
        graphics.setGradientFill(knobGradient);
        graphics.fillEllipse(knobBounds);
        graphics.setColour(outline().brighter(0.12f).withAlpha(enabledAlpha));
        graphics.drawEllipse(knobBounds, 1.0f);

        const auto indicatorLength = knobRadius * 0.72f;
        const auto indicatorInner = knobRadius * 0.18f;
        const auto sinAngle = std::sin(angle);
        const auto cosAngle = std::cos(angle);
        graphics.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(enabledAlpha));
        graphics.drawLine(centreX + indicatorInner * sinAngle,
                          centreY - indicatorInner * cosAngle,
                          centreX + indicatorLength * sinAngle,
                          centreY - indicatorLength * cosAngle, 2.2f);
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
            ? std::max(3.0f, std::min(6.0f, area.getWidth() * 0.14f))
            : std::max(3.0f, std::min(6.0f, area.getHeight() * 0.18f));
        const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.30f;

        juce::Rectangle<float> track;
        if (vertical)
            track = {area.getCentreX() - trackThickness * 0.5f, area.getY() + 6.0f,
                     trackThickness, std::max(1.0f, area.getHeight() - 12.0f)};
        else
            track = {area.getX() + 6.0f, area.getCentreY() - trackThickness * 0.5f,
                     std::max(1.0f, area.getWidth() - 12.0f), trackThickness};

        graphics.setColour(juce::Colours::black.withAlpha(0.42f));
        graphics.fillRoundedRectangle(track.expanded(1.5f), trackThickness * 0.7f);
        graphics.setColour(outline().withAlpha(0.92f * enabledAlpha));
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

        auto trackColour = slider.findColour(juce::Slider::trackColourId)
                               .withAlpha(0.96f * enabledAlpha);
        juce::ColourGradient valueGradient(trackColour.darker(0.22f), valueTrack.getBottomLeft(),
                                           trackColour.brighter(0.22f), valueTrack.getTopRight(), false);
        graphics.setGradientFill(valueGradient);
        graphics.fillRoundedRectangle(valueTrack, trackThickness * 0.5f);

        const auto thumbX = vertical ? track.getCentreX()
                                     : juce::jlimit(track.getX(), track.getRight(), sliderPos);
        const auto thumbY = vertical ? juce::jlimit(track.getY(), track.getBottom(), sliderPos)
                                     : track.getCentreY();
        const auto thumbColour = slider.findColour(juce::Slider::thumbColourId)
                                     .withAlpha(enabledAlpha);

        if (vertical) {
            auto thumb = juce::Rectangle<float>(thumbX - 8.0f, thumbY - 4.0f, 16.0f, 8.0f);
            graphics.setColour(juce::Colours::black.withAlpha(0.40f));
            graphics.fillRoundedRectangle(thumb.translated(0.0f, 1.0f), 3.0f);
            graphics.setColour(surfaceHighlight().withAlpha(enabledAlpha));
            graphics.fillRoundedRectangle(thumb, 3.0f);
            graphics.setColour(thumbColour);
            graphics.drawRoundedRectangle(thumb, 3.0f, 1.2f);
        } else {
            auto thumb = juce::Rectangle<float>(thumbX - 4.0f, thumbY - 8.0f, 8.0f, 16.0f);
            graphics.setColour(juce::Colours::black.withAlpha(0.40f));
            graphics.fillRoundedRectangle(thumb.translated(0.0f, 1.0f), 3.0f);
            graphics.setColour(surfaceHighlight().withAlpha(enabledAlpha));
            graphics.fillRoundedRectangle(thumb, 3.0f);
            graphics.setColour(thumbColour);
            graphics.drawRoundedRectangle(thumb, 3.0f, 1.2f);
            graphics.drawLine(thumb.getCentreX(), thumb.getY() + 3.0f,
                              thumb.getCentreX(), thumb.getBottom() - 3.0f, 1.0f);
        }
    }

    void drawComboBox(juce::Graphics& graphics, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                             static_cast<float>(height)).reduced(0.75f);
        auto fill = box.findColour(juce::ComboBox::backgroundColourId);
        if (isButtonDown) fill = fill.brighter(0.10f);
        if (!box.isEnabled()) fill = fill.withAlpha(0.34f);

        graphics.setColour(juce::Colours::black.withAlpha(0.30f));
        graphics.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 6.0f);
        juce::ColourGradient gradient(fill.brighter(0.04f), bounds.getTopLeft(),
                                      fill.darker(0.08f), bounds.getBottomLeft(), false);
        graphics.setGradientFill(gradient);
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
                               .withAlpha(box.isEnabled() ? 0.96f : 0.28f));
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
        return juce::Font(juce::FontOptions(size).withStyle("Bold")).withExtraKerningFactor(0.02f);
    }
};