#pragma once
#include "PluginProcessor.h"
class VFXAampsEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit VFXAampsEditor (VFXAampsProcessor& p) : juce::GenericAudioProcessorEditor (p) {}
};
