#pragma once

#include <JuceHeader.h>
#include "Widgets.h"
#include "../PluginProcessor.h"

/** Drag-and-drop signal chain.

      IN ─ [pre blocks] ─┬─ [lane A] ─┬─ [post blocks] ─ OUT
                         └─ [lane B] ─┘

    - click a block to edit it; double-click (or click its LED) to bypass
    - drag a block to move it anywhere, including between lanes
    - "+" adds an effect, amp or cab at that point; right-click a block for
      duplicate / change type / move / remove; Delete removes the selection
    - SPLIT creates the parallel lanes; the lane mixer sets level and pan */
class Pedalboard : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::ChangeListener,
                   private juce::Timer
{
public:
    explicit Pedalboard (ViridianForgeProcessor& processor);
    ~Pedalboard() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** The block being edited (key from vf::graph). */
    int getSelected() const noexcept { return selected; }
    void select (int key);
    std::function<void (int key)> onSelectionChanged;

    /** Where a block is drawn (empty if not in the chain); used by UI tests. */
    juce::Rectangle<float> getTileBounds (int key) const
    {
        for (const auto& t : tiles) if (t.key == key) return t.r;
        return {};
    }
    juce::Rectangle<float> getOutputJackBounds() const noexcept { return outJack; }

    static juce::Colour categoryColour (vf::fx::FxCategory c);
    static juce::Colour blockColour (const ViridianForgeProcessor& p, int key);
    static juce::String blockTitle (const ViridianForgeProcessor& p, int key);

    /** Builds the "add block" menu; result ids are decoded by addFromMenu(). */
    static juce::PopupMenu buildAddMenu (const ViridianForgeProcessor& p);
    static void addFromMenu (ViridianForgeProcessor& p, int menuResult, vf::graph::Section section, int index, std::function<void (int)> selectKey);

private:
    using Section = vf::graph::Section;
    struct Tile { int key; Section section; int index; juce::Rectangle<float> r; };
    struct Gap  { Section section; int index; juce::Rectangle<float> r; bool isPlus; };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override { repaint(); }
    void layoutTiles();
    const Tile* tileAt (juce::Point<float> p) const;
    const Gap* plusAt (juce::Point<float> p) const;
    const Gap* nearestGap (juce::Point<float> p) const;
    juce::Rectangle<float> ledArea (const Tile&) const;
    bool isBlockOn (int key) const;
    void toggleBypass (int key);
    void showBlockMenu (const Tile&);
    void updateMixerVisibility();

    ViridianForgeProcessor& processor;
    APVTS& state;
    std::vector<Tile> tiles;
    std::vector<Gap> gaps;
    juce::Rectangle<float> board, inJack, outJack, splitNode, mergeNode;
    float rowA = 0, rowB = 0, rowMain = 0;

    int selected = vf::graph::kAmp1;
    int dragKey = -1;
    bool dragging = false;
    juce::Point<float> dragPos;
    const Gap* hoverGap = nullptr;

    juce::TextButton splitButton { "SPLIT" };
    LabeledKnob levelA, panA, levelB, panB;
    ChoiceBox splitMode;
};

/** Editor for the effect in one slot: drawn as a stompbox with its controls,
    footswitch LED (bypass) and type selector. Rebuilds when the slot's type changes. */
class EffectEditor : public juce::Component, private juce::ChangeListener
{
public:
    explicit EffectEditor (ViridianForgeProcessor& processor);
    ~EffectEditor() override;

    void setSlot (int slot);
    int getSlot() const noexcept { return slot; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    class Control;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuild();
    juce::Rectangle<float> enclosure() const;
    juce::Rectangle<float> footswitch() const;

    ViridianForgeProcessor& processor;
    int slot = -1, builtType = -2;
    juce::OwnedArray<Control> controls;
    std::unique_ptr<juce::ParameterAttachment> onWatch;
    bool isOn = true;
    juce::TextButton typeButton { "CHANGE" }, duplicateButton { "DUPLICATE" }, removeButton { "REMOVE" };
    juce::TextButton loadNamButton { "LOAD .NAM" };
    std::unique_ptr<class NamProfileBar> namProfiles;   // 1-10 capture knob for NAM Loader blocks
    juce::Label namLabel;
    void chooseNamFile();
    std::unique_ptr<juce::FileChooser> namChooser;
};
