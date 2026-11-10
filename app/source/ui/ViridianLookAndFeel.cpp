#include "ViridianLookAndFeel.h"
#include "vf/Brand.h"

using namespace juce;

VFTheme VFTheme::dark()
{
    VFTheme t;
    t.id = "dark";
    t.background      = Colour (0xff0f1112);
    t.panel           = Colour (0xff1a1d1f);
    t.panelEdge       = Colour (0xff2c3134);
    t.text            = Colour (0xffe9ece9);
    t.textDim         = Colour (0xff8c9690);
    t.accent          = Colour (vf::brand::viridian);
    t.accentDeep      = Colour (vf::brand::viridianDeep);
    t.faceplateTop    = Colour (0xff2a2e31);
    t.faceplateBottom = Colour (0xff15181a);
    t.tolex           = Colour (0xff161616);
    t.grille          = Colour (0xff23302a);
    t.knobBody        = Colour (0xff2b2b2b);
    t.knobPointer     = Colour (0xfff2f2f2);
    t.led             = Colour (vf::brand::viridian);
    t.ledOff          = Colour (0xff1f3327);
    return t;
}

VFTheme VFTheme::light()
{
    VFTheme t;
    t.id = "light";
    t.background      = Colour (0xffe7e8e4);
    t.panel           = Colour (0xfff4f4f1);
    t.panelEdge       = Colour (0xffc6c9c3);
    t.text            = Colour (0xff1b1e1c);
    t.textDim         = Colour (0xff5d6660);
    t.accent          = Colour (vf::brand::viridianDeep);
    t.accentDeep      = Colour (0xff075a28);
    t.faceplateTop    = Colour (0xffd9dcd8); // brushed aluminium faceplate
    t.faceplateBottom = Colour (0xffb5b9b4);
    t.tolex           = Colour (0xff2a2a2a);
    t.grille          = Colour (0xffcfc6a8); // wheat grille cloth
    t.knobBody        = Colour (0xff262626);
    t.knobPointer     = Colour (0xfff7f7f7);
    t.led             = Colour (vf::brand::viridian);
    t.ledOff          = Colour (0xff9fb0a5);
    return t;
}

//==============================================================================
ViridianLookAndFeel::ViridianLookAndFeel() { setTheme (VFTheme::dark()); }

void ViridianLookAndFeel::setTheme (const VFTheme& t)
{
    theme = t;
    setColour (ResizableWindow::backgroundColourId, theme.background);
    setColour (Label::textColourId, theme.text);
    setColour (Slider::textBoxTextColourId, theme.text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, theme.panel);
    setColour (ComboBox::textColourId, theme.text);
    setColour (ComboBox::outlineColourId, theme.panelEdge);
    setColour (ComboBox::arrowColourId, theme.accent);
    setColour (PopupMenu::backgroundColourId, theme.panel);
    setColour (PopupMenu::textColourId, theme.text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme.accentDeep);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::buttonColourId, theme.panel);
    setColour (TextButton::textColourOffId, theme.text);
    setColour (TextButton::textColourOnId, theme.accent);
    setColour (ToggleButton::textColourId, theme.text);
    setColour (TooltipWindow::backgroundColourId, theme.panel);
    setColour (TooltipWindow::textColourId, theme.text);
}

Font ViridianLookAndFeel::labelFont (float height, bool bold)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain)).withExtraKerningFactor (0.06f);
}

Font ViridianLookAndFeel::getLabelFont (Label& l)
{
    if (auto* box = dynamic_cast<ComboBox*> (l.getParentComponent()))
        return getComboBoxFont (*box);
    return labelFont (jlimit (10.0f, 16.0f, (float) l.getHeight() * 0.7f));
}
Font ViridianLookAndFeel::getComboBoxFont (ComboBox& c)     { return labelFont (jlimit (11.0f, 18.0f, (float) c.getHeight() * 0.38f), false); }
Font ViridianLookAndFeel::getTextButtonFont (TextButton&, int h) { return labelFont (jmax (10.0f, (float) h * 0.42f)); }

Label* ViridianLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (labelFont (12.0f, false));
    l->setColour (Label::textColourId, theme.textDim);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    return l;
}

void ViridianLookAndFeel::drawScrew (Graphics& g, Point<float> c, float r)
{
    g.setGradientFill (ColourGradient (Colour (0xffe4e4e4), c.x - r, c.y - r, Colour (0xff6a6a6a), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colour (0xff3a3a3a));
    g.drawLine (c.x - r * 0.65f, c.y, c.x + r * 0.65f, c.y, jmax (1.0f, r * 0.25f));
}

void ViridianLookAndFeel::drawLed (Graphics& g, Rectangle<float> a, Colour on, bool isOn)
{
    const auto c = a.getCentre();
    const float r = jmin (a.getWidth(), a.getHeight()) * 0.5f;
    if (isOn)
    {
        g.setGradientFill (ColourGradient (on.withAlpha (0.45f), c.x, c.y, on.withAlpha (0.0f), c.x + r * 2.4f, c.y, true));
        g.fillEllipse (a.expanded (r * 1.4f));
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (a.expanded (1.0f));
    const auto body = isOn ? on : on.darker (2.2f).withSaturation (0.35f);
    g.setGradientFill (ColourGradient (body.brighter (isOn ? 0.8f : 0.2f), c.x - r * 0.4f, c.y - r * 0.5f, body, c.x + r, c.y + r, true));
    g.fillEllipse (a);
}

void ViridianLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider&)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat();
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float rOuter = size * 0.5f - 2.0f;
    const float rTicks = rOuter;
    const float rKnob = rOuter * 0.74f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Scale ticks 0..10 printed on the faceplate
    g.setColour (theme.textDim);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const float len = (i % 5 == 0) ? rOuter * 0.16f : rOuter * 0.09f;
        Point<float> p1 (centre.x + std::sin (a) * (rTicks - len), centre.y - std::cos (a) * (rTicks - len));
        Point<float> p2 (centre.x + std::sin (a) * rTicks, centre.y - std::cos (a) * rTicks);
        g.drawLine ({ p1, p2 }, i % 5 == 0 ? 1.6f : 1.0f);
    }

    // Value arc in brand green
    Path arc;
    arc.addCentredArc (centre.x, centre.y, rKnob + rOuter * 0.12f, rKnob + rOuter * 0.12f, 0.0f, startAngle, angle, true);
    g.setColour (theme.accent.withAlpha (0.85f));
    g.strokePath (arc, PathStrokeType (jmax (2.0f, rOuter * 0.06f), PathStrokeType::curved, PathStrokeType::rounded));

    // Drop shadow
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (centre.x - rKnob + rKnob * 0.08f, centre.y - rKnob + rKnob * 0.14f, rKnob * 2, rKnob * 2);

    // Knurled skirt
    const auto skirt = Rectangle<float> (rKnob * 2, rKnob * 2).withCentre (centre);
    g.setGradientFill (ColourGradient (theme.knobBody.brighter (0.5f), skirt.getX(), skirt.getY(),
                                       theme.knobBody.darker (0.8f), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse (skirt);
    g.setColour (Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (centre.x + std::sin (a) * rKnob * 0.86f, centre.y - std::cos (a) * rKnob * 0.86f,
                    centre.x + std::sin (a) * rKnob, centre.y - std::cos (a) * rKnob, 1.0f);
    }

    // Brushed metal cap
    const float rCap = rKnob * 0.72f;
    const auto cap = Rectangle<float> (rCap * 2, rCap * 2).withCentre (centre);
    ColourGradient metal (Colour (0xfff4f5f6), cap.getX(), cap.getY(), Colour (0xff7e848a), cap.getRight(), cap.getBottom(), false);
    metal.addColour (0.45, Colour (0xffc7cbd0));
    metal.addColour (0.55, Colour (0xffa9aeb4));
    g.setGradientFill (metal);
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawEllipse (cap, 1.0f);

    // Pointer
    Path pointer;
    const float pw = jmax (2.0f, rKnob * 0.12f);
    pointer.addRoundedRectangle (-pw * 0.5f, -rKnob * 0.98f, pw, rKnob * 0.55f, pw * 0.5f);
    g.setColour (theme.knobPointer);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (pointer, PathStrokeType (0.8f), AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void ViridianLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    // Hardware-style: LED above a small rocker label.
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const float ledSize = jmin (r.getHeight() * 0.32f, 12.0f);
    auto ledArea = r.removeFromTop (ledSize + 4.0f).withSizeKeepingCentre (ledSize, ledSize);
    drawLed (g, ledArea, theme.led, b.getToggleState());

    auto body = r.reduced (r.getWidth() * 0.08f, 2.0f);
    g.setGradientFill (ColourGradient (theme.panel.brighter (0.15f), body.getX(), body.getY(),
                                       theme.panel.darker (0.4f), body.getX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (highlighted ? theme.accent : theme.panelEdge);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
    g.setColour (b.getToggleState() ? theme.text : theme.textDim);
    g.setFont (labelFont (jlimit (9.0f, 14.0f, body.getHeight() * 0.48f)));
    g.drawFittedText (b.getButtonText().toUpperCase(), body.toNearestInt(), Justification::centred, 1);
}

void ViridianLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setGradientFill (ColourGradient ((on ? theme.accentDeep : theme.panel).brighter (down ? 0.0f : 0.12f), r.getX(), r.getY(),
                                       (on ? theme.accentDeep : theme.panel).darker (0.35f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (highlighted || on ? theme.accent : theme.panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void ViridianLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (theme.panel.darker (0.25f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme.accent : theme.panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (theme.accent);
    g.fillPath (arrow);
}

void ViridianLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                            Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
        return;
    }
    // Hardware fader: slot, centre detent mark, metal cap
    const auto r = Rectangle<int> (x, y, w, h).toFloat();
    const float cx = r.getCentreX();
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (cx - 2.5f, r.getY() + 4.0f, 5.0f, r.getHeight() - 8.0f, 2.5f);
    g.setColour (theme.textDim.withAlpha (0.6f));
    g.drawHorizontalLine (roundToInt (r.getCentreY()), cx - 8.0f, cx + 8.0f);

    const float capH = jmin (18.0f, r.getHeight() * 0.18f), capW = jmin (w * 0.8f, 24.0f);
    auto cap = Rectangle<float> (capW, capH).withCentre ({ cx, sliderPos });
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    ColourGradient metal (Colour (0xffeef0f2), cap.getX(), cap.getY(), Colour (0xff7d838a), cap.getX(), cap.getBottom(), false);
    metal.addColour (0.5, Colour (0xffb7bcc2));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (theme.accent);
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
}
