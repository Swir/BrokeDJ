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
        setColour(juce::Slider::textBoxBackgroundColourId, background().brighter(0.035f));
        setColour(juce::Slider::textBoxTextColourId, primaryText());
        setColour(juce::Slider::textBoxOutlineColourId, outline().withAlpha(0.90f));
        setColour(juce::Slider::textBoxHighlightColourId, accentBlue().withAlpha(0.62f));

        setColour(juce::ComboBox::backgroundColourId, surfaceRaised());
        setColour(juce::ComboBox::textColourId, primaryText());
        setColour(juce::ComboBox::outlineColourId, outline());
        setColour(juce::ComboBox::arrowColourId, cyan());
        setColour(juce::ComboBox::focusedOutlineColourId, cyan().withAlpha(0.90f));

        setColour(juce::PopupMenu::backgroundColourId, surface());
        setColour(juce::PopupMenu::textColourId, primaryText());
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentDeep());
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, cyan());

        setColour(juce::TextEditor::backgroundColourId, background().brighter(0.035f));
        setColour(juce::TextEditor::textColourId, primaryText());
        setColour(juce::TextEditor::highlightColourId, accentBlue().withAlpha(0.62f));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId, outline());
        setColour(juce::TextEditor::focusedOutlineColourId, cyan().withAlpha(0.90f));
        setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

        setColour(juce::TooltipWindow::backgroundColourId, surfaceRaised());
        setColour(juce::TooltipWindow::textColourId, primaryText());
        setColour(juce::TooltipWindow::outlineColourId, cyan().withAlpha(0.38f));

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
        const auto size = juce::jlimit(11.0f, 14.25f, static_cast<float>(buttonHeight) * 0.36f);
        return juce::Font(juce::FontOptions(size).withStyle("Bold")).withExtraKerningFactor(0.018f);
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

        auto fill = active ? onColour.interpolatedWith(surfaceRaised(), 0.20f) : offColour;
        if (down && enabled) fill = fill.brighter(0.10f);
        else if (highlighted && enabled) fill = fill.brighter(0.055f);

        const float alpha = enabled ? 1.0f : 0.46f;
        fill = fill.withMultipliedAlpha(alpha);

        graphics.setColour(juce::Colours::black.withAlpha(enabled ? 0.30f : 0.16f));
        graphics.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 4.5f);

        juce::ColourGradient face(fill.brighter(0.035f), bounds.getTopLeft(),
                                  fill.darker(active ? 0.08f : 0.05f), bounds.getBottomLeft(), false);
        graphics.setGradientFill(face);
        graphics.fillRoundedRectangle(bounds, 4.5f);

        const auto border = active
            ? cyan().interpolatedWith(onColour, 0.24f).withAlpha(0.94f * alpha)
            : (highlighted ? cyan().withAlpha(0.42f * alpha)
                           : outline().withAlpha(0.96f * alpha));
        graphics.setColour(border);
        graphics.drawRoundedRectangle(bounds, 4.5f, active ? 1.4f : 1.0f);

        // Thin state rail: easy to scan in a dense deck without turning every
        // active control into a large neon block.
        if (enabled && (active || highlighted)) {
            auto rail = bounds.reduced(5.0f, 0.0f);
            rail.setY(bounds.getBottom() - (active ? 2.3f : 1.7f));
            rail.setHeight(active ? 1.5f : 0.9f);
            graphics.setColour((active ? cyan() : accentBlue()).withAlpha(active ? 0.94f : 0.48f));
            graphics.fillRoundedRectangle(rail, 0.8f);
        }

        if (button.hasKeyboardFocus(true) && enabled) {
            graphics.setColour(cyan().withAlpha(0.80f));
            graphics.drawRoundedRectangle(bounds.reduced(2.0f), 3.2f, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& graphics, juce::TextButton& button,
                        bool highlighted, bool) override {
        graphics.setFont(getTextButtonFont(button, button.getHeight()));
        auto colour = button.findColour(button.getToggleState()
                                            ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId);
        if (!button.isEnabled()) colour = mutedText().withAlpha(0.60f);
        else if (highlighted) colour = colour.brighter(0.06f);
        graphics.setColour(colour);
        graphics.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                                juce::Justification::centred, 1, 0.60f);
    }

    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height))
                          .reduced(3.0f);
        if (bounds.getWidth() <= 8.0f || bounds.getHeight() <= 8.0f) return;

        // JUCE already excludes the slider text-box from these bounds. The old
        // skin subtracted the text-box a second time, which made mixer knobs tiny.
        const auto radius = std::max(5.0f, std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f);
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        const auto angle = startAngle + sliderPos * (endAngle - startAngle);
        const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.42f;

        // Sparse position ticks live outside the value arc and remain readable at
        // compact deck sizes.
        constexpr int tickCount = 9;
        for (int tick = 0; tick < tickCount; ++tick) {
            const auto proportion = static_cast<float>(tick) / static_cast<float>(tickCount - 1);
            const auto tickAngle = startAngle + proportion * (endAngle - startAngle);
            const auto outer = radius + 0.5f;
            const auto inner = radius - (tick == tickCount / 2 ? 3.6f : 2.3f);
            const auto sx = std::sin(tickAngle);
            const auto cy = std::cos(tickAngle);
            graphics.setColour((tick == tickCount / 2 ? mutedText() : outline())
                                   .withAlpha((tick == tickCount / 2 ? 0.66f : 0.46f) * enabledAlpha));
            graphics.drawLine(centreX + inner * sx, centreY - inner * cy,
                              centreX + outer * sx, centreY - outer * cy,
                              tick == tickCount / 2 ? 1.25f : 0.9f);
        }

        const auto arcRadius = std::max(2.0f, radius - 5.0f);
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                                    0.0f, startAngle, endAngle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId)
                               .withAlpha(0.92f * enabledAlpha));
        graphics.strokePath(backgroundArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, startAngle, angle, true);
        graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
                               .withAlpha(0.98f * enabledAlpha));
        graphics.strokePath(valueArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

        const auto knobRadius = std::max(4.5f, radius * 0.58f);
        const auto knobBounds = juce::Rectangle<float>(centreX - knobRadius, centreY - knobRadius,
                                                       knobRadius * 2.0f, knobRadius * 2.0f);
        graphics.setColour(juce::Colours::black.withAlpha(0.40f * enabledAlpha));
        graphics.fillEllipse(knobBounds.translated(0.0f, 1.2f));
        juce::ColourGradient knobGradient(surfaceHighlight().brighter(0.025f), knobBounds.getTopLeft(),
                                          surfaceRaised().darker(0.28f), knobBounds.getBottomLeft(), false);
        graphics.setGradientFill(knobGradient);
        graphics.fillEllipse(knobBounds);
        graphics.setColour(outline().brighter(0.15f).withAlpha(0.95f * enabledAlpha));
        graphics.drawEllipse(knobBounds, 1.0f);

        const auto pointerLength = knobRadius * 0.72f;
        const auto pointerStart = knobRadius * 0.18f;
        const auto sinAngle = std::sin(angle);
        const auto cosAngle = std::cos(angle);
        const auto pointer = slider.findColour(juce::Slider::thumbColourId).withAlpha(enabledAlpha);
        graphics.setColour(pointer.withAlpha(0.96f * enabledAlpha));
        graphics.drawLine(centreX + pointerStart * sinAngle,
                          centreY - pointerStart * cosAngle,
                          centreX + pointerLength * sinAngle,
                          centreY - pointerLength * cosAngle, 2.2f);
        graphics.setColour(pointer.withAlpha(0.84f * enabledAlpha));
        graphics.fillEllipse(centreX - 1.6f, centreY - 1.6f, 3.2f, 3.2f);
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
        if (area.getWidth() <= 2.0f || area.getHeight() <= 2.0f) return;

        const auto enabledAlpha = slider.isEnabled() ? 1.0f : 0.42f;
        const auto trackThickness = vertical
            ? std::max(3.0f, std::min(5.0f, area.getWidth() * 0.11f))
            : std::max(3.0f, std::min(5.0f, area.getHeight() * 0.16f));

        juce::Rectangle<float> track;
        if (vertical) {
            track = {area.getCentreX() - trackThickness * 0.5f, area.getY() + 7.0f,
                     trackThickness, std::max(1.0f, area.getHeight() - 14.0f)};
        } else {
            track = {area.getX() + 7.0f, area.getCentreY() - trackThickness * 0.5f,
                     std::max(1.0f, area.getWidth() - 14.0f), trackThickness};
        }

        graphics.setColour(juce::Colours::black.withAlpha(0.50f));
        graphics.fillRoundedRectangle(track.expanded(1.3f), trackThickness * 0.55f);
        graphics.setColour(outline().withAlpha(0.95f * enabledAlpha));
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

        const auto valueColour = slider.findColour(juce::Slider::trackColourId)
                                     .withAlpha(0.98f * enabledAlpha);
        graphics.setColour(valueColour);
        graphics.fillRoundedRectangle(valueTrack, trackThickness * 0.5f);

        const auto thumbX = vertical ? track.getCentreX()
                                     : juce::jlimit(track.getX(), track.getRight(), sliderPos);
        const auto thumbY = vertical ? juce::jlimit(track.getY(), track.getBottom(), sliderPos)
                                     : track.getCentreY();
        const auto thumbColour = slider.findColour(juce::Slider::thumbColourId)
                                     .withAlpha(enabledAlpha);

        if (vertical) {
            auto thumb = juce::Rectangle<float>(thumbX - 9.0f, thumbY - 4.5f, 18.0f, 9.0f);
            graphics.setColour(juce::Colours::black.withAlpha(0.45f));
            graphics.fillRoundedRectangle(thumb.translated(0.0f, 1.2f), 2.8f);
            juce::ColourGradient cap(surfaceHighlight().brighter(0.04f), thumb.getTopLeft(),
                                     surfaceRaised().darker(0.14f), thumb.getBottomLeft(), false);
            graphics.setGradientFill(cap);
            graphics.fillRoundedRectangle(thumb, 2.8f);
            graphics.setColour(thumbColour);
            graphics.drawRoundedRectangle(thumb, 2.8f, 1.25f);
            graphics.drawLine(thumb.getX() + 3.0f, thumb.getCentreY(),
                              thumb.getRight() - 3.0f, thumb.getCentreY(), 1.0f);
        } else {
            auto thumb = juce::Rectangle<float>(thumbX - 4.5f, thumbY - 9.0f, 9.0f, 18.0f);
            graphics.setColour(juce::Colours::black.withAlpha(0.45f));
            graphics.fillRoundedRectangle(thumb.translated(0.0f, 1.2f), 2.8f);
            juce::ColourGradient cap(surfaceHighlight().brighter(0.04f), thumb.getTopLeft(),
                                     surfaceRaised().darker(0.14f), thumb.getBottomLeft(), false);
            graphics.setGradientFill(cap);
            graphics.fillRoundedRectangle(thumb, 2.8f);
            graphics.setColour(thumbColour);
            graphics.drawRoundedRectangle(thumb, 2.8f, 1.25f);
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
        if (isButtonDown) fill = fill.brighter(0.075f);
        if (!box.isEnabled()) fill = fill.withAlpha(0.48f);

        graphics.setColour(juce::Colours::black.withAlpha(0.32f));
        graphics.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 4.5f);
        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds, 4.5f);

        const auto focused = box.hasKeyboardFocus(true);
        graphics.setColour(focused ? box.findColour(juce::ComboBox::focusedOutlineColourId)
                                   : box.findColour(juce::ComboBox::outlineColourId));
        graphics.drawRoundedRectangle(bounds, 4.5f, focused ? 1.4f : 1.0f);

        auto arrowArea = juce::Rectangle<float>(static_cast<float>(buttonX),
                                                static_cast<float>(buttonY),
                                                static_cast<float>(buttonW),
                                                static_cast<float>(buttonH)).reduced(7.0f, 8.0f);
        if (arrowArea.getWidth() > 2.0f && arrowArea.getHeight() > 2.0f) {
            juce::Path arrow;
            arrow.startNewSubPath(arrowArea.getX(), arrowArea.getY());
            arrow.lineTo(arrowArea.getCentreX(), arrowArea.getBottom());
            arrow.lineTo(arrowArea.getRight(), arrowArea.getY());
            graphics.setColour(box.findColour(juce::ComboBox::arrowColourId)
                                   .withAlpha(box.isEnabled() ? 0.94f : 0.36f));
            graphics.strokePath(arrow, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        }
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(9, 1, std::max(1, box.getWidth() - 30), std::max(1, box.getHeight() - 2));
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override {
        const auto size = juce::jlimit(10.75f, 13.25f, static_cast<float>(box.getHeight()) * 0.37f);
        return juce::Font(juce::FontOptions(size).withStyle("Bold")).withExtraKerningFactor(0.012f);
    }
};