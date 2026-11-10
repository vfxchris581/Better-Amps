#pragma once

#include <JuceHeader.h>

/** Colour set for one theme. Every custom component paints from this, so a
    theme switch is just "swap the palette and repaint". */
struct VFTheme
{
    juce::String id;
    juce::Colour background, panel, panelEdge, text, textDim, accent, accentDeep;
    juce::Colour faceplateTop, faceplateBottom, tolex, grille, knobBody, knobPointer, led, ledOff;

    static VFTheme dark();
    static VFTheme light();
    static VFTheme byId (const juce::String& id) { return id == "light" ? light() : dark(); }
};

class ViridianLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ViridianLookAndFeel();

    void setTheme (const VFTheme& t);
    const VFTheme& getTheme() const noexcept { return theme; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    static juce::Font labelFont (float height, bool bold = true);
    static void drawLed (juce::Graphics&, juce::Rectangle<float> area, juce::Colour onColour, bool on);
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius);

private:
    VFTheme theme = VFTheme::dark();
};
