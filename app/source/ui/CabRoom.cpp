#include "CabRoom.h"
#include "vf/Parameters.h"

using namespace juce;
using namespace vf::dsp;

static VFTheme themeOf (Component& c)
{
    if (auto* l = dynamic_cast<ViridianLookAndFeel*> (&c.getLookAndFeel())) return l->getTheme();
    return VFTheme::dark();
}

static const Colour kMicColour[4] { Colour (0xff3ee07a), Colour (0xffffa12e), Colour (0xffb78cff), Colour (0xff5aa9ff) };

CabRoomView::CabRoomView (ViridianForgeProcessor& p, int cabIndex)
    : processor (p), state (p.getState()), index (cabIndex),
      typeA (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micType, "Mic A Type"),
      typeB (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micBType, "Mic B Type"),
      faceA (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micAFace, "Side"),
      faceB (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micBFace, "Side"),
      environment (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::roomEnv, "Room Environment"),
      levelA (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micALevel, "Level"),
      levelB (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micBLevel, "Level"),
      typeC (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micCType, "Mic C Type"),
      typeD (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micDType, "Mic D Type"),
      faceC (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micCFace, "Side"),
      faceD (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micDFace, "Side"),
      levelC (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micCLevel, "Level"),
      levelD (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micDLevel, "Level"),
      micCOnSwitch (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micCOn, "On"),
      micDOnSwitch (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micDOn, "On"),
      phaseC (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micCPhase, "Phase"),
      phaseD (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micDPhase, "Phase"),
      roomKnob (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::room, "Level"),
      roomDistKnob (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::roomDist, "Dist"),
      micBOnSwitch (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micBOn, "On"),
      phaseB (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micBPhase, "Phase"),
      align (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::micAlign, "Time Align"),
      stereo (p.getState(), ViridianForgeProcessor::cabPrefix (cabIndex) + vf::ParamID::cabStereo, "A L / B R")
{
    const String key = "cab" + String (index + 1) + "_mic";
    thetaKey[0] = key + "A_theta";
    thetaKey[1] = key + "B_theta";

    micALabel.setText ("MIC A", dontSendNotification);
    micBLabel.setText ("MIC B", dontSendNotification);
    micCLabel.setText ("MIC C", dontSendNotification);
    micDLabel.setText ("MIC D", dontSendNotification);
    micCLabel.setColour (Label::textColourId, kMicColour[2]);
    micDLabel.setColour (Label::textColourId, kMicColour[3]);
    roomLabel.setText ("ROOM", dontSendNotification);
    micALabel.setColour (Label::textColourId, kMicColour[0]);
    micBLabel.setColour (Label::textColourId, kMicColour[1]);
    for (auto* c : std::initializer_list<Component*> { &faceA, &faceB, &faceC, &faceD, &environment, &frontButton, &backButton })
        addAndMakeVisible (c);
    frontButton.setTooltip ("Show the front of the cab");
    backButton.setTooltip ("Show the back of the cab: mic the rear of an open-back cab, or through the back panel of a closed one");
    frontButton.onClick = [this] { setView (false); };
    backButton.onClick = [this] { setView (true); };
    cube = std::make_unique<CabCubeView> (p, cabIndex);
    addChildComponent (cube.get());
    addAndMakeVisible (cubeButton);
    cubeButton.setTooltip ("Show the cabinet as a 3D cube: mics on all six faces");
    cubeButton.onClick = [this]
    {
        cubeMode = ! cubeMode;
        cubeButton.setToggleState (cubeMode, dontSendNotification);
        cube->setVisible (cubeMode);
        resized();
        repaint();
    };
    for (auto* c : std::initializer_list<Component*> { &micALabel, &micBLabel, &micCLabel, &micDLabel, &roomLabel, &typeA, &typeB,
                                                       &typeC, &typeD, &levelA, &levelB, &levelC, &levelD,
                                                       &roomKnob, &roomDistKnob, &micBOnSwitch, &phaseB,
                                                       &micCOnSwitch, &micDOnSwitch, &phaseC, &phaseD, &align, &stereo })
        addAndMakeVisible (c);

    align.getButton().setTooltip ("Delay the closer mic so both arrive together: removes the comb filtering of two mics at different distances");
    phaseB.getButton().setTooltip ("Flip mic B's polarity");
    stereo.getButton().setTooltip ("Mic A to the left channel, mic B to the right (otherwise both are mixed)");
    roomDistKnob.getSlider().setTooltip ("How far back the room mics stand: later, sparser, darker reflections");
    setRepaintsOnMouseActivity (false);

    setView (false);
    preview.prepare (48000.0);
    startTimerHz (20);
    timerCallback();
}

CabRoomView::~CabRoomView() { stopTimer(); }

String CabRoomView::pid (const char* id) const { return ViridianForgeProcessor::cabPrefix (index) + id; }

float CabRoomView::value (const char* id) const
{
    if (auto* v = state.getRawParameterValue (pid (id))) return v->load();
    return 0.0f;
}

void CabRoomView::setValue (const char* id, float plain)
{
    if (auto* p = state.getParameter (pid (id)))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

CabModel CabRoomView::currentCab() const
{
    return static_cast<CabModel> (jlimit (0, (int) CabModel::count - 1, roundToInt (value (vf::ParamID::cabModel))));
}

bool CabRoomView::builtInSource() const { return roundToInt (value (vf::ParamID::cabSource)) == 0; }

CabRoomView::MicState CabRoomView::mic (int m) const
{
    using namespace vf::ParamID;
    const auto geo = getCabGeometry (currentCab());
    MicState s {};
    if (m == 0)
        s = { roundToInt (value (micASpeaker)), value (micPosition) / 10.0f, value (micDistance) / 10.0f, value (micAAngle), 0.0f, true, roundToInt (value (micType)), value (micAFace) > 0.5f };
    else
        s = { roundToInt (value (micBSpeaker)), value (micBPos) / 10.0f, value (micBDist) / 10.0f, value (micBAngle), 0.0f, value (micBOn) > 0.5f, roundToInt (value (micBType)), value (micBFace) > 0.5f };
    s.face = jlimit (0, 5, roundToInt (value (m == 0 ? vf::ParamID::micAFace : vf::ParamID::micBFace)));
    s.back = s.face == 1;   // Back; the other faces are handled by the six-face model
    s.speaker = jlimit (0, geo.speakers - 1, s.speaker);
    s.theta = (float) state.state.getProperty (thetaKey[m], m == 0 ? -2.3 : 0.7);
    return s;
}

//==============================================================================
// Geometry: the front view shows the baffle at a uniform cm -> px scale.

float CabRoomView::pxPerCm() const
{
    const auto geo = getCabGeometry (currentCab());
    const float w = (float) geo.columns * geo.spacingCm + 14.0f;
    const float h = (float) (geo.speakers / geo.columns) * geo.spacingCm + 14.0f;
    return jmin (frontArea.getWidth() / jmax (w, 60.0f), (frontArea.getHeight() - 22.0f) / jmax (h, 50.0f)) * 0.86f;
}

void CabRoomView::setView (bool back)
{
    viewBack = back;
    if (mic (selectedMic).back != back && mic (1 - selectedMic).back == back) selectedMic = 1 - selectedMic;
    frontButton.setToggleState (! back, dontSendNotification);
    backButton.setToggleState (back, dontSendNotification);
    repaint();
}

// Seen from behind, the speakers are mirrored left/right.
Point<float> CabRoomView::baffleToScreen (float x, float y) const
{
    const float s = pxPerCm();
    if (viewBack) x = -x;
    return { frontArea.getCentreX() + x * s, frontArea.getCentreY() - 10.0f + y * s };
}

Point<float> CabRoomView::screenToBaffle (Point<float> p) const
{
    const float s = pxPerCm();
    const float x = (p.x - frontArea.getCentreX()) / s;
    return { viewBack ? -x : x, (p.y - frontArea.getCentreY() + 10.0f) / s };
}

Point<float> CabRoomView::micFrontPos (int m) const
{
    const auto geo = getCabGeometry (currentCab());
    const auto st = mic (m);
    float cx, cy;
    geo.centre (st.speaker, cx, cy);
    const float r = st.position * geo.coneRadiusCm;
    return baffleToScreen (cx + r * std::cos (st.theta), cy + r * std::sin (st.theta));
}

// Side view: the cab in the middle (front/grille on the left, back panel on the right);
// front mics stand to its left, rear mics to its right, 0-30 cm away.
struct SideGeometry { Rectangle<float> inner; float cabLeft, cabRight, span, cy, coneR; };

static SideGeometry sideGeometry (Rectangle<float> sideArea)
{
    SideGeometry g;
    g.inner = sideArea.reduced (14.0f, 26.0f);
    const float cabW = jmin (70.0f, g.inner.getWidth() * 0.2f);
    g.cabLeft = g.inner.getCentreX() - cabW * 0.5f;
    g.cabRight = g.inner.getCentreX() + cabW * 0.5f;
    g.span = g.cabLeft - g.inner.getX() - 46.0f;
    g.cy = g.inner.getCentreY();
    g.coneR = g.inner.getHeight() * 0.36f;
    return g;
}

Point<float> CabRoomView::micSidePos (int m) const
{
    const auto st = mic (m);
    const auto sg = sideGeometry (sideArea);
    const float off = 8.0f + st.distance * sg.span;
    const float x = st.back ? sg.cabRight + off : sg.cabLeft - off;
    const float y = sg.cy + st.position * sg.coneR * std::sin (st.theta);
    return { x, y };
}

int CabRoomView::micAtFront (Point<float> p) const
{
    for (int m : { selectedMic, 1 - selectedMic })
        if (mic (m).back == viewBack && micFrontPos (m).getDistanceFrom (p) < 16.0f) return m;
    return -1;
}

int CabRoomView::micAtSide (Point<float> p) const
{
    for (int m : { selectedMic, 1 - selectedMic })
    {
        const auto c = micSidePos (m);
        const auto box = mic (m).back ? Rectangle<float> (c.x - 6.0f, c.y - 14.0f, 64.0f, 28.0f)
                                      : Rectangle<float> (c.x - 58.0f, c.y - 14.0f, 64.0f, 28.0f);
        if (box.contains (p)) return m;
    }
    return -1;
}

void CabRoomView::placeMicFront (int m, Point<float> screen)
{
    const auto geo = getCabGeometry (currentCab());
    const auto b = screenToBaffle (screen);
    int best = 0;
    float bestD = 1.0e9f;
    for (int k = 0; k < geo.speakers; ++k)
    {
        float cx, cy;
        geo.centre (k, cx, cy);
        const float d = std::hypot (b.x - cx, b.y - cy);
        if (d < bestD) { bestD = d; best = k; }
    }
    float cx, cy;
    geo.centre (best, cx, cy);
    const float r = jlimit (0.0f, 1.0f, bestD / geo.coneRadiusCm);
    const float theta = std::atan2 (b.y - cy, b.x - cx);
    using namespace vf::ParamID;
    setValue (m == 0 ? micASpeaker : micBSpeaker, (float) best);
    setValue (m == 0 ? micPosition : micBPos, r * 10.0f);
    if (r > 0.02f) state.state.setProperty (thetaKey[m], theta, nullptr);
    setValue (m == 0 ? micAFace : micBFace, viewBack ? 1.0f : 0.0f); // placed on the side being shown
    if (m == 1 && value (micBOn) < 0.5f) setValue (micBOn, 1.0f); // grabbing mic B puts it up
}

void CabRoomView::placeMicSide (int m, Point<float> screen)
{
    const auto sg = sideGeometry (sideArea);
    const bool back = screen.x > sg.inner.getCentreX();          // drag across the cab to move a mic round the back
    const float off = back ? screen.x - sg.cabRight : sg.cabLeft - screen.x;
    const float d = jlimit (0.0f, 1.0f, (off - 8.0f) / sg.span);
    const float angle = jlimit (0.0f, kMaxMicAngleDeg, std::abs (screen.y - sg.cy) / (sg.inner.getHeight() * 0.5f) * kMaxMicAngleDeg);
    using namespace vf::ParamID;
    setValue (m == 0 ? micDistance : micBDist, d * 10.0f);
    setValue (m == 0 ? micAAngle : micBAngle, angle);
    setValue (m == 0 ? micAFace : micBFace, back ? 1.0f : 0.0f);
    if (back != viewBack) setView (back);
    if (m == 1 && value (micBOn) < 0.5f) setValue (micBOn, 1.0f);
}

//==============================================================================
void CabRoomView::mouseMove (const MouseEvent& e)
{
    const bool overMic = (frontArea.contains (e.position) && micAtFront (e.position) >= 0) || (sideArea.contains (e.position) && micAtSide (e.position) >= 0);
    setMouseCursor (overMic ? MouseCursor::DraggingHandCursor : (frontArea.contains (e.position) ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor));
}

void CabRoomView::mouseDown (const MouseEvent& e)
{
    dragKind = DragKind::None;
    dragMic = -1;
    if (! builtInSource()) return;
    using namespace vf::ParamID;
    if (frontArea.contains (e.position))
    {
        int m = micAtFront (e.position);
        if (m < 0) m = selectedMic;            // click on the baffle: move the selected mic there
        selectedMic = m;
        dragMic = m;
        dragKind = DragKind::Front;
        for (const char* id : { micASpeaker, micPosition, micBSpeaker, micBPos, micBOn, micAFace, micBFace })
            if (auto* p = state.getParameter (pid (id))) p->beginChangeGesture();
        placeMicFront (m, e.position);
    }
    else if (sideArea.contains (e.position))
    {
        int m = micAtSide (e.position);
        if (m < 0) m = selectedMic;
        selectedMic = m;
        dragMic = m;
        dragKind = DragKind::Side;
        for (const char* id : { micDistance, micAAngle, micBDist, micBAngle, micBOn })
            if (auto* p = state.getParameter (pid (id))) p->beginChangeGesture();
        placeMicSide (m, e.position);
    }
    repaint();
}

void CabRoomView::mouseDrag (const MouseEvent& e)
{
    if (dragMic < 0) return;
    if (dragKind == DragKind::Front) placeMicFront (dragMic, e.position);
    else if (dragKind == DragKind::Side) placeMicSide (dragMic, e.position);
    repaint();
}

void CabRoomView::mouseUp (const MouseEvent&)
{
    using namespace vf::ParamID;
    if (dragKind != DragKind::None)
        for (const char* id : { micASpeaker, micPosition, micBSpeaker, micBPos, micBOn, micAFace, micBFace, micDistance, micAAngle, micBDist, micBAngle })
            if (auto* p = state.getParameter (pid (id))) p->endChangeGesture();
    dragKind = DragKind::None;
    dragMic = -1;
}

void CabRoomView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! builtInSource() || ! frontArea.contains (e.position)) return;
    int m = micAtFront (e.position);
    if (m < 0) m = selectedMic;
    selectedMic = m;
    const char* id = m == 0 ? vf::ParamID::micDistance : vf::ParamID::micBDist;
    if (auto* p = state.getParameter (pid (id)))
    {
        p->beginChangeGesture();
        setValue (id, jlimit (0.0f, 10.0f, value (id) - w.deltaY * 2.0f));
        p->endChangeGesture();
    }
    repaint();
}

//==============================================================================
void CabRoomView::timerCallback()
{
    using namespace vf::ParamID;
    String sig;
    for (const char* id : { cabModel, micType, micASpeaker, micPosition, micDistance, micAAngle, micALevel, micBOn, micBSpeaker,
                            micBPos, micBDist, micBAngle, micBType, micBLevel, micBPhase, micAlign, cabSource, room, roomDist,
                            micAFace, micBFace, roomEnv })
        sig << String (value (id), 3) << ";";
    sig << state.state.getProperty (thetaKey[0]).toString() << state.state.getProperty (thetaKey[1]).toString();
    if (sig == lastSignature) return;
    lastSignature = sig;

    // Mirror the audio settings and recompute the response curves.
    auto settingsFor = [this] (int m)
    {
        const auto st = mic (m);
        MicSettings s;
        s.enabled = st.on;
        s.type = static_cast<MicType> (jlimit (0, 2, st.type));
        s.speaker = st.speaker;
        s.position = st.position;
        s.distance = st.distance;
        s.angle = st.angle;
        s.levelDb = value (m == 0 ? micALevel : micBLevel);
        s.invert = m == 1 && value (micBPhase) > 0.5f;
        s.face = st.face;
        return s;
    };
    preview.set (currentCab(), settingsFor (0), settingsFor (1), value (micAlign) > 0.5f);
    const int n = 160;
    curveA.resize ((size_t) n); curveB.resize ((size_t) n); curveMix.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double f = 50.0 * std::pow (12000.0 / 50.0, (double) i / (n - 1));
        auto dB = [] (double m) { return (float) (20.0 * std::log10 (std::max (m, 1.0e-6))); };
        curveA[(size_t) i] = dB (std::abs (preview.responseAt (0, f)));
        curveB[(size_t) i] = dB (std::abs (preview.responseAt (1, f)));
        curveMix[(size_t) i] = dB (std::abs (preview.monoResponseAt (f)));
    }
    repaint();
}

//==============================================================================
void CabRoomView::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromTop (26);
    auto left = r.removeFromLeft (r.getWidth() * 50 / 100);
    r.removeFromLeft (12);
    responseArea = left.removeFromBottom (left.getHeight() * 26 / 100).toFloat();
    left.removeFromBottom (8);
    frontArea = left.toFloat();
    frontButton.setBounds ((int) frontArea.getX() + 4, (int) frontArea.getY() + 2, 58, 22);
    backButton.setBounds ((int) frontArea.getX() + 64, (int) frontArea.getY() + 2, 58, 22);
    cubeButton.setBounds ((int) frontArea.getX() + 124, (int) frontArea.getY() + 2, 46, 22);
    if (cube) cube->setBounds (frontArea.toNearestInt().withTrimmedTop (28));

    sideArea = r.removeFromTop (r.getHeight() * 32 / 100).toFloat();
    r.removeFromTop (6);

    // Controls: mic A, mic B, room, switches
    auto toggles = r.removeFromBottom (34);
    const int rowH = r.getHeight() / 5;
    auto rowA = r.removeFromTop (rowH), rowB = r.removeFromTop (rowH), rowC = r.removeFromTop (rowH), rowD = r.removeFromTop (rowH), rowE = r;
    const int labelW = 46, knobW = jmin (58, r.getWidth() / 7), smallW = 48;
    auto choice = [] (ChoiceBox& c, Rectangle<int> area) { c.setBounds (area.reduced (2, 0).withSizeKeepingCentre (area.getWidth() - 4, jmin (44, area.getHeight()))); };

    micALabel.setBounds (rowA.removeFromLeft (labelW));
    rowA.removeFromLeft (smallW);                       // (mic A is always on)
    choice (faceA, rowA.removeFromLeft (64));
    levelA.setBounds (rowA.removeFromRight (knobW + smallW).removeFromLeft (knobW));
    choice (typeA, rowA);

    micBLabel.setBounds (rowB.removeFromLeft (labelW));
    micBOnSwitch.setBounds (rowB.removeFromLeft (smallW).withSizeKeepingCentre (smallW - 4, 36));
    choice (faceB, rowB.removeFromLeft (64));
    phaseB.setBounds (rowB.removeFromRight (smallW).withSizeKeepingCentre (smallW - 2, 36));
    levelB.setBounds (rowB.removeFromRight (knobW));
    choice (typeB, rowB);

    micCLabel.setBounds (rowC.removeFromLeft (labelW));
    micCOnSwitch.setBounds (rowC.removeFromLeft (smallW).withSizeKeepingCentre (smallW - 4, 36));
    choice (faceC, rowC.removeFromLeft (64));
    phaseC.setBounds (rowC.removeFromRight (smallW).withSizeKeepingCentre (smallW - 2, 36));
    levelC.setBounds (rowC.removeFromRight (knobW));
    choice (typeC, rowC);

    micDLabel.setBounds (rowD.removeFromLeft (labelW));
    micDOnSwitch.setBounds (rowD.removeFromLeft (smallW).withSizeKeepingCentre (smallW - 4, 36));
    choice (faceD, rowD.removeFromLeft (64));
    phaseD.setBounds (rowD.removeFromRight (smallW).withSizeKeepingCentre (smallW - 2, 36));
    levelD.setBounds (rowD.removeFromRight (knobW));
    choice (typeD, rowD);

    roomLabel.setBounds (rowE.removeFromLeft (labelW));
    roomDistKnob.setBounds (rowE.removeFromRight (knobW + 8));
    roomKnob.setBounds (rowE.removeFromRight (knobW));
    choice (environment, rowE);

    const int tw = toggles.getWidth() / 2;
    align.setBounds (toggles.removeFromLeft (tw).withSizeKeepingCentre (jmin (110, tw - 8), 30));
    stereo.setBounds (toggles.withSizeKeepingCentre (jmin (110, tw - 8), 30));
}

void CabRoomView::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    g.setColour (t.panel.darker (0.1f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (11.0f));
    const String title = "CABINET " + String (index + 1) + "  -  " + String (cabModelNames[(size_t) currentCab()]).toUpperCase() + "  -  MIC PLACEMENT";
    g.drawText (title, getLocalBounds().reduced (14, 10).removeFromTop (20), Justification::centredLeft);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
    g.drawText ("drag mics | wheel: distance | side view: across = distance & front/back, up/down = angle",
                getLocalBounds().reduced (14, 10).removeFromTop (20), Justification::centredRight);

    drawFront (g, t);
    drawSide (g, t);
    drawResponse (g, t);

    if (! builtInSource())
    {
        auto a = frontArea.getUnion (sideArea).getUnion (responseArea).expanded (6.0f);
        g.setColour (t.background.withAlpha (0.78f));
        g.fillRoundedRectangle (a, 8.0f);
        g.setColour (t.text);
        g.setFont (ViridianLookAndFeel::labelFont (14.0f));
        g.drawFittedText ("This cab is playing impulse responses.\nIRs carry the mic placement they were recorded with.\n"
                          "Set the cab source to BUILT-IN to place mics.", a.reduced (30.0f).toNearestInt(), Justification::centred, 4);
    }
}

void CabRoomView::drawMicTop (Graphics& g, Point<float> c, float size, Colour colour, const String& label, bool selected, bool on, float tiltDeg, float theta)
{
    const float alpha = on ? 1.0f : 0.35f;
    // aim line: an angled mic points across the cone; show which way and how far
    if (tiltDeg > 0.5f)
    {
        const float len = size * 0.9f + size * 1.6f * tiltDeg / kMaxMicAngleDeg;
        g.setColour (colour.withAlpha (0.6f * alpha));
        const Point<float> dir (-std::cos (theta), -std::sin (theta));
        g.drawLine (Line<float> (c, c + dir * len), 2.0f);
    }
    g.setColour (Colours::black.withAlpha (0.5f * alpha));
    g.fillEllipse (Rectangle<float> (size * 1.1f, size * 1.1f).withCentre (c.translated (1.5f, 2.5f)));
    g.setGradientFill (ColourGradient (Colour (0xffe6e6e6).withAlpha (alpha), c.x - size * 0.4f, c.y - size * 0.4f,
                                       Colour (0xff5a5a5a).withAlpha (alpha), c.x + size * 0.5f, c.y + size * 0.5f, true));
    g.fillEllipse (Rectangle<float> (size, size).withCentre (c));
    // grille mesh
    g.setColour (Colours::black.withAlpha (0.25f * alpha));
    for (float k = -0.3f; k <= 0.31f; k += 0.15f)
    {
        g.drawLine (c.x + k * size, c.y - size * 0.42f, c.x + k * size, c.y + size * 0.42f, 0.7f);
        g.drawLine (c.x - size * 0.42f, c.y + k * size, c.x + size * 0.42f, c.y + k * size, 0.7f);
    }
    g.setColour (colour.withAlpha (alpha));
    g.drawEllipse (Rectangle<float> (size, size).withCentre (c), selected ? 3.0f : 2.0f);
    if (selected)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (size + 10.0f, size + 10.0f).withCentre (c), 1.5f);
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (Rectangle<float> (18.0f, 14.0f).withCentre (c.translated (size * 0.62f, -size * 0.62f)), 3.0f);
    g.setColour (colour.withAlpha (alpha));
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText (label, Rectangle<float> (18.0f, 14.0f).withCentre (c.translated (size * 0.62f, -size * 0.62f)), Justification::centred);
}

void CabRoomView::drawFront (Graphics& g, const VFTheme& t)
{
    const auto geo = getCabGeometry (currentCab());
    const float s = pxPerCm();
    const int rows = geo.speakers / geo.columns;
    const float wCm = (float) geo.columns * geo.spacingCm + 12.0f, hCm = (float) rows * geo.spacingCm + 12.0f;
    auto baffle = Rectangle<float> (wCm * s, hCm * s).withCentre (frontArea.getCentre().translated (0.0f, -10.0f));
    auto cabinet = baffle.expanded (10.0f * s / 3.0f + 6.0f);

    if (viewBack)
        drawRear (g, t, cabinet, baffle);
    else
    {
        // tolex box + piping
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cabinet.translated (0.0f, 5.0f), 10.0f);
        g.setColour (t.tolex);
        g.fillRoundedRectangle (cabinet, 10.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (cabinet.reduced (4.0f), 8.0f, 1.0f);

        // grille cloth (drawn semi-transparent: we are looking "through" it at the speakers)
        g.setColour (t.grille.darker (0.35f));
        g.fillRoundedRectangle (baffle, 4.0f);
        g.setColour (Colours::black.withAlpha (0.08f));
        for (float y = baffle.getY(); y < baffle.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, baffle.getX(), baffle.getRight());

        for (int k = 0; k < geo.speakers; ++k)
        {
            float cx, cy;
            geo.centre (k, cx, cy);
            const auto c = baffleToScreen (cx, cy);
            const float R = geo.coneRadiusCm * s;
            // frame / surround / cone / dust cap
            g.setColour (Colour (0xff1c1c1c));
            g.fillEllipse (Rectangle<float> (R * 2.3f, R * 2.3f).withCentre (c));
            g.setGradientFill (ColourGradient (Colour (0xff3b3b3b), c.x, c.y, Colour (0xff141414), c.x + R, c.y + R, true));
            g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
            g.setColour (Colours::white.withAlpha (0.06f));
            for (float rr = 0.45f; rr < 1.0f; rr += 0.18f)
                g.drawEllipse (Rectangle<float> (R * 2.0f * rr, R * 2.0f * rr).withCentre (c), 1.0f);
            g.setGradientFill (ColourGradient (Colour (0xff5a5a5a), c.x - R * 0.12f, c.y - R * 0.12f, Colour (0xff202020), c.x + R * 0.2f, c.y + R * 0.2f, true));
            g.fillEllipse (Rectangle<float> (R * 0.62f, R * 0.62f).withCentre (c));
            g.setColour (t.textDim.withAlpha (0.8f));
            g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
            if (geo.speakers > 1)
                g.drawText (String (k + 1), Rectangle<float> (20.0f, 14.0f).withCentre (c.translated (-R * 0.92f, -R * 0.92f)), Justification::centred);
        }
    }

    // mics (unselected first so the selected one draws on top)
    String elsewhere;
    for (int m : { 1 - selectedMic, selectedMic })
    {
        const auto st = mic (m);
        if (st.back != viewBack)
        {
            if (st.on) elsewhere << (elsewhere.isEmpty() ? "" : ", ") << "mic " << (m == 0 ? "A" : "B") << (st.back ? " is behind the cab" : " is in front");
            continue;
        }
        const float size = jlimit (18.0f, 34.0f, 4.2f * s) * (1.0f + 0.35f * st.distance); // further back = "higher" in top view
        drawMicTop (g, micFrontPos (m), size, kMicColour[m], m == 0 ? "A" : "B", m == selectedMic, st.on, st.angle, st.theta);
    }
    if (elsewhere.isNotEmpty())
    {
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
        g.drawText (elsewhere + " (" + (viewBack ? "FRONT" : "BACK") + " to see it)", frontArea.withHeight (22.0f).withTrimmedLeft (130.0f).translated (0.0f, 2.0f), Justification::centredLeft);
    }

    // readout
    const auto st = mic (selectedMic);
    const String where = st.position < 0.12f ? "dust cap" : (st.position < 0.45f ? "cap edge" : (st.position < 0.8f ? "mid cone" : "cone edge"));
    String text = String ("MIC ") + (selectedMic == 0 ? "A" : "B") + (st.back ? " (rear): " : ": ") + (geo.speakers > 1 ? "speaker " + String (st.speaker + 1) + ", " : String())
                + where + " (" + String (roundToInt (st.position * 100.0f)) + "%), "
                + String (st.distance * kMaxMicDistanceCm, 1) + " cm, " + String (roundToInt (st.angle)) + " deg off-axis";
    if (selectedMic == 1 && ! st.on) text << "  (off)";
    g.setColour (kMicColour[selectedMic]);
    g.setFont (ViridianLookAndFeel::labelFont (11.0f));
    g.drawText (text, frontArea.withTop (frontArea.getBottom() - 16.0f), Justification::centred);
}

void CabRoomView::drawRear (Graphics& g, const VFTheme& t, Rectangle<float> cabinet, Rectangle<float> baffle)
{
    const auto geo = getCabGeometry (currentCab());
    const bool open = getCabinetSpec (currentCab()).openBack;
    const float s = pxPerCm();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (cabinet.translated (0.0f, 5.0f), 10.0f);
    g.setColour (t.tolex);
    g.fillRoundedRectangle (cabinet, 10.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (cabinet.reduced (4.0f), 8.0f, 1.0f);

    if (open)
    {
        // Open back: a top panel, then the inside of the cab with the speakers' rear (baskets + magnets)
        auto inside = baffle;
        auto panel = inside.removeFromTop (inside.getHeight() * 0.16f);
        g.setColour (t.tolex.darker (0.2f));
        g.fillRect (panel);
        g.setColour (Colour (0xff0b0b0b));
        g.fillRect (inside);
        for (int k = 0; k < geo.speakers; ++k)
        {
            float cx, cy;
            geo.centre (k, cx, cy);
            const auto c = baffleToScreen (cx, cy);
            const float R = geo.coneRadiusCm * s;
            g.setColour (Colour (0xff2e2e2e));
            g.drawEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 2.0f);
            for (int spoke = 0; spoke < 4; ++spoke)
            {
                const float a = MathConstants<float>::halfPi * (float) spoke + 0.785f;
                g.drawLine (Line<float> (c, c + Point<float> (std::cos (a), std::sin (a)) * R), 3.0f);
            }
            g.setGradientFill (ColourGradient (Colour (0xff8a8a8a), c.x - R * 0.2f, c.y - R * 0.2f, Colour (0xff2a2a2a), c.x + R * 0.3f, c.y + R * 0.3f, true));
            g.fillEllipse (Rectangle<float> (R * 0.85f, R * 0.85f).withCentre (c)); // magnet
            g.setColour (t.textDim.withAlpha (0.8f));
            g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
            if (geo.speakers > 1)
                g.drawText (String (k + 1), Rectangle<float> (20.0f, 14.0f).withCentre (c.translated (R * 0.92f, -R * 0.92f)), Justification::centred);
        }
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
        g.drawText ("OPEN BACK - rear of the speakers (out of phase with the front)", baffle.withHeight (panel.getHeight()), Justification::centred);
    }
    else
    {
        // Closed back: a solid panel with screws and a jack plate; mics hear through the wood
        g.setColour (t.tolex.darker (0.15f));
        g.fillRoundedRectangle (baffle, 4.0f);
        for (float fx : { 0.06f, 0.94f })
            for (float fy : { 0.06f, 0.94f })
                ViridianLookAndFeel::drawScrew (g, { baffle.getX() + baffle.getWidth() * fx, baffle.getY() + baffle.getHeight() * fy }, 4.0f);
        auto jack = Rectangle<float> (60.0f, 40.0f).withCentre ({ baffle.getCentreX(), baffle.getY() + baffle.getHeight() * 0.12f });
        g.setColour (Colour (0xff9a9a9a));
        g.fillRoundedRectangle (jack, 3.0f);
        g.setColour (Colours::black);
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (jack.getCentre()));
        for (int k = 0; k < geo.speakers; ++k)
        {
            float cx, cy;
            geo.centre (k, cx, cy);
            const auto c = baffleToScreen (cx, cy);
            g.setColour (Colours::black.withAlpha (0.18f));
            g.drawEllipse (Rectangle<float> (geo.coneRadiusCm * s * 2.0f, geo.coneRadiusCm * s * 2.0f).withCentre (c), 1.0f);
        }
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
        g.drawText ("CLOSED BACK - mics hear through the panel (muffled, boomy)", baffle.withTrimmedTop (baffle.getHeight() * 0.85f), Justification::centred);
    }
}

void CabRoomView::drawSide (Graphics& g, const VFTheme& t)
{
    g.setColour (t.panel.darker (0.3f));
    g.fillRoundedRectangle (sideArea, 8.0f);
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText ("SIDE VIEW", sideArea.reduced (10.0f, 6.0f).removeFromTop (12.0f), Justification::centredLeft);
    const auto sg = sideGeometry (sideArea);
    const bool open = getCabinetSpec (currentCab()).openBack;
    g.setFont (ViridianLookAndFeel::labelFont (9.0f, false));
    g.drawText ("FRONT", Rectangle<float> (sg.cabLeft - 60.0f, sideArea.getY() + 6.0f, 50.0f, 12.0f), Justification::centredRight);
    g.drawText (open ? "OPEN BACK" : "CLOSED BACK", Rectangle<float> (sg.cabRight + 10.0f, sideArea.getY() + 6.0f, 80.0f, 12.0f), Justification::centredLeft);

    // cabinet in profile: grille on the left, speaker cone, back panel (or open) on the right
    const auto box = Rectangle<float> (sg.cabLeft, sg.inner.getY(), sg.cabRight - sg.cabLeft, sg.inner.getHeight());
    g.setColour (t.tolex);
    g.fillRect (box.withWidth (6.0f).withX (box.getX()));                      // baffle edge
    g.fillRect (box.withHeight (6.0f));                                        // top
    g.fillRect (box.withHeight (6.0f).withY (box.getBottom() - 6.0f));         // bottom
    if (open) g.fillRect (box.withX (box.getRight() - 6.0f).withWidth (6.0f).withHeight (box.getHeight() * 0.18f));
    else      g.fillRect (box.withX (box.getRight() - 6.0f).withWidth (6.0f));
    g.setColour (t.textDim.withAlpha (0.5f));
    g.drawRect (box.withTrimmedRight (open ? 0.0f : 0.0f), 1.0f);
    if (open)
    {
        g.setColour (t.panel.darker (0.3f));   // the open part of the back
        g.fillRect (box.withX (box.getRight() - 1.5f).withWidth (3.0f).withTrimmedTop (box.getHeight() * 0.18f).withTrimmedBottom (6.0f));
    }
    g.setColour (t.grille);
    g.fillRect (Rectangle<float> (box.getX() - 3.0f, box.getY(), 3.0f, box.getHeight()));
    Path cone;
    cone.startNewSubPath (box.getX() + 6.0f, sg.cy - sg.coneR);
    cone.lineTo (box.getX() + 26.0f, sg.cy - sg.coneR * 0.25f);
    cone.lineTo (box.getX() + 26.0f, sg.cy + sg.coneR * 0.25f);
    cone.lineTo (box.getX() + 6.0f, sg.cy + sg.coneR);
    g.setColour (Colour (0xff3a3a3a));
    g.strokePath (cone, PathStrokeType (3.0f));
    g.setColour (Colour (0xff6a6a6a));
    g.fillRect (Rectangle<float> (box.getX() + 26.0f, sg.cy - sg.coneR * 0.3f, 12.0f, sg.coneR * 0.6f)); // magnet

    // distance scales either side
    g.setColour (t.textDim.withAlpha (0.6f));
    for (int side = 0; side < 2; ++side)
        for (int cm = 0; cm <= 30; cm += 10)
        {
            const float off = 8.0f + sg.span * (float) cm / kMaxMicDistanceCm;
            const float x = side == 0 ? sg.cabLeft - off : sg.cabRight + off;
            g.drawVerticalLine ((int) x, sg.inner.getBottom() + 2.0f, sg.inner.getBottom() + 6.0f);
            g.drawText (String (cm) + (cm == 30 ? " cm" : ""), Rectangle<float> (x - 18.0f, sg.inner.getBottom() + 6.0f, 36.0f, 12.0f), Justification::centred);
        }

    for (int m : { 1 - selectedMic, selectedMic })
    {
        const auto st = mic (m);
        const auto p = micSidePos (m);
        const float alpha = st.on ? 1.0f : 0.35f;
        // capsule faces the cab; the body extends away from it; tilted by its angle
        const float a = degreesToRadians (st.angle) * (st.theta < 0.0f ? 1.0f : -1.0f) + (st.back ? 0.0f : MathConstants<float>::pi);
        Path body;
        body.addRoundedRectangle (0.0f, -7.0f, 56.0f, 14.0f, 6.0f);
        body.addEllipse (-4.0f, -9.0f, 18.0f, 18.0f);
        const auto xf = AffineTransform::rotation (st.back ? a : -a + MathConstants<float>::twoPi).translated (p.x, p.y);
        g.setColour (Colours::black.withAlpha (0.4f * alpha));
        g.fillPath (body, xf.translated (1.5f, 2.5f));
        g.setGradientFill (ColourGradient (Colour (0xffcfcfcf).withAlpha (alpha), p.x, p.y - 8.0f, Colour (0xff4a4a4a).withAlpha (alpha), p.x, p.y + 8.0f, false));
        g.fillPath (body, xf);
        g.setColour (kMicColour[m].withAlpha (alpha));
        g.strokePath (body, PathStrokeType (m == selectedMic ? 2.2f : 1.4f), xf);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f));
        g.drawText (m == 0 ? "A" : "B", Rectangle<float> (14.0f, 12.0f).withCentre (p.translated (st.back ? 30.0f : -30.0f, -16.0f)), Justification::centred);
    }
}

void CabRoomView::drawResponse (Graphics& g, const VFTheme& t)
{
    g.setColour (t.panel.darker (0.3f));
    g.fillRoundedRectangle (responseArea, 8.0f);
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText ("RESPONSE", responseArea.reduced (10.0f, 6.0f).removeFromTop (12.0f), Justification::centredLeft);
    if (curveMix.empty()) return;

    auto plot = responseArea.reduced (12.0f, 8.0f).withTrimmedTop (12.0f).withTrimmedBottom (10.0f);
    const float top = *std::max_element (curveMix.begin(), curveMix.end()) + 3.0f, range = 36.0f;
    g.setFont (ViridianLookAndFeel::labelFont (9.0f, false));
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const float x = plot.getX() + plot.getWidth() * (float) (std::log (f / 50.0) / std::log (12000.0 / 50.0));
        g.setColour (t.panelEdge.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (t.textDim);
        g.drawText (f >= 1000.0 ? String ((int) (f / 1000.0)) + "k" : String ((int) f), Rectangle<float> (x - 14.0f, plot.getBottom(), 28.0f, 10.0f), Justification::centred);
    }
    auto curve = [&] (const std::vector<float>& c, Colour col, float width)
    {
        Path p;
        for (size_t i = 0; i < c.size(); ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (c.size() - 1);
            const float y = plot.getY() + plot.getHeight() * jlimit (0.0f, 1.0f, (top - c[i]) / range);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, PathStrokeType (width));
    };
    const bool bOn = mic (1).on;
    if (bOn)
    {
        curve (curveA, kMicColour[0].withAlpha (0.55f), 1.2f);
        curve (curveB, kMicColour[1].withAlpha (0.55f), 1.2f);
    }
    curve (curveMix, bOn ? Colours::white.withAlpha (0.9f) : kMicColour[0], 2.0f);
    g.setColour (t.textDim);
    g.drawText (bOn ? "A + B mix (white)" : "Mic A", plot.removeFromTop (12.0f), Justification::centredRight);
}
