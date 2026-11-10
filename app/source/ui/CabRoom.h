#pragma once

#include <JuceHeader.h>
#include "Widgets.h"
#include "CabCube.h"
#include "vf/dsp/CabMics.h"
#include "../PluginProcessor.h"

/** The big cabinet editor shown when a cab block is selected: place up to two
    mics with the mouse.

    Front view: drag a mic over any speaker - across the cone from the dust cap
    (bright) to the edge (dark); the wheel moves it closer/further. Side view:
    drag left/right for distance from the grille, up/down to angle it off-axis.
    Below: each mic's type, level, phase, Time Align, room mic level/distance,
    and the resulting frequency response, updated live. */
class CabRoomView : public juce::Component, private juce::Timer
{
public:
    CabRoomView (ViridianForgeProcessor& processor, int cabIndex);
    ~CabRoomView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Show the front or the back of the cab. */
    void setView (bool back);

    /** Mic currently being edited (0 = A, 1 = B). */
    int getSelectedMic() const noexcept { return selectedMic; }

private:
    struct MicState { int speaker; float position, distance, angle, theta; bool on; int type; bool back; int face; };

    void timerCallback() override;
    juce::String pid (const char* id) const;
    float value (const char* id) const;
    void setValue (const char* id, float plainValue);
    MicState mic (int m) const;
    vf::dsp::CabModel currentCab() const;
    bool builtInSource() const;

    // geometry
    float pxPerCm() const;
    juce::Point<float> baffleToScreen (float xCm, float yCm) const;
    juce::Point<float> screenToBaffle (juce::Point<float> p) const;
    juce::Point<float> micFrontPos (int m) const;
    juce::Point<float> micSidePos (int m) const;
    int micAtFront (juce::Point<float> p) const;
    int micAtSide (juce::Point<float> p) const;
    void placeMicFront (int m, juce::Point<float> screen);
    void placeMicSide (int m, juce::Point<float> screen);

    void drawFront (juce::Graphics&, const VFTheme&);
    void drawRear (juce::Graphics&, const VFTheme&, juce::Rectangle<float> cabinet, juce::Rectangle<float> baffle);
    void drawSide (juce::Graphics&, const VFTheme&);
    void drawResponse (juce::Graphics&, const VFTheme&);
    void drawMicTop (juce::Graphics&, juce::Point<float> c, float size, juce::Colour colour, const juce::String& label, bool selected, bool on, float tiltDeg, float thetaRad);

    ViridianForgeProcessor& processor;
    APVTS& state;
    int index;
    juce::String thetaKey[2];

    juce::Rectangle<float> frontArea, sideArea, responseArea;
    int selectedMic = 0, dragMic = -1;
    enum class DragKind { None, Front, Side } dragKind = DragKind::None;

    // controls
    juce::Label micALabel, micBLabel, micCLabel, micDLabel, roomLabel;
    ChoiceBox typeA, typeB, faceA, faceB, environment;
    ChoiceBox typeC, typeD, faceC, faceD;
    LabeledKnob levelA, levelB, levelC, levelD, roomKnob, roomDistKnob;
    LedSwitch micBOnSwitch, phaseB, micCOnSwitch, micDOnSwitch, phaseC, phaseD, align, stereo;
    juce::TextButton frontButton { "FRONT" }, backButton { "BACK" }, cubeButton { "3D" };
    std::unique_ptr<CabCubeView> cube;   // the six-face view, shown in the front area when 3D is on
    bool cubeMode = false;
    bool viewBack = false;      // which side of the cab the front view is showing

    // live response curve (message-thread mirror of the audio DSP)
    vf::dsp::CabMics preview;
    std::vector<float> curveA, curveB, curveMix;
    juce::String lastSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabRoomView)
};
