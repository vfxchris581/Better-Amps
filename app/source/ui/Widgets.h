#pragma once

#include <JuceHeader.h>
#include "ViridianLookAndFeel.h"
#include "vf/dsp/Tuner.h"

using APVTS = juce::AudioProcessorValueTreeState;

/** Rotary knob with a label underneath, bound to one parameter. */
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Slider slider;
    juce::Label caption;
    APVTS::SliderAttachment attachment;
};

/** LED toggle bound to a bool parameter. */
class LedSwitch : public juce::Component
{
public:
    LedSwitch (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton& getButton() noexcept { return button; }

private:
    juce::ToggleButton button;
    APVTS::ButtonAttachment attachment;
};

/** Labelled combo box bound to a choice parameter. */
class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;

private:
    juce::ComboBox box;
    juce::Label caption;
    APVTS::ComboBoxAttachment attachment;
};

/** Latching buttons with LEDs bound to a choice parameter (channel, amp source, cab source). */
class ChannelSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChannelSelector (APVTS& state, const juce::String& paramId);
    /** Relabel for the current amp model; channels beyond names.size() are disabled. */
    void setChannelNames (const juce::StringArray& names);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void select (int index);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int current = 0, available = 3;
};

/** Amp model picker grouped by category. Uses its own attachment keyed by
    model ID (JUCE's ComboBoxAttachment maps by list position, which breaks
    once items are grouped under headings). */
class AmpModelSelector : public juce::Component
{
public:
    AmpModelSelector (APVTS& state, const juce::String& paramId);
    void resized() override;
    std::function<void (int)> onModelChanged;
    std::function<void (int)> onUserPick;   // only when the player picks a model, not on load
    int getModel() const noexcept { return current; }

private:
    juce::ComboBox box;
    juce::Label caption;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int current = 0;
};

/** Cabinet picker grouped by size (4x12, 2x12, 1x12 / 4x10), keyed by model ID. */
class CabModelSelector : public juce::Component
{
public:
    CabModelSelector (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;

private:
    juce::ComboBox box;
    juce::Label caption;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Vertical fader with a label (graphic EQ band). */
class LabeledFader : public juce::Component
{
public:
    LabeledFader (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;

private:
    juce::Slider slider;
    juce::Label caption;
    APVTS::SliderAttachment attachment;
};

/** A file slot for a NAM capture or an impulse response: status line plus a LOAD
    menu (library files, import, browse) and a clear button. Status is polled. */
class FileSlot : public juce::Component, private juce::Timer
{
public:
    struct Status { int state = 0; juce::String text, error; }; // 0 empty, 1 loading, 2 ready, 3 failed

    FileSlot (juce::String title, juce::String libraryKind, juce::String wildcard);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<Status()> getStatus;
    std::function<void (const juce::File&)> onLoad;   // file chosen (already copied into the library when imported)
    std::function<void()> onClear;
    std::function<juce::File()> getLibraryFolder;
    std::function<juce::File (const juce::File&)> importFile;

private:
    void timerCallback() override;
    void showMenu();
    juce::String title, kind, wildcard;
    juce::TextButton loadButton { "LOAD" }, clearButton { "X" };
    std::unique_ptr<juce::FileChooser> chooser;
    Status shown;
};

/** Segmented peak meter with peak hold and clip indicator. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String labelText) : label (std::move (labelText)) {}
    void pushPeak (float linearPeak);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clipped = false; repaint(); }

private:
    juce::String label;
    float levelDb = -96.0f, holdDb = -96.0f;
    int holdFrames = 0;
    bool clipped = false;
};

/** Chromatic tuner readout: note name + cents needle + in-tune LED. */
class TunerDisplay : public juce::Component
{
public:
    void setReading (const vf::dsp::TunerReading& r);
    void paint (juce::Graphics&) override;

private:
    vf::dsp::TunerReading reading;
    float smoothedCents = 0.0f;
    int framesSinceValid = 1000;
};

/** Small text readout used for CPU load and latency. */
class StatReadout : public juce::Component
{
public:
    explicit StatReadout (juce::String title) : titleText (std::move (title)) {}
    void setValueText (const juce::String& v, juce::Colour c);
    void paint (juce::Graphics&) override;

private:
    juce::String titleText, valueText { "--" };
    juce::Colour valueColour { juce::Colours::white };
};
