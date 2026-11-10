#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Panels.h"
#include "ui/Pedalboard.h"
#include "ui/CabRoom.h"
#include "ui/ViridianLookAndFeel.h"

class ViridianForgeEditor : public juce::AudioProcessorEditor,
                            private juce::Timer,
                            private juce::Value::Listener
{
public:
    explicit ViridianForgeEditor (ViridianForgeProcessor&);
    ~ViridianForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1280, baseHeight = 800;

    /** Select a chain block for editing (as a click on the pedalboard would). */
    void selectBlock (int key) { pedalboard.select (key); }
    Pedalboard& getPedalboard() noexcept { return pedalboard; }
    void showCabRear (bool rear) { cabRoom1.setView (rear); cabRoom2.setView (rear); }

private:
    void timerCallback() override;
    void valueChanged (juce::Value&) override;
    void applyTheme();
    void updateLiveInputButton();
    bool isStandalone() const;
    void showBlock (int key);   // put the selected block's editor in the main area

    ViridianForgeProcessor& processor;
    ViridianLookAndFeel laf;
    juce::TooltipWindow tooltips { this, 600 };

    TopBar topBar;
    IOPanel ioPanel;
    AmpHeadPanel ampHead1, ampHead2;
    CabPanel cabPanel1, cabPanel2;
    EffectEditor effectEditor;
    CabRoomView cabRoom1, cabRoom2;
    Pedalboard pedalboard;
    AboutOverlay about;
    std::unique_ptr<SplashOverlay> splash;

    juce::Value themeValue;
    int tunerDivider = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViridianForgeEditor)
};
