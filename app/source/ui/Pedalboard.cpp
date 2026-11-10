#include "Pedalboard.h"
#include "Panels.h"
#include "vf/Parameters.h"

using namespace juce;
using vf::graph::kNumFxSlots;
using vf::graph::kAmp1;
using vf::graph::kAmp2;
using vf::graph::kCab1;
using vf::graph::kCab2;

static VFTheme themeOf (Component& c)
{
    if (auto* l = dynamic_cast<ViridianLookAndFeel*> (&c.getLookAndFeel())) return l->getTheme();
    return VFTheme::dark();
}

// Menu ids: 1..N effect types (type index + 1), then blocks.
static constexpr int kMenuAmp1 = 10001, kMenuAmp2 = 10002, kMenuCab1 = 10003, kMenuCab2 = 10004;

//==============================================================================
Colour Pedalboard::categoryColour (vf::fx::FxCategory c)
{
    using C = vf::fx::FxCategory;
    switch (c)
    {
        case C::Drive:      return Colour (0xffe8792b);
        case C::Modulation: return Colour (0xff3d8bf0);
        case C::Delay:      return Colour (0xff1fb8a6);
        case C::Reverb:     return Colour (0xff8c6cf0);
        case C::Dynamics:   return Colour (0xffe0c232);
        case C::Filter:     return Colour (0xffe0559a);
        case C::Pitch:      return Colour (0xff43c1e8);
        case C::Creative:   return Colour (0xffc65fe0);
        case C::Utility:
        case C::count:
        default:            return Colour (0xff8e98a3);
    }
}

Colour Pedalboard::blockColour (const ViridianForgeProcessor& p, int key)
{
    if (vf::graph::isAmpKey (key)) return Colour (0xff1fc24a);
    if (vf::graph::isCabKey (key)) return Colour (0xff6f8a5a);
    const int t = p.getSlotType (key);
    return t >= 0 ? categoryColour (vf::fx::effectTypes()[static_cast<size_t> (t)].category) : Colours::grey;
}

String Pedalboard::blockTitle (const ViridianForgeProcessor& p, int key)
{
    if (key == kAmp1) return "AMP 1";
    if (key == kAmp2) return "AMP 2";
    if (key == kCab1) return "CAB 1";
    if (key == kCab2) return "CAB 2";
    const int t = p.getSlotType (key);
    return t >= 0 ? String (vf::fx::effectTypes()[static_cast<size_t> (t)].name) : String ("Empty");
}

PopupMenu Pedalboard::buildAddMenu (const ViridianForgeProcessor& p)
{
    PopupMenu menu;
    const bool full = p.freeSlotCount() == 0;
    const auto& types = vf::fx::effectTypes();
    for (int c = 0; c < static_cast<int> (vf::fx::FxCategory::count); ++c)
    {
        PopupMenu sub;
        for (size_t i = 0; i < types.size(); ++i)
            if (static_cast<int> (types[i].category) == c)
                sub.addItem (static_cast<int> (i) + 1, types[i].name, ! full);
        menu.addSubMenu (vf::fx::categoryName (static_cast<vf::fx::FxCategory> (c)), sub, ! full);
    }
    menu.addSeparator();
    const auto& ch = p.getChain();
    menu.addItem (kMenuAmp1, "Amp 1", ! ch.contains (kAmp1));
    menu.addItem (kMenuAmp2, "Amp 2", ! ch.contains (kAmp2));
    menu.addItem (kMenuCab1, "Cab 1", ! ch.contains (kCab1));
    menu.addItem (kMenuCab2, "Cab 2", ! ch.contains (kCab2));
    if (full)
    {
        menu.addSeparator();
        menu.addItem (-1, "All " + String (kNumFxSlots) + " effect slots are in use", false);
    }
    return menu;
}

void Pedalboard::addFromMenu (ViridianForgeProcessor& p, int r, Section section, int index, std::function<void (int)> selectKey)
{
    int key = -1;
    if (r >= 1 && r <= static_cast<int> (vf::fx::effectTypes().size()))
        key = p.addEffect (r - 1, section, index);
    else if (r >= kMenuAmp1 && r <= kMenuCab2)
    {
        key = r == kMenuAmp1 ? kAmp1 : r == kMenuAmp2 ? kAmp2 : r == kMenuCab1 ? kCab1 : kCab2;
        p.moveBlock (key, section, index);
    }
    if (key >= 0 && selectKey) selectKey (key);
}

//==============================================================================
Pedalboard::Pedalboard (ViridianForgeProcessor& p)
    : processor (p), state (p.getState()),
      levelA (p.getState(), vf::ParamID::laneALevel, "A Level"),
      panA (p.getState(), vf::ParamID::laneAPan, "A Pan"),
      levelB (p.getState(), vf::ParamID::laneBLevel, "B Level"),
      panB (p.getState(), vf::ParamID::laneBPan, "B Pan"),
      splitMode (p.getState(), vf::ParamID::splitMode, "Split")
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (splitButton);
    splitButton.setClickingTogglesState (false);
    splitButton.setTooltip ("Split the chain into two parallel lanes (A and B) that merge again - e.g. two amps or two cabs in parallel");
    splitButton.onClick = [this] { processor.setSplit (! processor.getChain().split); };
    for (auto* c : std::initializer_list<Component*> { &levelA, &panA, &levelB, &panB, &splitMode })
        addChildComponent (c);
    setTooltip ("Drag blocks to reorder. Click to edit, double-click to bypass, right-click for more. + adds a block.");
    processor.addChangeListener (this);
    startTimerHz (8); // LED states follow automation
    updateMixerVisibility();
}

Pedalboard::~Pedalboard()
{
    processor.removeChangeListener (this);
}

void Pedalboard::changeListenerCallback (ChangeBroadcaster*)
{
    if (! processor.getChain().contains (selected) && onSelectionChanged)
    {
        // the selected block went away: fall back to something that exists
        const auto keys = processor.getChain().allKeys();
        select (keys.empty() ? kAmp1 : keys.front());
    }
    updateMixerVisibility();
    resized();
    repaint();
}

void Pedalboard::updateMixerVisibility()
{
    const bool split = processor.getChain().split;
    for (auto* c : std::initializer_list<Component*> { &levelA, &panA, &levelB, &panB, &splitMode })
        c->setVisible (split);
    splitButton.setButtonText (split ? "UNSPLIT" : "SPLIT");
    splitButton.setToggleState (split, dontSendNotification);
}

void Pedalboard::select (int key)
{
    selected = key;
    if (onSelectionChanged) onSelectionChanged (key);
    repaint();
}

bool Pedalboard::isBlockOn (int key) const
{
    if (vf::graph::isFxKey (key))
        if (auto* v = state.getRawParameterValue (ViridianForgeProcessor::slotOnId (key))) return v->load() > 0.5f;
    if (vf::graph::isCabKey (key))
        if (auto* v = state.getRawParameterValue (ViridianForgeProcessor::cabPrefix (key == kCab2 ? 1 : 0) + vf::ParamID::cabEnabled)) return v->load() > 0.5f;
    return true;
}

void Pedalboard::toggleBypass (int key)
{
    String id;
    if (vf::graph::isFxKey (key)) id = ViridianForgeProcessor::slotOnId (key);
    else if (vf::graph::isCabKey (key)) id = ViridianForgeProcessor::cabPrefix (key == kCab2 ? 1 : 0) + vf::ParamID::cabEnabled;
    if (auto* param = state.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
        repaint();
    }
}

//==============================================================================
void Pedalboard::resized()
{
    auto r = getLocalBounds().reduced (12, 8).withTrimmedTop (14);
    const bool split = processor.getChain().split;
    auto mixer = r.removeFromRight (split ? jmin (330, r.getWidth() / 3) : 84);
    auto buttonArea = mixer.removeFromLeft (84);
    splitButton.setBounds (buttonArea.withSizeKeepingCentre (76, 26));
    if (split)
    {
        mixer.removeFromLeft (6);
        auto top = mixer.removeFromTop (mixer.getHeight() / 2);
        const int w = mixer.getWidth() / 3;
        splitMode.setBounds (top.removeFromLeft (w).reduced (2, 6));
        levelA.setBounds (top.removeFromLeft (w).reduced (2, 0));
        panA.setBounds (top.reduced (2, 0));
        mixer.removeFromLeft (w);
        levelB.setBounds (mixer.removeFromLeft (w).reduced (2, 0));
        panB.setBounds (mixer.reduced (2, 0));
    }
    board = r.toFloat();
    layoutTiles();
}

void Pedalboard::layoutTiles()
{
    tiles.clear();
    gaps.clear();
    hoverGap = nullptr;
    const auto& ch = processor.getChain();
    const bool split = ch.split;
    const float jack = 34.0f, plusW = 22.0f, node = 14.0f, spacing = 10.0f;

    const int laneLen = split ? (int) jmax (ch.laneA.size(), ch.laneB.size()) : 0;
    const int nTiles = (int) ch.pre.size() + laneLen + (int) ch.post.size();
    const int nPlus = 1 + (split ? 2 : 0) + (split ? 1 : 0);
    const float fixed = 2.0f * jack + (float) nPlus * (plusW + spacing) + (split ? 2.0f * (node + spacing) : 0.0f);
    const float avail = board.getWidth() - fixed - spacing * (float) nTiles;
    const float tileW = jlimit (40.0f, 104.0f, nTiles > 0 ? avail / (float) nTiles : 104.0f);
    const float tileH = split ? jmin (board.getHeight() * 0.44f, tileW * 0.62f + 14.0f) : jmin (board.getHeight() * 0.86f, tileW * 0.9f);

    rowMain = board.getCentreY();
    rowA = board.getY() + board.getHeight() * 0.25f;
    rowB = board.getY() + board.getHeight() * 0.75f;

    float x = board.getX();
    inJack = { x, rowMain - 12.0f, jack, 24.0f };
    x += jack + spacing;

    auto place = [&] (const std::vector<int>& keys, Section s, float rowY, float startX) -> float
    {
        float px = startX;
        for (int i = 0; i < (int) keys.size(); ++i)
        {
            gaps.push_back ({ s, i, { px - spacing, rowY - tileH / 2, spacing, tileH }, false });
            tiles.push_back ({ keys[(size_t) i], s, i, { px, rowY - tileH / 2, tileW, tileH } });
            px += tileW + spacing;
        }
        gaps.push_back ({ s, (int) keys.size(), { px, rowY - 11.0f, plusW, 22.0f }, true });
        return px + plusW + spacing;
    };

    x = place (ch.pre, Section::Pre, rowMain, x);
    if (split)
    {
        splitNode = { x, rowMain - node / 2, node, node };
        x += node + spacing;
        const float laneW = (float) laneLen * (tileW + spacing) + plusW + spacing;
        place (ch.laneA, Section::LaneA, rowA, x);
        place (ch.laneB, Section::LaneB, rowB, x);
        x += laneW;
        mergeNode = { x, rowMain - node / 2, node, node };
        x += node + spacing;
        x = place (ch.post, Section::Post, rowMain, x);
    }
    outJack = { jmin (x, board.getRight() - jack), rowMain - 12.0f, jack, 24.0f };
}

//==============================================================================
void Pedalboard::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (t.panel.brighter (0.05f), 0, r.getY(), t.panel.darker (0.15f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (11.0f));
    g.drawText ("SIGNAL CHAIN", r.reduced (12.0f, 6.0f).removeFromTop (14.0f), Justification::centredLeft);
    const auto& ch = processor.getChain();
    g.setFont (ViridianLookAndFeel::labelFont (10.0f, false));
    g.drawText (String (kNumFxSlots - processor.freeSlotCount()) + " / " + String (kNumFxSlots) + " effect slots",
                r.reduced (12.0f, 6.0f).removeFromTop (14.0f).withTrimmedLeft (110.0f), Justification::centredLeft);

    // Cables
    const Colour cable = t.textDim.withAlpha (0.7f);
    g.setColour (cable);
    auto hline = [&g] (float x1, float x2, float y) { if (x2 > x1) g.drawLine (x1, y, x2, y, 2.0f); };
    const float endPre = ch.split ? splitNode.getCentreX() : outJack.getX();
    hline (inJack.getRight(), endPre, rowMain);
    if (ch.split)
    {
        Path p;
        for (float row : { rowA, rowB })
        {
            p.startNewSubPath (splitNode.getCentre());
            p.cubicTo (splitNode.getCentreX() + 10.0f, row, splitNode.getRight() + 6.0f, row, splitNode.getRight() + 10.0f, row);
            p.lineTo (mergeNode.getX() - 10.0f, row);
            p.cubicTo (mergeNode.getX() - 6.0f, row, mergeNode.getCentreX() - 10.0f, row, mergeNode.getCentreX(), mergeNode.getCentreY());
        }
        g.strokePath (p, PathStrokeType (2.0f));
        hline (mergeNode.getCentreX(), outJack.getX(), rowMain);
        for (auto n : { splitNode, mergeNode })
        {
            g.setColour (t.accent);
            g.fillEllipse (n);
            g.setColour (t.background);
            g.drawEllipse (n, 1.5f);
        }
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f));
        g.drawText ("A", Rectangle<float> (splitNode.getX() - 4.0f, rowA - 8.0f, 12.0f, 12.0f), Justification::centred);
        g.drawText ("B", Rectangle<float> (splitNode.getX() - 4.0f, rowB - 4.0f, 12.0f, 12.0f), Justification::centred);
    }

    // Jacks
    for (auto [jackRect, label] : { std::pair<Rectangle<float>, const char*> { inJack, "IN" }, { outJack, "OUT" } })
    {
        g.setColour (t.panel.darker (0.4f));
        g.fillRoundedRectangle (jackRect, 4.0f);
        g.setColour (t.panelEdge);
        g.drawRoundedRectangle (jackRect, 4.0f, 1.0f);
        g.setColour (t.text);
        g.setFont (ViridianLookAndFeel::labelFont (10.0f));
        g.drawText (label, jackRect, Justification::centred);
    }

    // "+" points
    for (const auto& gap : gaps)
    {
        if (! gap.isPlus) continue;
        const bool hot = hoverGap == &gap && ! dragging;
        g.setColour (hot ? t.accent : t.panel.darker (0.25f));
        g.fillEllipse (gap.r);
        g.setColour (hot ? t.background : t.textDim);
        g.drawEllipse (gap.r.reduced (0.5f), 1.0f);
        g.setFont (ViridianLookAndFeel::labelFont (16.0f));
        g.drawText ("+", gap.r.translated (0.0f, -1.0f), Justification::centred);
    }

    // Blocks
    for (const auto& tile : tiles)
    {
        const bool on = isBlockOn (tile.key);
        const Colour c = blockColour (processor, tile.key);
        auto box = tile.r;
        if (dragging && tile.key == dragKey) { g.setColour (c.withAlpha (0.25f)); g.drawRoundedRectangle (box, 6.0f, 1.5f); continue; }

        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (box.translated (0.0f, 2.5f), 6.0f);
        g.setGradientFill (ColourGradient (c.withMultipliedBrightness (on ? 0.95f : 0.35f), box.getX(), box.getY(),
                                           c.withMultipliedBrightness (on ? 0.55f : 0.22f), box.getX(), box.getBottom(), false));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (tile.key == selected ? Colours::white : Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, tile.key == selected ? 2.0f : 1.0f);

        auto inner = box.reduced (5.0f, 4.0f);
        const auto led = ledArea (tile);
        ViridianLookAndFeel::drawLed (g, led, Colour (0xff4dff7a), on);

        String sub;
        if (vf::graph::isFxKey (tile.key))
            sub = vf::fx::categoryName (vf::fx::effectTypes()[(size_t) jmax (0, processor.getSlotType (tile.key))].category);
        else if (auto* param = state.getParameter (vf::graph::isAmpKey (tile.key) ? ViridianForgeProcessor::ampPrefix (tile.key == kAmp2 ? 1 : 0) + vf::ParamID::ampModel
                                                                                  : ViridianForgeProcessor::cabPrefix (tile.key == kCab2 ? 1 : 0) + vf::ParamID::cabModel))
            sub = param->getCurrentValueAsText();
        if (vf::graph::isAmpKey (tile.key))
            if (auto* src = state.getRawParameterValue (ViridianForgeProcessor::ampPrefix (tile.key == kAmp2 ? 1 : 0) + vf::ParamID::ampSource))
                if (src->load() > 0.5f) sub = "NAM capture";
        if (vf::graph::isCabKey (tile.key))
            if (auto* src = state.getRawParameterValue (ViridianForgeProcessor::cabPrefix (tile.key == kCab2 ? 1 : 0) + vf::ParamID::cabSource))
                if (src->load() > 0.5f) sub = src->load() > 1.5f ? "IR A + B" : "IR A";

        g.setColour (Colours::white.withAlpha (on ? 0.95f : 0.55f));
        const float fs = jlimit (9.0f, 13.0f, box.getWidth() * 0.125f);
        g.setFont (ViridianLookAndFeel::labelFont (fs));
        auto textArea = inner.withTrimmedBottom (led.getHeight() + 2.0f);
        g.drawFittedText (blockTitle (processor, tile.key).toUpperCase(), textArea.removeFromTop (textArea.getHeight() * 0.62f).toNearestInt(),
                          Justification::centred, 2, 0.8f);
        g.setColour (Colours::white.withAlpha (on ? 0.65f : 0.4f));
        g.setFont (ViridianLookAndFeel::labelFont (fs * 0.78f, false));
        g.drawFittedText (sub, textArea.toNearestInt(), Justification::centredTop, 1, 0.7f);
    }

    // Drag feedback: insertion marker + ghost
    if (dragging && dragKey >= 0)
    {
        if (const auto* gp = nearestGap (dragPos))
        {
            g.setColour (t.accent);
            const float x = gp->isPlus ? gp->r.getX() - 4.0f : gp->r.getCentreX() - 1.5f;
            g.fillRoundedRectangle (x, gp->r.getCentreY() - 26.0f, 3.0f, 52.0f, 1.5f);
        }
        const Tile* src = nullptr;
        for (const auto& tl : tiles) if (tl.key == dragKey) src = &tl;
        if (src != nullptr)
        {
            auto ghost = src->r.withCentre (dragPos);
            g.setColour (blockColour (processor, dragKey).withAlpha (0.7f));
            g.fillRoundedRectangle (ghost, 6.0f);
            g.setColour (Colours::white);
            g.setFont (ViridianLookAndFeel::labelFont (11.0f));
            g.drawFittedText (blockTitle (processor, dragKey).toUpperCase(), ghost.reduced (4.0f).toNearestInt(), Justification::centred, 2);
        }
    }

    if (tiles.empty())
    {
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (12.0f, false));
        g.drawText ("Empty chain - the dry signal passes straight through. Click + to add an amp, cab or effect.",
                    board.withTrimmedLeft (90.0f), Justification::centredLeft);
    }
}

Rectangle<float> Pedalboard::ledArea (const Tile& tile) const
{
    const float d = jlimit (6.0f, 10.0f, tile.r.getWidth() * 0.09f);
    return { tile.r.getCentreX() - d / 2, tile.r.getBottom() - d - 5.0f, d, d };
}

const Pedalboard::Tile* Pedalboard::tileAt (Point<float> p) const
{
    for (const auto& tile : tiles) if (tile.r.contains (p)) return &tile;
    return nullptr;
}

const Pedalboard::Gap* Pedalboard::plusAt (Point<float> p) const
{
    for (const auto& gap : gaps) if (gap.isPlus && gap.r.expanded (3.0f).contains (p)) return &gap;
    return nullptr;
}

const Pedalboard::Gap* Pedalboard::nearestGap (Point<float> p) const
{
    const Gap* best = nullptr;
    float bestD = 1.0e9f;
    for (const auto& gap : gaps)
    {
        const float dx = std::abs (p.x - gap.r.getCentreX()), dy = std::abs (p.y - gap.r.getCentreY());
        const float d = dx + dy * 2.5f; // prefer the row the pointer is on
        if (d < bestD) { bestD = d; best = &gap; }
    }
    return best;
}

//==============================================================================
void Pedalboard::mouseMove (const MouseEvent& e)
{
    const auto* g = plusAt (e.position);
    if (g != hoverGap) { hoverGap = g; repaint(); }
    setMouseCursor (g != nullptr || tileAt (e.position) != nullptr ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void Pedalboard::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    dragging = false;
    dragKey = -1;
    if (const auto* tile = tileAt (e.position))
    {
        if (e.mods.isPopupMenu()) { select (tile->key); showBlockMenu (*tile); return; }
        dragKey = tile->key;
        dragPos = e.position;
    }
}

void Pedalboard::mouseDrag (const MouseEvent& e)
{
    if (dragKey < 0) return;
    dragPos = e.position;
    if (! dragging && e.getDistanceFromDragStart() > 6) dragging = true;
    if (dragging) repaint();
}

void Pedalboard::mouseUp (const MouseEvent& e)
{
    if (dragging && dragKey >= 0)
    {
        if (const auto* gp = nearestGap (e.position))
            processor.moveBlock (dragKey, gp->section, gp->index);
        dragging = false;
        dragKey = -1;
        repaint();
        return;
    }
    dragKey = -1;
    if (e.mods.isPopupMenu()) return;

    if (const auto* gp = plusAt (e.position))
    {
        const Section s = gp->section;
        const int index = gp->index;
        buildAddMenu (processor).showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (gp->r.toNearestInt())),
            [this, s, index, sp = SafePointer<Pedalboard> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                addFromMenu (processor, r, s, index, [this] (int key) { select (key); });
            });
        return;
    }
    if (const auto* tile = tileAt (e.position))
    {
        if (ledArea (*tile).expanded (4.0f).contains (e.position)) toggleBypass (tile->key);
        else select (tile->key);
    }
}

void Pedalboard::mouseDoubleClick (const MouseEvent& e)
{
    if (const auto* tile = tileAt (e.position))
        if (! ledArea (*tile).expanded (4.0f).contains (e.position)) toggleBypass (tile->key);
}

bool Pedalboard::keyPressed (const KeyPress& k)
{
    if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && processor.getChain().contains (selected))
    {
        processor.removeBlock (selected);
        return true;
    }
    if (k == KeyPress::spaceKey) { toggleBypass (selected); return true; }
    return false;
}

void Pedalboard::showBlockMenu (const Tile& tile)
{
    const int key = tile.key;
    const bool fx = vf::graph::isFxKey (key);
    const auto& ch = processor.getChain();
    PopupMenu m;
    m.addSectionHeader (blockTitle (processor, key));
    if (fx || vf::graph::isCabKey (key)) m.addItem (1, isBlockOn (key) ? "Bypass" : "Engage");
    if (fx)
    {
        m.addItem (2, "Duplicate", processor.freeSlotCount() > 0);
        PopupMenu change;
        const auto& types = vf::fx::effectTypes();
        for (int c = 0; c < static_cast<int> (vf::fx::FxCategory::count); ++c)
        {
            PopupMenu sub;
            for (size_t i = 0; i < types.size(); ++i)
                if (static_cast<int> (types[i].category) == c)
                    sub.addItem (1000 + static_cast<int> (i), types[i].name, true, static_cast<int> (i) == processor.getSlotType (key));
            change.addSubMenu (vf::fx::categoryName (static_cast<vf::fx::FxCategory> (c)), sub);
        }
        m.addSubMenu ("Change effect", change);
    }
    if (ch.split)
    {
        PopupMenu move;
        move.addItem (10, "Before split", tile.section != Section::Pre);
        move.addItem (11, "Lane A", tile.section != Section::LaneA);
        move.addItem (12, "Lane B", tile.section != Section::LaneB);
        move.addItem (13, "After merge", tile.section != Section::Post);
        m.addSubMenu ("Move to", move);
    }
    m.addSeparator();
    m.addItem (3, "Remove from chain");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (tile.r.toNearestInt())),
        [this, key, sp = SafePointer<Pedalboard> (this)] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            if (r == 1) toggleBypass (key);
            else if (r == 2) { const int k = processor.duplicateEffect (key); if (k >= 0) select (k); }
            else if (r == 3) processor.removeBlock (key);
            else if (r >= 10 && r <= 13)
            {
                const Section s = static_cast<Section> (r - 10);
                processor.moveBlock (key, s, (int) processor.getChain().list (s).size());
            }
            else if (r >= 1000) processor.changeEffectType (key, r - 1000);
        });
}

//==============================================================================
// EffectEditor

/** One control for a slot parameter, shown in the effect's real units. */
class EffectEditor::Control : public Component
{
public:
    Control (RangedAudioParameter& param, const vf::fx::ParamInfo& info) : pi (info)
    {
        caption.setText (String (pi.name).toUpperCase(), dontSendNotification);
        caption.setJustificationType (Justification::centred);
        addAndMakeVisible (caption);
        if (pi.numChoices > 0)
        {
            for (int c = 0; c < pi.numChoices; ++c) box.addItem (pi.choices[c], c + 1);
            box.onChange = [this] { if (attachment) attachment->setValueAsCompleteGesture (pi.toNormalised ((float) (box.getSelectedId() - 1))); };
            addAndMakeVisible (box);
        }
        else
        {
            slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
            slider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 16);
            slider.setRange (pi.min, pi.max, 0.0);
            if (pi.logScale && pi.min > 0.0f) slider.setSkewFactorFromMidPoint (std::sqrt (pi.min * pi.max));
            slider.setDoubleClickReturnValue (true, pi.def);
            slider.textFromValueFunction = [this] (double v)
            {
                const float mag = std::abs ((float) v);
                const int decimals = mag >= 100.0f ? 0 : (mag >= 10.0f ? 1 : 2);
                return String (v, decimals) + (String (pi.unit).isNotEmpty() ? " " + String (pi.unit) : String());
            };
            slider.onDragStart = [this] { if (attachment) attachment->beginGesture(); };
            slider.onDragEnd = [this] { if (attachment) attachment->endGesture(); };
            slider.onValueChange = [this]
            {
                if (attachment && ! updating) attachment->setValueAsPartOfGesture (pi.toNormalised ((float) slider.getValue()));
            };
            slider.setTooltip (String (pi.name) + " (double-click to reset)");
            slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
            slider.updateText();
            addAndMakeVisible (slider);
        }
        attachment = std::make_unique<ParameterAttachment> (param, [this] (float norm)
        {
            const float v = pi.fromNormalised (norm);
            updating = true;
            if (pi.numChoices > 0) box.setSelectedId (roundToInt (v) + 1, dontSendNotification);
            else { slider.setValue (v, dontSendNotification); slider.updateText(); }
            updating = false;
        });
        attachment->sendInitialUpdate();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        caption.setBounds (r.removeFromTop (16));
        if (pi.numChoices > 0) box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 26));
        else slider.setBounds (r);
    }

private:
    vf::fx::ParamInfo pi;
    Label caption;
    Slider slider;
    ComboBox box;
    std::unique_ptr<ParameterAttachment> attachment;
    bool updating = false;
};

EffectEditor::EffectEditor (ViridianForgeProcessor& p) : processor (p)
{
    for (auto* b : { &typeButton, &duplicateButton, &removeButton, &loadNamButton })
        addAndMakeVisible (b);
    namLabel.setFont (juce::Font (13.0f));
    namLabel.setMinimumHorizontalScale (0.6f);
    addChildComponent (namLabel);
    loadNamButton.setVisible (false);
    loadNamButton.onClick = [this] { chooseNamFile(); };
    {
        NamProfileBackend be;
        be.pathAt = [this] (int i) { return processor.getSlotNamProfilePath (slot, i); };
        be.activeIndex = [this] { return processor.getSlotNamActiveProfile (slot); };
        be.select = [this] (int i) { processor.selectSlotNamProfile (slot, i); };
        be.assign = [this] (int i, const juce::File& f) { processor.setSlotNamProfile (slot, i, f); };
        be.clearActive = [this]
        {
            const int a = processor.getSlotNamActiveProfile (slot);
            if (a >= 0) processor.clearSlotNamProfile (slot, a);
        };
        namProfiles = std::make_unique<NamProfileBar> (std::move (be));
        addChildComponent (*namProfiles);
    }
    typeButton.setTooltip ("Swap this block for another effect");
    typeButton.onClick = [this]
    {
        PopupMenu change;
        const auto& types = vf::fx::effectTypes();
        for (int c = 0; c < static_cast<int> (vf::fx::FxCategory::count); ++c)
        {
            PopupMenu sub;
            for (size_t i = 0; i < types.size(); ++i)
                if (static_cast<int> (types[i].category) == c)
                    sub.addItem (static_cast<int> (i) + 1, types[i].name, true, static_cast<int> (i) == processor.getSlotType (slot));
            change.addSubMenu (vf::fx::categoryName (static_cast<vf::fx::FxCategory> (c)), sub);
        }
        // (a named local: MSVC mis-resolves `this` inside a nested init-capture)
        SafePointer<EffectEditor> safeThis (this);
        change.showMenuAsync (PopupMenu::Options().withTargetComponent (&typeButton),
            [safeThis] (int r) { if (safeThis != nullptr && r > 0) safeThis->processor.changeEffectType (safeThis->slot, r - 1); });
    };
    duplicateButton.onClick = [this] { processor.duplicateEffect (slot); };
    loadNamButton.setTooltip ("Choose a .nam capture for this block");
    removeButton.onClick = [this] { processor.removeBlock (slot); };
    processor.addChangeListener (this);
}

EffectEditor::~EffectEditor()
{
    processor.removeChangeListener (this);
}

void EffectEditor::setSlot (int s)
{
    if (s == slot && processor.getSlotType (s) == builtType) return;
    slot = s;
    rebuild();
}

void EffectEditor::changeListenerCallback (ChangeBroadcaster*)
{
    if (slot >= 0 && processor.getSlotType (slot) != builtType) rebuild();
}

void EffectEditor::chooseNamFile()
{
    namChooser = std::make_unique<juce::FileChooser> ("Choose a NAM capture", juce::File(), "*.nam");
    SafePointer<EffectEditor> safeThis (this);
    namChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (safeThis != nullptr && file.existsAsFile())
                safeThis->processor.loadSlotNamFile (safeThis->slot, file);
        });
}

void EffectEditor::rebuild()
{
    controls.clear();
    onWatch.reset();
    builtType = processor.getSlotType (slot);
    const bool isNam = builtType >= 0 && vf::fx::effectTypes()[(size_t) builtType].id == "nam.loader";
    loadNamButton.setVisible (isNam);
    namLabel.setVisible (isNam);
    if (namProfiles) { namProfiles->setVisible (isNam); namProfiles->refresh(); }
    if (isNam)
    {
        const auto path = processor.slotNamPath (slot);
        namLabel.setText (path.isEmpty() ? juce::String ("no capture loaded") : juce::File (path).getFileName(), juce::dontSendNotification);
    }
    if (slot >= 0 && builtType >= 0)
    {
        const auto& pl = ViridianForgeProcessor::typeParams (builtType);
        auto& st = processor.getState();
        for (int p = 0; p < pl.count; ++p)
            if (auto* param = st.getParameter (ViridianForgeProcessor::slotParamId (slot, p)))
                addAndMakeVisible (controls.add (new Control (*param, pl.p[(size_t) p])));
        if (auto* on = st.getParameter (ViridianForgeProcessor::slotOnId (slot)))
        {
            onWatch = std::make_unique<ParameterAttachment> (*on, [this] (float v) { isOn = v > 0.5f; repaint(); });
            onWatch->sendInitialUpdate();
        }
    }
    resized();
    repaint();
}

Rectangle<float> EffectEditor::enclosure() const
{
    auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.04f, getHeight() * 0.06f).withTrimmedTop (30.0f);
    return r;
}

Rectangle<float> EffectEditor::footswitch() const
{
    auto e = enclosure();
    const float d = jmin (54.0f, e.getHeight() * 0.18f);
    return { e.getCentreX() - d / 2, e.getBottom() - d - 14.0f, d, d };
}

void EffectEditor::resized()
{
    auto top = getLocalBounds().reduced (getWidth() / 25, 6).removeFromTop (28);
    removeButton.setBounds (top.removeFromRight (84).reduced (2, 2));
    duplicateButton.setBounds (top.removeFromRight (96).reduced (2, 2));
    typeButton.setBounds (top.removeFromRight (84).reduced (2, 2));
    const bool isNam = loadNamButton.isVisible();
    if (isNam)
    {
        loadNamButton.setBounds (top.removeFromLeft (110).reduced (2, 2));
        namLabel.setBounds (top.reduced (4, 2));
    }

    auto e = enclosure().toNearestInt();
    auto area = e.reduced (e.getWidth() / 16, 0).withTrimmedTop (e.getHeight() * 30 / 100).withTrimmedBottom (e.getHeight() * 30 / 100);
    if (isNam && namProfiles != nullptr)
        namProfiles->setBounds (area.removeFromLeft (120));   // captures knob sits left of the controls
    const int n = controls.size();
    if (n == 0) return;
    const int perRow = n <= 5 ? n : (n + 1) / 2;
    const int rows = (n + perRow - 1) / perRow;
    const int cw = jmin (130, area.getWidth() / perRow), rh = area.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        const int inRow = jmin (perRow, n - row * perRow);
        const int x0 = area.getCentreX() - inRow * cw / 2;
        controls[i]->setBounds (x0 + col * cw, area.getY() + row * rh, cw, rh);
    }
}

void EffectEditor::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    g.setColour (t.panel.darker (0.1f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    if (slot < 0 || builtType < 0)
    {
        g.setColour (t.textDim);
        g.setFont (ViridianLookAndFeel::labelFont (14.0f, false));
        g.drawText ("Select a block in the signal chain to edit it.", getLocalBounds(), Justification::centred);
        return;
    }
    const auto& type = vf::fx::effectTypes()[(size_t) builtType];
    const Colour c = Pedalboard::categoryColour (type.category);

    g.setColour (t.textDim);
    g.setFont (ViridianLookAndFeel::labelFont (11.0f));
    g.drawText ("FX " + String (slot + 1) + "  -  " + String (vf::fx::categoryName (type.category)).toUpperCase(),
                getLocalBounds().reduced (getWidth() / 25, 6).removeFromTop (28), Justification::centredLeft);

    // Stompbox enclosure
    auto e = enclosure();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (e.translated (0.0f, 5.0f), 16.0f);
    g.setGradientFill (ColourGradient (c.withMultipliedBrightness (isOn ? 1.0f : 0.5f), e.getX(), e.getY(),
                                       c.withMultipliedBrightness (isOn ? 0.55f : 0.3f), e.getX(), e.getBottom(), false));
    g.fillRoundedRectangle (e, 16.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (e.reduced (3.0f), 13.0f, 1.2f);

    // Knob deck
    auto deck = e.reduced (e.getWidth() / 20, 0).withTrimmedTop (e.getHeight() * 0.26f).withHeight (e.getHeight() * 0.46f);
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (deck, 10.0f);

    // Name plate
    auto title = e.reduced (20.0f, 0.0f).withHeight (e.getHeight() * 0.24f);
    g.setColour (Colours::white);
    g.setFont (ViridianLookAndFeel::labelFont (jmin (30.0f, title.getHeight() * 0.42f)).withExtraKerningFactor (0.08f));
    g.drawFittedText (String (type.name).toUpperCase(), title.withTrimmedBottom (title.getHeight() * 0.3f).toNearestInt(), Justification::centredBottom, 1);
    g.setColour (Colours::white.withAlpha (0.7f));
    g.setFont (ViridianLookAndFeel::labelFont (11.0f, false));
    g.drawFittedText ("inspired by " + String (type.inspiredBy), title.withTrimmedTop (title.getHeight() * 0.72f).toNearestInt(), Justification::centredTop, 1);

    // Footswitch + LED
    auto fsw = footswitch();
    ViridianLookAndFeel::drawLed (g, Rectangle<float> (12.0f, 12.0f).withCentre ({ fsw.getCentreX(), fsw.getY() - 14.0f }), Colour (0xffff3b30), isOn);
    g.setGradientFill (ColourGradient (Colour (0xffd8d8d8), fsw.getX(), fsw.getY(), Colour (0xff6c6c6c), fsw.getRight(), fsw.getBottom(), false));
    g.fillEllipse (fsw);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (fsw, 2.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.setFont (ViridianLookAndFeel::labelFont (10.0f));
    g.drawText (isOn ? "ON" : "BYPASS", fsw.withY (fsw.getBottom() + 1.0f).withHeight (14.0f).expanded (30.0f, 0.0f), Justification::centred);
}

void EffectEditor::mouseUp (const MouseEvent& e)
{
    if (slot >= 0 && footswitch().expanded (6.0f).contains (e.position))
        if (auto* on = processor.getState().getParameter (ViridianForgeProcessor::slotOnId (slot)))
        {
            on->beginChangeGesture();
            on->setValueNotifyingHost (on->getValue() > 0.5f ? 0.0f : 1.0f);
            on->endChangeGesture();
        }
}
