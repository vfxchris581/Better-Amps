// Viridian Forge — Neural Amp Model library.
//
// The user's collection of .nam captures: importing (single files, many files, whole
// folders), an index with each capture's metadata, favourites, custom categories,
// recently used, search, sorting, replacing and removing.
//
// Files are copied into the library folder on import; the user's originals are only
// ever read. Removing a model deletes the library's own copy, never a file outside
// the library folder. The index lives next to the models in "library.json" and is
// written atomically. Not thread-safe: use it from one (non-real-time) thread.
#pragma once

#include "vf/nam/NamEngine.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vf::nam
{

/** Whether a capture already contains a cabinet: decides if the app's cab runs after it. */
enum class CabMode { Auto = 0, AmpOnly = 1, IncludesCab = 2 };

struct LibraryEntry
{
    std::string id;            // content hash (hex): the same capture imported twice is one entry
    std::string file;          // the library's copy (absolute path)
    std::string originalPath;  // where it was imported from (never modified)
    ModelInfo info;            // read from the file

    bool favorite = false;
    std::string category;      // user category, empty = none
    // User overrides; empty = use the file's metadata.
    std::string manufacturer, artist, ampType, gainCategory, displayNameOverride;
    CabMode cabMode = CabMode::Auto;
    std::int64_t addedAt = 0, lastUsedAt = 0; // seconds since 1970, 0 = never

    std::string displayName() const;
    std::string manufacturerName() const;   // override, else gear make
    std::string artistName() const;         // override, else modeled_by
    std::string ampTypeName() const;        // override, else capture kind ("Amp", "Preamp", "Amp + Cab"...)
    std::string gainCategoryName() const;   // override, else from tone_type
    /** True when the app's cabinet should be bypassed after this capture. */
    bool includesCab() const noexcept;
};

enum class SortKey { Name = 0, Manufacturer, AmpType, GainCategory, Artist, Recent, DateAdded };
const char* sortKeyName (SortKey) noexcept;
inline constexpr int kNumSortKeys = 7;

struct LibraryQuery
{
    std::string text;              // every word must match (name, gear, artist, type, gain, category, file name)
    bool favoritesOnly = false;
    bool recentOnly = false;       // only models used at least once, newest first unless sorted otherwise
    std::string category;          // empty = all
    SortKey sort = SortKey::Name;
    bool descending = false;
};

struct ImportReport
{
    std::vector<std::string> added;                                 // entry ids
    std::vector<std::string> duplicates;                            // entry ids already in the library
    std::vector<std::pair<std::string, std::string>> failed;        // source path, reason
};

class ModelLibrary
{
public:
    /** `folder` is created if needed. Call load() before use. */
    explicit ModelLibrary (std::string folder);

    /** Reads the index, then rescans the folder. Returns false only if the folder is unusable. */
    bool load();
    /** Writes the index (atomically). */
    bool save() const;
    /** Picks up .nam files placed in the folder by hand; drops entries whose file has gone. */
    void rescan();

    ImportReport importFile (const std::string& path);
    ImportReport importFiles (const std::vector<std::string>& paths);
    /** Imports every .nam file in a folder (and its subfolders when `recursive`). */
    ImportReport importFolder (const std::string& folder, bool recursive = true);

    /** Swaps an entry's capture for another file, keeping favourite, category and overrides. */
    bool replaceModel (const std::string& id, const std::string& newSource, std::string& error);
    /** Removes an entry and deletes the library's copy (only ever a file inside the library folder). */
    bool remove (const std::string& id);

    void setFavorite (const std::string& id, bool favorite);
    void setCategory (const std::string& id, const std::string& category);
    void setCabMode (const std::string& id, CabMode mode);
    enum class Field { Name, Manufacturer, Artist, AmpType, GainCategory };
    void setOverride (const std::string& id, Field field, const std::string& value);
    /** Records that a model was loaded (for Recent). `when` = 0 uses the current time. */
    void markUsed (const std::string& id, std::int64_t when = 0);

    const LibraryEntry* find (const std::string& id) const;
    /** Finds by the library copy's path or the path it was imported from. */
    const LibraryEntry* findByPath (const std::string& path) const;
    std::vector<const LibraryEntry*> query (const LibraryQuery&) const;
    /** User categories in use, sorted. */
    std::vector<std::string> categories() const;
    std::vector<const LibraryEntry*> recent (std::size_t maxCount = 20) const;

    std::size_t size() const noexcept { return entries.size(); }
    const std::string& folder() const noexcept { return dir; }
    std::string indexPath() const;

    /** Hex content hash of a file (empty if unreadable). */
    static std::string hashFile (const std::string& path);

private:
    LibraryEntry* findMutable (const std::string& id);
    bool addFile (const std::string& source, ImportReport& report);
    std::string uniqueDestination (const std::string& fileName) const;
    bool isInsideLibrary (const std::string& path) const;

    std::string dir;
    std::vector<LibraryEntry> entries;
};

} // namespace vf::nam
