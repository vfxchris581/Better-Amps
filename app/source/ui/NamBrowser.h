// Neural Amp Model browser: the NAM section of the amp browser.
#pragma once

#include <JuceHeader.h>

#if VF_WITH_NAM
#include "vf/nam/ModelLibrary.h"
#include <functional>
#include <vector>

/** Full-window overlay over the editor for the user's NAM library: search, sort (name, manufacturer,
    amp type, gain, artist, recent, date added), filters (all, favourites, recent, categories),
    import files / a folder, replace, remove, favourites, categories, editing the shown details,
    whether a capture contains a cab, and every piece of metadata the file carries.
    Double-click (or LOAD) loads the selected capture into whoever opened the browser. */
class NamBrowser : public juce::Component,
                   private juce::TableListBoxModel
{
public:
    NamBrowser();
    ~NamBrowser() override;

    /** Opens the browser; `onPick` receives the chosen capture's library file. `targetName` is shown
        in the title ("Amp 1", "FX 3 - NAM Loader"). `currentFile` is highlighted if it is in the library. */
    void open (const juce::String& targetName, const juce::File& currentFile, double hostSampleRate,
               std::function<void (const juce::File&)> onPick);
    void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int w, int h, bool selected) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void sortOrderChanged (int columnId, bool forwards) override;
    void returnKeyPressed (int lastRowSelected) override;

    enum Column { colFavorite = 1, colName, colMaker, colType, colGain, colArtist, colRate, colCab };

    void refresh (bool keepSelection = true);
    void rebuildFilterBox();
    const vf::nam::LibraryEntry* selected() const;
    void loadSelected();
    void importFiles();
    void importFolder();
    void replaceSelected();
    void removeSelected();
    void editSelected();
    void showReport (const vf::nam::ImportReport&);
    void persist();
    juce::String infoText (const vf::nam::LibraryEntry&) const;
    juce::Rectangle<int> card() const;

    juce::Label title, infoTitle;
    juce::TextEditor search, info;
    juce::ComboBox sortBox, filterBox, cabBox;
    juce::TextButton orderButton { "A-Z" }, loadButton { "LOAD" }, importButton { "IMPORT FILES..." },
        importFolderButton { "IMPORT FOLDER..." }, favoriteButton { "FAVORITE" }, categoryButton { "CATEGORY..." },
        editButton { "EDIT INFO..." }, replaceButton { "REPLACE..." }, removeButton { "REMOVE" },
        folderButton { "SHOW FOLDER" }, closeButton { "CLOSE" };
    juce::Label countLabel, statusLabel;
    juce::TableListBox table { "NAM models", this };

    std::vector<std::string> rows;     // entry ids in display order
    std::vector<std::string> filterCategories;
    vf::nam::LibraryQuery query;
    std::function<void (const juce::File&)> pick;
    juce::String target;
    double hostRate = 48000.0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamBrowser)
};
#endif
