#include "Widgets.h"
#include "vf/models/AmpModels.h"
#include "vf/dsp/Cabinet.h"

using namespace juce;

static ViridianLookAndFeel* vfLaf (Component& c)
{
    return dynamic_cast<ViridianLookAndFeel*> (&c.getLookAndFeel());
}

static VFTheme themeOf (Component& c)
{
    if (auto* l = vfLaf (c)) return l->getTheme();
    return VFTheme::dark();
}

//==============================================================================
LabeledKnob::LabeledKnob (APVTS& state, const String& paramId, const String& label)
    : attachment (state, paramId, slider)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.setPopupDisplayEnabled (true, true, this);
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (paramId)->convertFrom0to1 (
                                                state.getParameter (paramId)->getDefaultValue()));
    slider.setTooltip (label + " (double-click to reset)");
    addAndMakeVisible (slider);

    caption.setText (label.toUpperCase(), dontSendNotification);
    caption.setJustificationType (Justification::centred);
    addAndMakeVisible (caption);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    const int labelH = jlimit (12, 22, r.getHeight() / 5);
    caption.setBounds (r.removeFromBottom (labelH));
    slider.setBounds (r);
}

//==============================================================================
LedSwitch::LedSwitch (APVTS& state, const String& paramId, const String& label)
    : attachment (state, paramId, button)
{
    button.setButtonText (label);
    addAndMakeVisible (button);
}

//==============================================================================
ChoiceBox::ChoiceBox (APVTS& state, const String& paramId, const String& label)
    : attachment (state, paramId, [&]() -> ComboBox&
      {
          if (auto* p = dynamic_cast<AudioParameterChoice*> (state.getParameter (paramId)))
              box.addItemList (p->choices, 1);
          return box;
      }())
{
    addAndMakeVisible (box);
    caption.setText (label.toUpperCase(), dontSendNotification);
    caption.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (caption);
}

void ChoiceBox::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (r.getHeight() / 3));
    box.setBounds (r);
}

//==============================================================================
ChannelSelector::ChannelSelector (APVTS& state, const String& paramId)
{
    auto* param = state.getParameter (paramId);
    auto* choice = dynamic_cast<AudioParameterChoice*> (param);
    jassert (choice != nullptr);

    for (int i = 0; i < choice->choices.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (choice->choices[i].toUpperCase()));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { select (i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float denormValue)
    {
        current = roundToInt (denormValue);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == current, dontSendNotification);
        repaint();
    });
    attachment->sendInitialUpdate();
}

void ChannelSelector::select (int index)
{
    if (index < available)
        attachment->setValueAsCompleteGesture ((float) index);
}

void ChannelSelector::setChannelNames (const StringArray& names)
{
    available = jlimit (1, buttons.size(), names.size());
    for (int i = 0; i < buttons.size(); ++i)
    {
        const bool exists = i < available;
        buttons[i]->setButtonText (exists ? names[i].toUpperCase() : String ("-"));
        buttons[i]->setEnabled (exists);
        buttons[i]->setAlpha (exists ? 1.0f : 0.35f);
    }
    repaint();
}

//==============================================================================
AmpModelSelector::AmpModelSelector (APVTS& state, const String& paramId)
{
    using namespace vf::models;
    for (int cat = 0; cat < static_cast<int> (AmpCategory::count); ++cat)
    {
        bool headingAdded = false;
        for (int m = 0; m < numAmpModels(); ++m)
        {
            const auto& spec = getAmpModel (m);
            if (static_cast<int> (spec.category) != cat) continue;
            if (! headingAdded) { box.addSectionHeading (categoryName (spec.category)); headingAdded = true; }
            box.addItem (spec.name, m + 1);
        }
    }
    box.setTooltip ("Amp model");
    addAndMakeVisible (box);
    caption.setText ("AMP", dontSendNotification);
    caption.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (caption);

    attachment = std::make_unique<ParameterAttachment> (*state.getParameter (paramId), [this] (float v)
    {
        current = roundToInt (v);
        box.setSelectedId (current + 1, dontSendNotification);
        box.setTooltip (String ("Inspired by ") + vf::models::getAmpModel (current).inspiredBy
                        + ". Names are trademarks of their owners; no affiliation implied.");
        if (onModelChanged) onModelChanged (current);
    });
    box.onChange = [this]
    {
        if (box.getSelectedId() <= 0) return;
        attachment->setValueAsCompleteGesture ((float) (box.getSelectedId() - 1));
        if (onUserPick) onUserPick (box.getSelectedId() - 1);
    };
    attachment->sendInitialUpdate();
}

CabModelSelector::CabModelSelector (APVTS& state, const String& paramId, const String& label)
{
    using namespace vf::dsp;
    const int n = static_cast<int> (CabModel::count);
    auto group = [&] (const char* heading, auto&& belongs)
    {
        box.addSectionHeading (heading);
        for (int m = 0; m < n; ++m)
            if (belongs (getCabinetSpec (static_cast<CabModel> (m))))
                box.addItem (getCabinetSpec (static_cast<CabModel> (m)).name, m + 1);
    };
    group ("4x12", [] (const CabinetSpec& c) { return c.speakers == 4 && c.inches == 12; });
    group ("2x12", [] (const CabinetSpec& c) { return c.speakers == 2; });
    group ("1x12 / 4x10", [] (const CabinetSpec& c) { return c.speakers == 1 || c.inches == 10; });
    addAndMakeVisible (box);
    caption.setText (label.toUpperCase(), dontSendNotification);
    caption.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (caption);

    attachment = std::make_unique<ParameterAttachment> (*state.getParameter (paramId), [this] (float v)
    {
        const int m = jlimit (0, static_cast<int> (vf::dsp::CabModel::count) - 1, roundToInt (v));
        box.setSelectedId (m + 1, dontSendNotification);
        const auto& c = vf::dsp::getCabinetSpec (static_cast<vf::dsp::CabModel> (m));
        box.setTooltip (String ("Inspired by ") + c.inspiredBy + " - " + c.speaker
                        + ". Names are trademarks of their owners; no affiliation implied.");
    });
    box.onChange = [this] { if (box.getSelectedId() > 0) attachment->setValueAsCompleteGesture ((float) (box.getSelectedId() - 1)); };
    attachment->sendInitialUpdate();
}

void CabModelSelector::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (r.getHeight() / 3));
    box.setBounds (r);
}

void AmpModelSelector::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (r.getHeight() / 3));
    box.setBounds (r);
}

void ChannelSelector::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    for (int i = 0; i < buttons.size(); ++i)
    {
        const auto b = buttons[i]->getBounds().toFloat();
        const float d = jmin (10.0f, b.getHeight() * 0.3f);
        ViridianLookAndFeel::drawLed (g, Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() - d }), t.led, i == current);
    }
}

void ChannelSelector::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (r.getHeight() / 3); // room for LEDs
    const int w = r.getWidth() / jmax (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w).reduced (3, 0));
}

//==============================================================================
void LevelMeter::pushPeak (float linearPeak)
{
    const float db = Decibels::gainToDecibels (linearPeak, -96.0f);
    levelDb = db > levelDb ? db : jmax (-96.0f, levelDb - 1.5f); // ~45 dB/s fall at 30 fps
    if (db >= holdDb) { holdDb = db; holdFrames = 45; }
    else if (--holdFrames < 0) holdDb = jmax (-96.0f, holdDb - 1.0f);
    if (linearPeak >= 0.999f) clipped = true;
    repaint();
}

void LevelMeter::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText (label, r.removeFromBottom (12.0f), Justification::centred);

    auto clipLed = r.removeFromTop (8.0f).withSizeKeepingCentre (8.0f, 6.0f);
    g.setColour (clipped ? Colour (0xffff3b30) : t.panelEdge);
    g.fillRoundedRectangle (clipLed, 1.5f);
    r.removeFromTop (2.0f);

    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 2.0f);

    const auto toY = [&] (float db) { return jmap (jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, r.getBottom(), r.getY()); };
    const int segments = 24;
    const float segH = r.getHeight() / (float) segments;
    for (int i = 0; i < segments; ++i)
    {
        const float segDb = -60.0f + 60.0f * ((float) i + 0.5f) / (float) segments;
        const auto seg = Rectangle<float> (r.getX() + 2.0f, r.getBottom() - segH * (float) (i + 1) + 1.0f, r.getWidth() - 4.0f, segH - 2.0f);
        const Colour c = segDb > -3.0f ? Colour (0xffff453a) : (segDb > -12.0f ? Colour (0xffffb02e) : t.accent);
        g.setColour (segDb <= levelDb ? c : c.withAlpha (0.12f));
        g.fillRect (seg);
    }
    if (holdDb > -60.0f)
    {
        g.setColour (t.text);
        g.fillRect (r.getX() + 2.0f, toY (holdDb), r.getWidth() - 4.0f, 1.5f);
    }
}

//==============================================================================
void TunerDisplay::setReading (const vf::dsp::TunerReading& r)
{
    if (r.valid && r.clarity > 0.75)
    {
        reading = r;
        smoothedCents = framesSinceValid > 5 ? (float) r.cents : smoothedCents * 0.6f + (float) r.cents * 0.4f;
        framesSinceValid = 0;
    }
    else
    {
        ++framesSinceValid;
    }
    repaint();
}

void TunerDisplay::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    r.reduce (8.0f, 4.0f);

    const bool active = framesSinceValid < 12;
    const bool inTune = active && std::abs (smoothedCents) < 3.0f;

    auto noteArea = r.removeFromLeft (r.getHeight() * 1.3f);
    g.setColour (active ? (inTune ? t.accent : Colours::white) : t.textDim.withAlpha (0.5f));
    g.setFont (ViridianLookAndFeel::labelFont (r.getHeight() * 0.72f));
    g.drawText (active ? String (reading.noteName()) : String ("--"), noteArea, Justification::centred);

    auto ledArea = r.removeFromRight (r.getHeight() * 0.5f);
    ViridianLookAndFeel::drawLed (g, ledArea.withSizeKeepingCentre (9.0f, 9.0f), t.led, inTune);

    // Cents scale: -50 .. +50
    auto scale = r.reduced (6.0f, r.getHeight() * 0.18f);
    g.setColour (t.textDim);
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = jmap ((float) c, -50.0f, 50.0f, scale.getX(), scale.getRight());
        const float len = (c == 0) ? scale.getHeight() : scale.getHeight() * 0.45f;
        g.drawLine (x, scale.getBottom() - len, x, scale.getBottom(), c == 0 ? 1.5f : 1.0f);
    }
    if (active)
    {
        const float x = jmap (jlimit (-50.0f, 50.0f, smoothedCents), -50.0f, 50.0f, scale.getX(), scale.getRight());
        g.setColour (inTune ? t.accent : Colour (0xffffb02e));
        g.fillRoundedRectangle (x - 2.0f, scale.getY() - 2.0f, 4.0f, scale.getHeight() + 4.0f, 2.0f);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
        g.setColour (t.textDim);
        g.drawText (String (smoothedCents >= 0 ? "+" : "") + String (smoothedCents, 1) + " ct  " + String (reading.frequency, 1) + " Hz",
                    r.removeFromTop (12.0f), Justification::centredRight);
    }
}

//==============================================================================
void StatReadout::setValueText (const String& v, Colour c)
{
    if (v != valueText || c != valueColour) { valueText = v; valueColour = c; repaint(); }
}

void StatReadout::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText (titleText, r.removeFromTop (r.getHeight() * 0.4f), Justification::centred);
    g.setColour (valueColour);
    g.setFont (ViridianLookAndFeel::labelFont (jmin (16.0f, r.getHeight() * 0.8f)));
    g.drawText (valueText, r, Justification::centred);
}

//==============================================================================
LabeledFader::LabeledFader (APVTS& state, const String& paramId, const String& label)
    : attachment (state, paramId, slider)
{
    slider.setSliderStyle (Slider::LinearVertical);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, this);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setTooltip (label + " (double-click to centre)");
    addAndMakeVisible (slider);
    caption.setText (label, dontSendNotification);
    caption.setJustificationType (Justification::centred);
    addAndMakeVisible (caption);
}

void LabeledFader::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

//==============================================================================
FileSlot::FileSlot (String t, String libraryKind, String wc)
    : title (std::move (t)), kind (std::move (libraryKind)), wildcard (std::move (wc))
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    loadButton.onClick = [this] { showMenu(); };
    clearButton.onClick = [this] { if (onClear) onClear(); };
    clearButton.setTooltip ("Clear");
    startTimerHz (8);
}

void FileSlot::timerCallback()
{
    if (! getStatus) return;
    const auto st = getStatus();
    if (st.state != shown.state || st.text != shown.text || st.error != shown.error)
    {
        shown = st;
        clearButton.setEnabled (shown.state != 0);
        repaint();
    }
}

void FileSlot::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    auto text = getLocalBounds().reduced (6, 2).withTrimmedRight (loadButton.getWidth() + clearButton.getWidth() + 8);
    const Colour led = shown.state == 2 ? t.led : (shown.state == 1 ? Colour (0xffffb02e) : (shown.state == 3 ? Colour (0xffff453a) : t.ledOff));
    ViridianLookAndFeel::drawLed (g, text.removeFromLeft (10).toFloat().withSizeKeepingCentre (7.0f, 7.0f), led, shown.state != 0);
    text.removeFromLeft (4);
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText (title, text.removeFromTop (14), Justification::bottomLeft);
    g.setColour (shown.state == 3 ? Colour (0xffff6b60) : t.text);
    g.setFont (ViridianLookAndFeel::labelFont (11.0f, false));
    const String line = shown.state == 0 ? String ("Empty - click LOAD")
                      : shown.state == 1 ? String ("Loading ") + shown.text + "..."
                      : shown.state == 3 ? String ("Error: ") + shown.error
                                         : shown.text;
    g.drawFittedText (line, text, Justification::topLeft, jmax (1, text.getHeight() / 13), 1.0f);
}

void FileSlot::resized()
{
    auto r = getLocalBounds().reduced (4);
    clearButton.setBounds (r.removeFromRight (26).withSizeKeepingCentre (26, jmin (24, r.getHeight())));
    r.removeFromRight (4);
    loadButton.setBounds (r.removeFromRight (54).withSizeKeepingCentre (54, jmin (24, r.getHeight())));
}

void FileSlot::showMenu()
{
    PopupMenu menu;
    Array<File> files;
    if (getLibraryFolder)
    {
        files = getLibraryFolder().findChildFiles (File::findFiles, false, wildcard);
        files.sort();
        PopupMenu lib;
        for (int i = 0; i < files.size(); ++i) lib.addItem (100 + i, files[i].getFileNameWithoutExtension());
        if (files.isEmpty()) lib.addItem (1, "(library is empty - use Import)", false, false);
        menu.addSubMenu ("Library", lib);
    }
    menu.addItem (2, "Import into library...");
    menu.addItem (3, "Load from disk...");
    menu.addSeparator();
    menu.addItem (4, "Show library folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadButton), [this, files] (int result)
    {
        if (result >= 100 && result - 100 < files.size()) { if (onLoad) onLoad (files[result - 100]); return; }
        if (result == 4 && getLibraryFolder) { getLibraryFolder().revealToUser(); return; }
        if (result != 2 && result != 3) return;
        const bool import = result == 2;
        chooser = std::make_unique<FileChooser> (import ? "Import into the " + kind + " library" : "Load " + title,
                                                 getLibraryFolder ? getLibraryFolder() : File(), wildcard);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, import] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            const auto target = (import && importFile) ? importFile (f) : f;
            if (onLoad) onLoad (target);
        });
    });
}
