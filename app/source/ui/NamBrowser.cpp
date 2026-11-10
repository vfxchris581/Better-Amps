#include "NamBrowser.h"

#if VF_WITH_NAM
#include "ViridianLookAndFeel.h"
#include "../PluginProcessor.h"

using namespace juce;
using vf::nam::LibraryEntry;

namespace
{
VFTheme themeOf (Component& c)
{
    if (auto* l = dynamic_cast<ViridianLookAndFeel*> (&c.getLookAndFeel())) return l->getTheme();
    return VFTheme::dark();
}

String str (const std::string& s) { return String::fromUTF8 (s.c_str()); }
std::string std8 (const String& s) { return s.toStdString(); }

String fmtDb (double v) { return String (v, 1) + " dB"; }
String fmtRate (double v)
{
    if (v <= 0) return "?";
    auto s = String (v / 1000.0, 1);
    if (s.endsWith (".0")) s = s.dropLastCharacters (2);
    return s + " kHz";
}

vf::nam::ModelLibrary& lib() { return ViridianForgeProcessor::namLibrary(); }

enum FilterId { filterAll = 1, filterFavorites = 2, filterRecent = 3, filterCategoryBase = 100 };
}

//==============================================================================
NamBrowser::NamBrowser()
{
    setWantsKeyboardFocus (true);

    title.setFont (ViridianLookAndFeel::labelFont (20.0f));
    infoTitle.setText ("MODEL INFORMATION", dontSendNotification);
    infoTitle.setFont (ViridianLookAndFeel::labelFont (11.0f));
    for (auto* l : { &title, &infoTitle, &countLabel, &statusLabel }) addAndMakeVisible (l);
    statusLabel.setFont (ViridianLookAndFeel::labelFont (11.0f, false));
    countLabel.setFont (ViridianLookAndFeel::labelFont (11.0f, false));
    countLabel.setJustificationType (Justification::centredRight);

    search.setTextToShowWhenEmpty ("Search name, gear, artist, type, gain, category...", Colours::grey);
    search.onTextChange = [this] { query.text = std8 (search.getText()); refresh(); };
    search.onReturnKey = [this] { loadSelected(); };
    search.onEscapeKey = [this] { if (search.isEmpty()) close(); else search.clear(); };
    addAndMakeVisible (search);

    for (int k = 0; k < vf::nam::kNumSortKeys; ++k)
        sortBox.addItem ("Sort: " + String (vf::nam::sortKeyName (static_cast<vf::nam::SortKey> (k))), k + 1);
    sortBox.setSelectedId (1, dontSendNotification);
    sortBox.onChange = [this]
    {
        query.sort = static_cast<vf::nam::SortKey> (sortBox.getSelectedId() - 1);
        refresh();
    };
    addAndMakeVisible (sortBox);

    orderButton.setTooltip ("Ascending / descending");
    orderButton.onClick = [this]
    {
        query.descending = ! query.descending;
        orderButton.setButtonText (query.descending ? "Z-A" : "A-Z");
        refresh();
    };
    addAndMakeVisible (orderButton);

    filterBox.onChange = [this]
    {
        const int id = filterBox.getSelectedId();
        query.favoritesOnly = id == filterFavorites;
        query.recentOnly = id == filterRecent;
        query.category = id >= filterCategoryBase && id - filterCategoryBase < (int) filterCategories.size()
                             ? filterCategories[static_cast<size_t> (id - filterCategoryBase)] : std::string();
        refresh();
    };
    addAndMakeVisible (filterBox);

    auto& header = table.getHeader();
    const int colFlags = TableHeaderComponent::visible | TableHeaderComponent::sortable | TableHeaderComponent::resizable;
    header.addColumn (CharPointer_UTF8 ("\xe2\x98\x85"), colFavorite, 28, 28, 28, TableHeaderComponent::visible);
    header.addColumn ("Name", colName, 210, 80, 600, colFlags);
    header.addColumn ("Manufacturer", colMaker, 150, 60, 400, colFlags);
    header.addColumn ("Type", colType, 110, 60, 200, colFlags);
    header.addColumn ("Gain", colGain, 80, 50, 140, colFlags);
    header.addColumn ("Artist", colArtist, 110, 50, 300, colFlags);
    header.addColumn ("Rate", colRate, 64, 50, 100, TableHeaderComponent::visible | TableHeaderComponent::resizable);
    header.addColumn ("Cab", colCab, 44, 40, 80, TableHeaderComponent::visible | TableHeaderComponent::resizable);
    header.setSortColumnId (colName, true);
    table.setRowHeight (24);
    table.setMultipleSelectionEnabled (false);
    addAndMakeVisible (table);

    info.setMultiLine (true);
    info.setReadOnly (true);
    info.setScrollbarsShown (true);
    info.setCaretVisible (false);
    info.setFont (ViridianLookAndFeel::labelFont (12.5f, false));
    addAndMakeVisible (info);

    cabBox.addItem ("Cab: Auto (from file)", 1);
    cabBox.addItem ("Cab: Amp only - use the app's cab", 2);
    cabBox.addItem ("Cab: Includes cab - bypass the app's cab", 3);
    cabBox.setTooltip ("Whether this capture already has a cabinet (and mic) in it. Captures that do bypass the app's cab "
                       "automatically, so the cab is never applied twice.");
    cabBox.onChange = [this]
    {
        if (auto* e = selected())
        {
            lib().setCabMode (e->id, static_cast<vf::nam::CabMode> (cabBox.getSelectedId() - 1));
            persist();
            refresh();
        }
    };
    addAndMakeVisible (cabBox);

    loadButton.onClick = [this] { loadSelected(); };
    importButton.onClick = [this] { importFiles(); };
    importFolderButton.onClick = [this] { importFolder(); };
    favoriteButton.onClick = [this]
    {
        if (auto* e = selected()) { lib().setFavorite (e->id, ! e->favorite); persist(); refresh(); }
    };
    categoryButton.onClick = [this]
    {
        auto* e = selected();
        if (e == nullptr) return;
        PopupMenu m;
        const auto cats = lib().categories();
        for (size_t i = 0; i < cats.size(); ++i) m.addItem (static_cast<int> (i) + 10, str (cats[i]), true, cats[i] == e->category);
        if (! cats.empty()) m.addSeparator();
        m.addItem (1, "New category...");
        m.addItem (2, "No category", ! e->category.empty());
        const auto id = e->id;
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&categoryButton), [this, id, cats] (int r)
        {
            if (r >= 10 && r - 10 < (int) cats.size()) { lib().setCategory (id, cats[static_cast<size_t> (r - 10)]); persist(); rebuildFilterBox(); refresh(); }
            else if (r == 2) { lib().setCategory (id, {}); persist(); rebuildFilterBox(); refresh(); }
            else if (r == 1)
            {
                auto* w = new AlertWindow ("New category", "Name the category:", MessageBoxIconType::NoIcon, this);
                w->addTextEditor ("name", {});
                w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
                w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
                SafePointer<NamBrowser> safe (this);
                w->enterModalState (true, ModalCallbackFunction::create ([safe, w, id] (int ok)
                {
                    const auto name = w->getTextEditorContents ("name").trim();
                    if (safe == nullptr || ok == 0 || name.isEmpty()) return;
                    lib().setCategory (id, std8 (name));
                    safe->persist();
                    safe->rebuildFilterBox();
                    safe->refresh();
                }), true);
            }
        });
    };
    editButton.onClick = [this] { editSelected(); };
    replaceButton.onClick = [this] { replaceSelected(); };
    removeButton.onClick = [this] { removeSelected(); };
    folderButton.onClick = [] { File (str (lib().folder())).revealToUser(); };
    closeButton.onClick = [this] { close(); };

    loadButton.setTooltip ("Load the selected capture (or double-click it)");
    importButton.setTooltip ("Copy one or more .nam files into the library. Your originals are not changed.");
    importFolderButton.setTooltip ("Copy every .nam file in a folder (and its subfolders) into the library");
    replaceButton.setTooltip ("Swap this entry's capture for another file, keeping its favourite, category and details");
    removeButton.setTooltip ("Remove from the library (deletes the library's copy only, never your original)");
    editButton.setTooltip ("Edit the name, manufacturer, artist, amp type or gain shown for this capture");

    for (auto* b : { &orderButton, &loadButton, &importButton, &importFolderButton, &favoriteButton, &categoryButton,
                     &editButton, &replaceButton, &removeButton, &folderButton, &closeButton })
        addAndMakeVisible (b);
}

NamBrowser::~NamBrowser() = default;

//==============================================================================
void NamBrowser::open (const String& targetName, const File& currentFile, double hostSampleRate, std::function<void (const File&)> onPick)
{
    target = targetName;
    hostRate = hostSampleRate > 0 ? hostSampleRate : 48000.0;
    pick = std::move (onPick);
    title.setText ("NEURAL AMP MODELS  -  " + target.toUpperCase(), dontSendNotification);

    lib().rescan(); // pick up files added or removed outside the app
    persist();
    rebuildFilterBox();
    refresh (false);

    if (const auto* cur = lib().findByPath (currentFile.getFullPathName().toStdString()))
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i] == cur->id) { table.selectRow (static_cast<int> (i)); break; }

    statusLabel.setText (lib().size() == 0 ? "Your library is empty: IMPORT FILES or IMPORT FOLDER to add captures." : String(),
                         dontSendNotification);
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void NamBrowser::close()
{
    setVisible (false);
    pick = nullptr;
}

void NamBrowser::persist()
{
    ViridianForgeProcessor::saveNamLibrary();
}

void NamBrowser::rebuildFilterBox()
{
    const int previous = filterBox.getSelectedId();
    const auto previousCategory = query.category;
    filterBox.clear (dontSendNotification);
    filterBox.addItem ("All models", filterAll);
    filterBox.addItem ("Favorites", filterFavorites);
    filterBox.addItem ("Recent", filterRecent);
    filterCategories = lib().categories();
    if (! filterCategories.empty()) filterBox.addSeparator();
    int selectId = previous == 0 ? filterAll : previous;
    for (size_t i = 0; i < filterCategories.size(); ++i)
    {
        const int id = filterCategoryBase + static_cast<int> (i);
        filterBox.addItem ("Category: " + str (filterCategories[i]), id);
        if (filterCategories[i] == previousCategory) selectId = id;
    }
    if (selectId >= filterCategoryBase && query.category.empty()) selectId = filterAll;
    filterBox.setSelectedId (selectId, dontSendNotification);
    if (filterBox.getSelectedId() == 0) filterBox.setSelectedId (filterAll, dontSendNotification);
    query.favoritesOnly = filterBox.getSelectedId() == filterFavorites;
    query.recentOnly = filterBox.getSelectedId() == filterRecent;
    if (filterBox.getSelectedId() < filterCategoryBase) query.category.clear();
}

void NamBrowser::refresh (bool keepSelection)
{
    std::string keep;
    if (keepSelection) if (auto* e = selected()) keep = e->id;

    rows.clear();
    for (const auto* e : lib().query (query)) rows.push_back (e->id);
    table.updateContent();
    table.repaint();

    int row = -1;
    for (size_t i = 0; i < rows.size(); ++i) if (rows[i] == keep) { row = static_cast<int> (i); break; }
    if (row < 0 && ! rows.empty() && keepSelection) row = 0;
    if (row >= 0) table.selectRow (row, false, true);
    else table.deselectAllRows();
    selectedRowsChanged (row);

    countLabel.setText (String ((int) rows.size()) + " of " + String ((int) lib().size()) + " models", dontSendNotification);
}

const LibraryEntry* NamBrowser::selected() const
{
    const int r = table.getSelectedRow();
    if (r < 0 || r >= (int) rows.size()) return nullptr;
    return lib().find (rows[static_cast<size_t> (r)]);
}

//==============================================================================
int NamBrowser::getNumRows() { return static_cast<int> (rows.size()); }

void NamBrowser::paintRowBackground (Graphics& g, int row, int, int, bool isSelected)
{
    const auto t = themeOf (*this);
    if (isSelected) g.fillAll (t.accent.withAlpha (0.35f));
    else if (row % 2) g.fillAll (Colours::white.withAlpha (0.03f));
}

void NamBrowser::paintCell (Graphics& g, int row, int column, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto* e = lib().find (rows[static_cast<size_t> (row)]);
    if (e == nullptr) return;
    const auto t = themeOf (*this);
    String text;
    Colour colour = t.text;
    switch (column)
    {
        case colFavorite: text = e->favorite ? CharPointer_UTF8 ("\xe2\x98\x85") : CharPointer_UTF8 ("\xe2\x98\x86"); colour = e->favorite ? t.accent : t.textDim; break;
        case colName:     text = str (e->displayName()); break;
        case colMaker:    text = str (e->manufacturerName()); break;
        case colType:     text = str (e->ampTypeName()); break;
        case colGain:     text = str (e->gainCategoryName()); break;
        case colArtist:   text = str (e->artistName()); break;
        case colRate:
        {
            text = fmtRate (e->info.sampleRate);
            if (e->info.sampleRate > 0 && std::abs (e->info.sampleRate - hostRate) > 0.5) colour = t.textDim; // will be resampled
            break;
        }
        case colCab:      text = e->includesCab() ? "Yes" : "-"; colour = e->includesCab() ? t.accent : t.textDim; break;
        default: break;
    }
    if (column != colName && column != colFavorite) colour = colour.withMultipliedAlpha (0.85f);
    g.setColour (colour);
    g.setFont (ViridianLookAndFeel::labelFont (column == colName ? 13.0f : 12.0f, column == colName));
    g.drawText (text, Rectangle<int> (6, 0, w - 8, h), column == colFavorite ? Justification::centred : Justification::centredLeft, true);
}

void NamBrowser::cellClicked (int row, int column, const MouseEvent&)
{
    if (column != colFavorite || row < 0 || row >= (int) rows.size()) return;
    if (const auto* e = lib().find (rows[static_cast<size_t> (row)]))
    {
        lib().setFavorite (e->id, ! e->favorite);
        persist();
        refresh();
    }
}

void NamBrowser::cellDoubleClicked (int row, int column, const MouseEvent&)
{
    if (column == colFavorite) return;
    table.selectRow (row);
    loadSelected();
}

void NamBrowser::returnKeyPressed (int) { loadSelected(); }

void NamBrowser::sortOrderChanged (int columnId, bool forwards)
{
    using vf::nam::SortKey;
    SortKey key = SortKey::Name;
    switch (columnId)
    {
        case colMaker:  key = SortKey::Manufacturer; break;
        case colType:   key = SortKey::AmpType; break;
        case colGain:   key = SortKey::GainCategory; break;
        case colArtist: key = SortKey::Artist; break;
        default:        key = SortKey::Name; break;
    }
    query.sort = key;
    query.descending = ! forwards;
    sortBox.setSelectedId (static_cast<int> (key) + 1, dontSendNotification);
    orderButton.setButtonText (query.descending ? "Z-A" : "A-Z");
    refresh();
}

void NamBrowser::selectedRowsChanged (int)
{
    const auto* e = selected();
    for (auto* b : { &loadButton, &favoriteButton, &categoryButton, &editButton, &replaceButton, &removeButton })
        b->setEnabled (e != nullptr);
    cabBox.setEnabled (e != nullptr);
    if (e == nullptr) { info.setText ({}); return; }
    favoriteButton.setButtonText (e->favorite ? "UNFAVORITE" : "FAVORITE");
    cabBox.setSelectedId (static_cast<int> (e->cabMode) + 1, dontSendNotification);
    info.setText (infoText (*e), false);
}

String NamBrowser::infoText (const LibraryEntry& e) const
{
    const auto& m = e.info;
    String s;
    auto line = [&s] (const String& k, const String& v) { if (v.isNotEmpty()) s << k << ":  " << v << "\n"; };
    line ("Name", str (e.displayName()));
    line ("Manufacturer", str (e.manufacturerName()));
    if (! e.manufacturer.empty() && ! m.gearName().empty()) line ("Gear (file)", str (m.gearName()));
    else line ("Gear model", str (m.gearModel));
    line ("Amp type", str (e.ampTypeName()));
    line ("Gain", str (e.gainCategoryName()));
    line ("Captured by", str (e.artistName()));
    line ("Category", str (e.category));
    s << "\n";
    line ("Contains a cab", e.includesCab() ? "Yes - the app's cab is bypassed after it" : "No - the app's cab runs after it");
    if (e.cabMode != vf::nam::CabMode::Auto) s << "  (set by you; the file says " << (m.includesCab() ? "it has a cab" : "amp only") << ")\n";
    s << "\n";
    line ("Architecture", str (m.architecture));
    line ("Format version", str (m.version));
    if (m.sampleRate > 0)
    {
        String rate = fmtRate (m.sampleRate);
        rate << (std::abs (m.sampleRate - hostRate) > 0.5 ? "  (session at " + fmtRate (hostRate) + ": resampled automatically)"
                                                         : "  (matches the session: runs natively)");
        line ("Sample rate", rate);
    }
    else line ("Sample rate", "not stated (assumed 48 kHz)");
    line ("Channels", String (m.numInputs) + " in / " + String (m.numOutputs) + " out");
    line ("Input level", m.hasInputLevel ? String (m.inputLevelDbu, 1) + " dBu at 0 dBFS (used by input calibration)" : String ("not stated (input calibration has no effect)"));
    line ("Output level", m.hasOutputLevel ? String (m.outputLevelDbu, 1) + " dBu at 0 dBFS" : String());
    line ("Loudness", m.hasLoudness ? fmtDb (m.loudnessDb) + " (normalised to -18 dB when NORMALIZE is on)" : String ("not stated"));
    s << "\n";
    line ("Library file", str (e.file));
    line ("Imported from", str (e.originalPath));
    if (e.lastUsedAt > 0) line ("Last used", Time (e.lastUsedAt * 1000).toString (true, true, false));
    if (e.addedAt > 0) line ("Added", Time (e.addedAt * 1000).toString (true, true, false));
    return s;
}

//==============================================================================
void NamBrowser::loadSelected()
{
    const auto* e = selected();
    if (e == nullptr || ! pick) return;
    const File f (str (e->file));
    auto cb = pick;
    close();
    cb (f); // the processor records it as recently used
}

void NamBrowser::showReport (const vf::nam::ImportReport& r)
{
    String s;
    s << (int) r.added.size() << " imported";
    if (! r.duplicates.empty()) s << ", " << (int) r.duplicates.size() << " already in the library";
    if (! r.failed.empty())
    {
        s << ", " << (int) r.failed.size() << " failed (";
        s << File (str (r.failed.front().first)).getFileName() << ": " << str (r.failed.front().second);
        if (r.failed.size() > 1) s << ", ...";
        s << ")";
    }
    statusLabel.setText (s, dontSendNotification);
}

void NamBrowser::importFiles()
{
    chooser = std::make_unique<FileChooser> ("Import NAM captures", File(), "*.nam");
    SafePointer<NamBrowser> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
        [safe] (const FileChooser& fc)
        {
            if (safe == nullptr) return;
            std::vector<std::string> paths;
            for (const auto& f : fc.getResults()) if (f.existsAsFile()) paths.push_back (f.getFullPathName().toStdString());
            if (paths.empty()) return;
            const auto report = lib().importFiles (paths);
            safe->persist();
            safe->refresh();
            safe->showReport (report);
            if (! report.added.empty())
                for (size_t i = 0; i < safe->rows.size(); ++i)
                    if (safe->rows[i] == report.added.front()) { safe->table.selectRow (static_cast<int> (i)); break; }
        });
}

void NamBrowser::importFolder()
{
    chooser = std::make_unique<FileChooser> ("Import every .nam file in a folder", File());
    SafePointer<NamBrowser> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [safe] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        const auto report = lib().importFolder (dir.getFullPathName().toStdString(), true);
        safe->persist();
        safe->refresh();
        safe->showReport (report);
    });
}

void NamBrowser::replaceSelected()
{
    const auto* e = selected();
    if (e == nullptr) return;
    const auto id = e->id;
    chooser = std::make_unique<FileChooser> ("Replace \"" + str (e->displayName()) + "\" with...", File(), "*.nam");
    SafePointer<NamBrowser> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe, id] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        std::string err;
        if (lib().replaceModel (id, f.getFullPathName().toStdString(), err))
        {
            safe->persist();
            safe->statusLabel.setText ("Replaced with " + f.getFileName() + ". Sessions using the old file keep it until you load the new one.",
                                       dontSendNotification);
        }
        else safe->statusLabel.setText ("Could not replace: " + str (err), dontSendNotification);
        safe->refresh();
    });
}

void NamBrowser::removeSelected()
{
    const auto* e = selected();
    if (e == nullptr) return;
    const auto id = e->id;
    const auto name = str (e->displayName());
    SafePointer<NamBrowser> safe (this);
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Remove capture",
        "Remove \"" + name + "\" from the library?\n\nThe library's copy is deleted. "
        + (e->originalPath.empty() ? String() : "Your original file stays where it is:\n" + str (e->originalPath)),
        "Remove", "Cancel", this, ModalCallbackFunction::create ([safe, id, name] (int ok)
        {
            if (safe == nullptr || ok == 0) return;
            lib().remove (id);
            safe->persist();
            safe->rebuildFilterBox();
            safe->refresh();
            safe->statusLabel.setText ("Removed " + name + ".", dontSendNotification);
        }));
}

void NamBrowser::editSelected()
{
    const auto* e = selected();
    if (e == nullptr) return;
    const auto id = e->id;
    auto* w = new AlertWindow ("Edit details", "Shown in the browser only; the .nam file is not changed.\nLeave a field empty to use the file's own value.",
                               MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", str (e->displayNameOverride), "Name");
    w->addTextEditor ("maker", str (e->manufacturer), "Manufacturer");
    w->addTextEditor ("artist", str (e->artist), "Artist");
    w->addComboBox ("type", { "(from file)", "Amp", "Preamp", "Power Amp", "Pedal", "Pedal + Amp", "Amp + Cab", "Pedal + Amp + Cab", "Full Rig" }, "Amp type");
    w->addComboBox ("gain", { "(from file)", "Clean", "Crunch", "Overdrive", "High Gain", "Fuzz" }, "Gain");
    if (auto* c = w->getComboBoxComponent ("type")) c->setText (e->ampType.empty() ? "(from file)" : str (e->ampType));
    if (auto* c = w->getComboBoxComponent ("gain")) c->setText (e->gainCategory.empty() ? "(from file)" : str (e->gainCategory));
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<NamBrowser> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w, id] (int ok)
    {
        if (safe == nullptr || ok == 0) return;
        using F = vf::nam::ModelLibrary::Field;
        auto fromBox = [w] (const char* name)
        {
            auto* c = w->getComboBoxComponent (name);
            const auto v = c != nullptr ? c->getText() : String();
            return v == "(from file)" ? std::string() : v.toStdString();
        };
        lib().setOverride (id, F::Name, std8 (w->getTextEditorContents ("name").trim()));
        lib().setOverride (id, F::Manufacturer, std8 (w->getTextEditorContents ("maker").trim()));
        lib().setOverride (id, F::Artist, std8 (w->getTextEditorContents ("artist").trim()));
        lib().setOverride (id, F::AmpType, fromBox ("type"));
        lib().setOverride (id, F::GainCategory, fromBox ("gain"));
        safe->persist();
        safe->refresh();
    }), true);
}

//==============================================================================
Rectangle<int> NamBrowser::card() const
{
    return getLocalBounds().reduced (jmax (12, getWidth() / 40), jmax (12, getHeight() / 30));
}

void NamBrowser::paint (Graphics& g)
{
    const auto t = themeOf (*this);
    g.fillAll (Colours::black.withAlpha (0.65f));
    auto c = card().toFloat();
    g.setColour (t.panel);
    g.fillRoundedRectangle (c, 12.0f);
    g.setColour (t.accent);
    g.drawRoundedRectangle (c.reduced (1.0f), 12.0f, 1.5f);

    title.setColour (Label::textColourId, t.accent);
    infoTitle.setColour (Label::textColourId, t.textDim);
    statusLabel.setColour (Label::textColourId, t.text);
    countLabel.setColour (Label::textColourId, t.textDim);
    table.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.25f));
    table.getHeader().setColour (TableHeaderComponent::backgroundColourId, t.panel.darker (0.2f));
    table.getHeader().setColour (TableHeaderComponent::textColourId, t.text);
    info.setColour (TextEditor::backgroundColourId, Colours::black.withAlpha (0.25f));
    info.setColour (TextEditor::textColourId, t.text);
    info.setColour (TextEditor::outlineColourId, t.panelEdge);
}

void NamBrowser::resized()
{
    auto r = card().reduced (18);
    auto head = r.removeFromTop (30);
    closeButton.setBounds (head.removeFromRight (90).reduced (0, 2));
    title.setBounds (head);
    r.removeFromTop (8);

    auto tools = r.removeFromTop (28);
    importFolderButton.setBounds (tools.removeFromRight (140));
    tools.removeFromRight (6);
    importButton.setBounds (tools.removeFromRight (130));
    tools.removeFromRight (12);
    orderButton.setBounds (tools.removeFromRight (48));
    tools.removeFromRight (4);
    sortBox.setBounds (tools.removeFromRight (170));
    tools.removeFromRight (6);
    filterBox.setBounds (tools.removeFromRight (190));
    tools.removeFromRight (6);
    search.setBounds (tools);
    r.removeFromTop (8);

    auto foot = r.removeFromBottom (28);
    folderButton.setBounds (foot.removeFromRight (120));
    foot.removeFromRight (8);
    countLabel.setBounds (foot.removeFromRight (140));
    statusLabel.setBounds (foot);
    r.removeFromBottom (8);

    auto side = r.removeFromRight (jlimit (260, 420, r.getWidth() * 36 / 100));
    r.removeFromRight (12);
    table.setBounds (r);

    infoTitle.setBounds (side.removeFromTop (18));
    auto buttons = side.removeFromBottom (28 * 3 + 2 * 6 + 34);
    info.setBounds (side.withTrimmedBottom (6));

    cabBox.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (6);
    auto row = [&buttons] (std::initializer_list<Button*> bs)
    {
        auto line = buttons.removeFromTop (28);
        buttons.removeFromTop (6);
        const int w = (line.getWidth() - 6 * ((int) bs.size() - 1)) / (int) bs.size();
        for (auto* b : bs) { b->setBounds (line.removeFromLeft (w)); line.removeFromLeft (6); }
    };
    row ({ &loadButton, &favoriteButton });
    row ({ &categoryButton, &editButton });
    row ({ &replaceButton, &removeButton });
}

bool NamBrowser::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { close(); return true; }
    return false;
}

void NamBrowser::mouseUp (const MouseEvent& e)
{
    if (! card().contains (e.getPosition())) close();
}
#endif
