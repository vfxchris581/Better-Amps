#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace juce;

static StandalonePluginHolder* standaloneHolder()
{
   #if JucePlugin_Build_Standalone
    return StandalonePluginHolder::getInstance();
   #else
    return nullptr;
   #endif
}

ViridianForgeEditor::ViridianForgeEditor (ViridianForgeProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      ioPanel (p.getState()),
      ampHead1 (p.getState(), p, 0), ampHead2 (p.getState(), p, 1),
      cabPanel1 (p.getState(), p, 0), cabPanel2 (p.getState(), p, 1),
      effectEditor (p),
      cabRoom1 (p, 0), cabRoom2 (p, 1),
      pedalboard (p)
{
    setLookAndFeel (&laf);

    for (auto* c : std::initializer_list<Component*> { &topBar, &ioPanel, &pedalboard })
        addAndMakeVisible (c);
    for (auto* c : std::initializer_list<Component*> { &ampHead1, &ampHead2, &cabPanel1, &cabPanel2, &effectEditor, &cabRoom1, &cabRoom2 })
        addChildComponent (c);
    pedalboard.onSelectionChanged = [this] (int key) { showBlock (key); };
    showBlock (vf::graph::kAmp1);
    cabPanel1.setVisible (true);
    addChildComponent (about);

    // Theme is stored in the session state so it survives reloads.
    themeValue.referTo (processor.getThemeValue());
    themeValue.addListener (this);
    topBar.themeButton.onClick = [this]
    {
        themeValue = (themeValue.toString() == "light") ? "dark" : "light";
    };

    topBar.aboutButton.onClick = [this] { about.setVisible (true); about.toFront (true); };

    // Audio device settings + live-input switch only make sense in the standalone app.
    const bool standalone = isStandalone();
    topBar.settingsButton.setVisible (standalone);
    topBar.liveInputButton.setVisible (standalone);
    if (standalone)
    {
        topBar.settingsButton.onClick = [] { if (auto* h = standaloneHolder()) h->showAudioSettingsDialog(); };
        topBar.liveInputButton.onClick = [this]
        {
            if (auto* h = standaloneHolder())
            {
                auto& mute = h->getMuteInputValue();
                mute = ! static_cast<bool> (mute.getValue());
                updateLiveInputButton();
            }
        };
        updateLiveInputButton();
    }

    // Splash once per app/plugin session.
    // VIRIDIAN_NO_SPLASH=1 skips it (automated UI tests / screenshots).
    static bool splashShown = SystemStats::getEnvironmentVariable ("VIRIDIAN_NO_SPLASH", {}).isNotEmpty();
    if (! splashShown)
    {
        splashShown = true;
        splash = std::make_unique<SplashOverlay>();
        addAndMakeVisible (*splash);
    }

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
        c->setSizeLimits (960, 600, 2560, 1600);
    }
    setSize (baseWidth, baseHeight);

    applyTheme();
    startTimerHz (30);
}

ViridianForgeEditor::~ViridianForgeEditor()
{
    stopTimer();
    themeValue.removeListener (this);
    setLookAndFeel (nullptr);
}

void ViridianForgeEditor::showBlock (int key)
{
    using namespace vf::graph;
    // Main area: the selected amp, effect or cab (mic placement). Right panel: the
    // selected cab, or the last one shown when something else is selected.
    if (isFxKey (key)) effectEditor.setSlot (key);
    effectEditor.setVisible (isFxKey (key));
    ampHead1.setVisible (key == kAmp1);
    ampHead2.setVisible (key == kAmp2);
    cabRoom1.setVisible (key == kCab1);
    cabRoom2.setVisible (key == kCab2);
    if (isCabKey (key))
    {
        cabPanel1.setVisible (key == kCab1);
        cabPanel2.setVisible (key == kCab2);
    }
}

bool ViridianForgeEditor::isStandalone() const
{
    return processor.wrapperType == AudioProcessor::wrapperType_Standalone && standaloneHolder() != nullptr;
}

void ViridianForgeEditor::updateLiveInputButton()
{
    if (auto* h = standaloneHolder())
    {
        const bool muted = static_cast<bool> (h->getMuteInputValue().getValue());
        topBar.liveInputButton.setButtonText (muted ? "INPUT MUTED" : "LIVE INPUT");
        topBar.liveInputButton.setToggleState (! muted, dontSendNotification);
    }
}

void ViridianForgeEditor::valueChanged (Value&) { applyTheme(); }

void ViridianForgeEditor::applyTheme()
{
    const auto theme = VFTheme::byId (themeValue.toString());
    laf.setTheme (theme);
    topBar.themeButton.setButtonText (theme.id == "light" ? "DARK" : "LIGHT");
    sendLookAndFeelChange();
    repaint();
}

void ViridianForgeEditor::paint (Graphics& g)
{
    g.fillAll (laf.getTheme().background);
}

void ViridianForgeEditor::resized()
{
    auto r = getLocalBounds();
    const float s = (float) getWidth() / (float) baseWidth;
    auto px = [s] (int v) { return roundToInt ((float) v * s); };

    topBar.setBounds (r.removeFromTop (px (64)));
    r.reduce (px (12), px (12));

    pedalboard.setBounds (r.removeFromBottom (px (150)));
    r.removeFromBottom (px (12));

    ioPanel.setBounds (r.removeFromLeft (px (130)));
    r.removeFromLeft (px (12));
    const auto cabArea = r.removeFromRight (px (260));
    cabPanel1.setBounds (cabArea);
    cabPanel2.setBounds (cabArea);
    r.removeFromRight (px (12));
    ampHead1.setBounds (r);
    ampHead2.setBounds (r);
    effectEditor.setBounds (r);
    cabRoom1.setBounds (r);
    cabRoom2.setBounds (r);

    about.setBounds (getLocalBounds());
    if (splash != nullptr)
        splash->setBounds (getLocalBounds());
}

void ViridianForgeEditor::timerCallback()
{
    processor.housekeeping(); // frees NAM models retired by the audio thread
    topBar.inMeter.pushPeak (processor.popInputPeak());
    topBar.outMeter.pushPeak (processor.popOutputPeak());

    // Tuner analysis runs here, on the message thread, never on the audio thread.
    if (++tunerDivider >= 2)
    {
        tunerDivider = 0;
        topBar.tuner.setReading (processor.analyseTuner());

        const double load = processor.getCpuLoad() * 100.0;
        const auto& t = laf.getTheme();
        topBar.cpu.setValueText (String (load, 1) + "%", load > 70.0 ? Colour (0xffff453a) : (load > 40.0 ? Colour (0xffffb02e) : t.accent));

        const double sr = processor.getSampleRate();
        const double ms = sr > 0.0 ? 1000.0 * (processor.getLatencySamples() + processor.getBlockSize()) / sr : 0.0;
        topBar.latency.setValueText (String (ms, 1) + " ms", t.text);

        if (isStandalone())
            updateLiveInputButton();
    }

    if (splash != nullptr && ! splash->isVisible())
        splash.reset();
}
