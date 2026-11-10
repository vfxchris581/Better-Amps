#include "CabCube.h"
#include "ViridianLookAndFeel.h"
#include "vf/Parameters.h"
#include "vf/dsp/CabMics.h"
#include <algorithm>
#include <array>

using namespace juce;
using namespace vf::dsp;
using namespace vf::dsp::cabfaces;

static const Colour kCubeMicColour[4] { Colour (0xff3ee07a), Colour (0xffffa12e), Colour (0xffb78cff), Colour (0xff5aa9ff) };

const CabCubeView::MicIds& CabCubeView::idsFor (int mic)
{
    using namespace vf::ParamID;
    static const MicIds ids[4] {
        { micAFace, micASpeaker, micPosition, micDistance, nullptr, micAU, micAV },
        { micBFace, micBSpeaker, micBPos, micBDist, micBOn, micBU, micBV },
        { micCFace, micCSpeaker, micCPos, micCDist, micCOn, micCU, micCV },
        { micDFace, micDSpeaker, micDPos, micDDist, micDOn, micDU, micDV } };
    return ids[mic];
}

CabCubeView::CabCubeView (ViridianForgeProcessor& p, int cabIndex)
    : processor (p), state (p.getState()), index (cabIndex)
{
    view.yaw = 0.55f;
    view.pitch = -0.32f;
    view.distance = 4.0f;
    startTimerHz (20);
}

CabCubeView::~CabCubeView() { stopTimer(); }

float CabCubeView::value (const char* id) const
{
    if (auto* v = state.getRawParameterValue (ViridianForgeProcessor::cabPrefix (index) + id)) return v->load();
    return 0.0f;
}

void CabCubeView::setValue (const char* id, float plain)
{
    if (auto* prm = state.getParameter (ViridianForgeProcessor::cabPrefix (index) + id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void CabCubeView::timerCallback() { repaint(); }

float CabCubeView::cubeScale() const
{
    return jmin (getWidth(), getHeight()) * 0.30f;
}

juce::Point<float> CabCubeView::centreOffset (juce::Point<float> p) const
{
    return p - getLocalBounds().toFloat().getCentre();
}

juce::Point<float> CabCubeView::project (Vec3 p, float& depth) const
{
    View v = view;
    v.scale = cubeScale();
    const auto q = vf::dsp::cabfaces::project (p, v, depth);
    return getLocalBounds().toFloat().getCentre() + juce::Point<float> (q.x, q.y);
}

// Where a mic sits on the cube: front and back use its speaker and cone position; sides use the face centre.
juce::Point<float> CabCubeView::micScreenPos (int mic, float& depth) const
{
    const auto& ids = idsFor (mic);
    const int face = jlimit (0, 5, roundToInt (value (ids.face)));
    const auto geo = getCabGeometry (static_cast<CabModel> (jlimit (0, (int) CabModel::count - 1, roundToInt (value (vf::ParamID::cabModel)))));
    const int spk = jlimit (0, geo.speakers - 1, roundToInt (value (ids.speaker)));
    const float W = (float) geo.columns * geo.spacingCm, H = (float) (geo.speakers / geo.columns) * geo.spacingCm;
    float sx = 0.0f, sy = 0.0f;
    geo.centre (spk, sx, sy);
    if (face == (int) Face::Front || face == (int) Face::Back)
    {
        const float r = value (ids.pos) / 10.0f * geo.coneRadiusCm;
        const float x = (sx + r) / W, y = -sy / H;     // cube coordinates (front face, v up)
        return project (pointOnFace ((Face) face, x, y), depth);
    }
    // Sides, top and bottom: where the mic was dropped on the face.
    return project (pointOnFace ((Face) face, value (ids.u), value (ids.v)), depth);
}

void CabCubeView::paint (Graphics& g)
{
    if (auto* lf = dynamic_cast<ViridianLookAndFeel*> (&getLookAndFeel()))
        g.fillAll (lf->getTheme().panel.darker (0.2f));
    else
        g.fillAll (Colours::black);

    // Faces, back to front by depth.
    struct FaceDraw { Face f; float depth; };
    std::array<FaceDraw, 6> order {};
    for (int i = 0; i < 6; ++i)
    {
        float d = 0.0f;
        project (pointOnFace ((Face) i, 0.0f, 0.0f), d);
        order[(size_t) i] = { (Face) i, d };
    }
    std::sort (order.begin(), order.end(), [] (const FaceDraw& a, const FaceDraw& b) { return a.depth < b.depth; });

    for (const auto& fd : order)
    {
        if (! faceVisible (fd.f, view)) continue;
        Path path;
        for (int c = 0; c < 4; ++c)
        {
            const float cu[4] { -0.5f, 0.5f, 0.5f, -0.5f }, cv[4] { -0.5f, -0.5f, 0.5f, 0.5f };
            float d = 0.0f;
            const auto pt = project (pointOnFace (fd.f, cu[c], cv[c]), d);
            if (c == 0) path.startNewSubPath (pt); else path.lineTo (pt);
        }
        path.closeSubPath();
        g.setColour (fd.f == Face::Front ? Colour (0xff27302b) : Colour (0xff1c231f));
        g.fillPath (path);
        g.setColour (Colour (0xff3b4a43));
        g.strokePath (path, PathStrokeType (1.0f));

        float d = 0.0f;
        const auto c = project (pointOnFace (fd.f, 0.0f, 0.0f), d);
        g.setColour (Colour (0xff8d9c95));
        g.setFont (ViridianLookAndFeel::labelFont (9.0f));
        g.drawText (String (fd.f == Face::Front ? "FRONT" : fd.f == Face::Back ? "BACK" : fd.f == Face::Top ? "TOP"
                    : fd.f == Face::Bottom ? "BOTTOM" : fd.f == Face::Left ? "LEFT" : "RIGHT"),
                    Rectangle<float> (60.0f, 14.0f).withCentre (c), Justification::centred);
    }

    // Speakers on the front face.
    const auto geo = getCabGeometry (static_cast<CabModel> (jlimit (0, (int) CabModel::count - 1, roundToInt (value (vf::ParamID::cabModel)))));
    const float W = (float) geo.columns * geo.spacingCm, H = (float) (geo.speakers / geo.columns) * geo.spacingCm;
    for (int k = 0; k < geo.speakers; ++k)
    {
        float sx = 0.0f, sy = 0.0f;
        geo.centre (k, sx, sy);
        float d = 0.0f;
        const auto c = project (pointOnFace (Face::Front, sx / W, -sy / H), d);
        const float rad = geo.coneRadiusCm / W * cubeScale() * 0.9f;
        g.setColour (Colour (0xff0f1311));
        g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        g.setColour (Colour (0xff4a6b57));
        g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), 2.0f);
    }

    // Mics, drawn last so they sit on top.
    for (int m = 0; m < 4; ++m)
    {
        const bool on = m == 0 || value (idsFor (m).on) > 0.5f;
        float d = 0.0f;
        const auto c = micScreenPos (m, d);
        const float size = 14.0f;
        g.setColour (kCubeMicColour[m].withAlpha (on ? 1.0f : 0.3f));
        g.fillEllipse (Rectangle<float> (size, size).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.8f));
        g.setFont (ViridianLookAndFeel::labelFont (10.0f));
        g.drawText (String::charToString ((juce_wchar) ('A' + m)), Rectangle<float> (size, size).withCentre (c), Justification::centred);
    }

    g.setColour (Colours::white.withAlpha (0.5f));
    g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
    g.drawText ("drag a mic onto a face  |  drag empty space to orbit", getLocalBounds().reduced (8).removeFromBottom (16), Justification::centred);
}

void CabCubeView::mouseDown (const MouseEvent& e)
{
    dragMic = -1;
    orbiting = false;
    lastMouse = e.position;
    float bestDist = 18.0f;
    for (int m = 0; m < 4; ++m)
    {
        const bool on = m == 0 || value (idsFor (m).on) > 0.5f;
        if (! on) continue;
        float d = 0.0f;
        const auto c = micScreenPos (m, d);
        if (c.getDistanceFrom (e.position) < bestDist) { bestDist = c.getDistanceFrom (e.position); dragMic = m; }
    }
    if (dragMic < 0) orbiting = true;
    dragPoint = e.position;
}

void CabCubeView::mouseDrag (const MouseEvent& e)
{
    if (orbiting)
    {
        const auto d = e.position - lastMouse;
        view.yaw += d.x * 0.01f;
        view.pitch = jlimit (-1.3f, 1.3f, view.pitch + d.y * 0.01f);
        lastMouse = e.position;
        repaint();
    }
    else if (dragMic >= 0)
    {
        dragPoint = e.position;
        repaint();
    }
}

void CabCubeView::mouseUp (const MouseEvent& e)
{
    if (dragMic >= 0 && ! orbiting)
        dropMic (dragMic, e.position);
    dragMic = -1;
    orbiting = false;
}

void CabCubeView::dropMic (int mic, Point<float> at)
{
    const auto p = centreOffset (at) / cubeScale();
    Face face {};
    float u = 0.0f, v = 0.0f;
    // Screen -> cube: the view's own scale is cubeScale(), so the pick works in unit-scale coordinates.
    View pickView = view;
    pickView.scale = 1.0f;
    if (! pickFace ({ p.x, p.y }, pickView, face, u, v)) return;

    const auto& ids = idsFor (mic);
    setValue (ids.face, (float) (int) face);
    if (face != Face::Front && face != Face::Back)
    {
        setValue (ids.u, u);
        setValue (ids.v, v);
    }

    if (face == Face::Front || face == Face::Back)
    {
        const auto geo = getCabGeometry (static_cast<CabModel> (jlimit (0, (int) CabModel::count - 1, roundToInt (value (vf::ParamID::cabModel)))));
        const float W = (float) geo.columns * geo.spacingCm, H = (float) (geo.speakers / geo.columns) * geo.spacingCm;
        const float x = u * W, y = -v * H;      // baffle cm (y down)
        int best = 0;
        float bestD = 1e9f;
        for (int k = 0; k < geo.speakers; ++k)
        {
            float sx = 0.0f, sy = 0.0f;
            geo.centre (k, sx, sy);
            const float dd = std::hypot (x - sx, y - sy);
            if (dd < bestD) { bestD = dd; best = k; }
        }
        float sx = 0.0f, sy = 0.0f;
        geo.centre (best, sx, sy);
        const float pos = jlimit (0.0f, 10.0f, bestD / jmax (1.0f, geo.coneRadiusCm) * 10.0f);
        setValue (ids.speaker, (float) best);
        setValue (ids.pos, pos);
    }
}
