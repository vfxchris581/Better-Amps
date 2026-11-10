#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "vf/dsp/CabFaces.h"

/** The six-face cabinet view: the cabinet as a cube you can orbit (drag empty space) with
    its mics on the faces. Drag a mic to another face to move it; on the front and back the
    mic's speaker, cone position and face follow where it is dropped. Sides, top and bottom
    take the face only for now. */
class CabCubeView : public juce::Component, private juce::Timer
{
public:
    CabCubeView (ViridianForgeProcessor& processor, int cabIndex);
    ~CabCubeView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct MicIds { const char* face; const char* speaker; const char* pos; const char* dist; const char* on; const char* u; const char* v; };
    static const MicIds& idsFor (int mic);

    void timerCallback() override;
    juce::Point<float> centreOffset (juce::Point<float> p) const;   // relative to the cube centre
    float cubeScale() const;
    juce::Point<float> project (vf::dsp::cabfaces::Vec3 p, float& depth) const;
    juce::Point<float> micScreenPos (int mic, float& depth) const;
    void dropMic (int mic, juce::Point<float> at);
    float value (const char* id) const;
    void setValue (const char* id, float plain);

    ViridianForgeProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    int index;
    vf::dsp::cabfaces::View view;
    int dragMic = -1;
    bool orbiting = false;
    juce::Point<float> lastMouse;
    juce::Point<float> dragPoint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabCubeView)
};
