#pragma once

#include <JuceHeader.h>

/** Status of a file-backed resource (NAM capture, impulse response) for the UI. */
enum class LoadState { Empty, Loading, Ready, Failed };

struct FileSlotStatus
{
    LoadState state = LoadState::Empty;
    juce::String path, description, error;
};
