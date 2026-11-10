// VFXAamps — Neural Amp Model library (compiled with the NAM core for its JSON parser).

#include "vf/nam/ModelLibrary.h"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace vf::nam
{

namespace
{
std::string lower (std::string s)
{
    for (auto& c : s) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return s;
}

bool isNamFile (const fs::path& p)
{
    return lower (p.extension().string()) == ".nam";
}

std::int64_t now()
{
    using namespace std::chrono;
    return duration_cast<seconds> (system_clock::now().time_since_epoch()).count();
}

std::string canonical (const std::string& path)
{
    std::error_code ec;
    auto c = fs::weakly_canonical (fs::path (path), ec);
    return ec ? fs::path (path).lexically_normal().string() : c.string();
}

// UTF-8 <-> path, so non-ASCII file names survive the index on every OS (C++20 u8 strings).
std::string toUtf8 (const fs::path& p)
{
    const auto u = p.generic_u8string();
    return std::string (u.begin(), u.end());
}
fs::path fromUtf8 (const std::string& s)
{
    return fs::path (std::u8string (s.begin(), s.end()));
}

// File identity for the metadata cache: size and modification time.
std::pair<std::uintmax_t, long long> fileStamp (const std::string& path)
{
    std::error_code ec;
    const auto size = fs::file_size (fs::path (path), ec);
    if (ec) return { 0, 0 };
    const auto t = fs::last_write_time (fs::path (path), ec);
    return { size, ec ? 0LL : static_cast<long long> (t.time_since_epoch().count()) };
}

nlohmann::json infoToJson (const ModelInfo& m)
{
    nlohmann::json j;
    j["name"] = m.name; j["modeledBy"] = m.modeledBy; j["gearMake"] = m.gearMake; j["gearModel"] = m.gearModel;
    j["gearType"] = m.gearType; j["toneType"] = m.toneType; j["architecture"] = m.architecture; j["version"] = m.version;
    j["sampleRate"] = m.sampleRate; j["numInputs"] = m.numInputs; j["numOutputs"] = m.numOutputs;
    if (m.hasLoudness) j["loudness"] = m.loudnessDb;
    if (m.hasInputLevel) j["inputDbu"] = m.inputLevelDbu;
    if (m.hasOutputLevel) j["outputDbu"] = m.outputLevelDbu;
    return j;
}

void infoFromJson (const nlohmann::json& j, const std::string& path, ModelInfo& m)
{
    m.path = path;
    m.name = j.value ("name", std::string()); m.modeledBy = j.value ("modeledBy", std::string());
    m.gearMake = j.value ("gearMake", std::string()); m.gearModel = j.value ("gearModel", std::string());
    m.gearType = j.value ("gearType", std::string()); m.toneType = j.value ("toneType", std::string());
    m.architecture = j.value ("architecture", std::string()); m.version = j.value ("version", std::string());
    m.sampleRate = j.value ("sampleRate", -1.0);
    m.numInputs = j.value ("numInputs", 1); m.numOutputs = j.value ("numOutputs", 1);
    m.hasLoudness = j.contains ("loudness");     m.loudnessDb = j.value ("loudness", 0.0);
    m.hasInputLevel = j.contains ("inputDbu");   m.inputLevelDbu = j.value ("inputDbu", 0.0);
    m.hasOutputLevel = j.contains ("outputDbu"); m.outputLevelDbu = j.value ("outputDbu", 0.0);
    if (m.name.empty()) m.name = fs::path (path).stem().string();
    m.kind = classify (m.gearType, m.name);
}

int cabModeToInt (CabMode m) { return static_cast<int> (m); }
CabMode cabModeFromInt (int v) { return v == 1 ? CabMode::AmpOnly : (v == 2 ? CabMode::IncludesCab : CabMode::Auto); }
} // namespace

//==============================================================================
std::string LibraryEntry::displayName() const
{
    if (! displayNameOverride.empty()) return displayNameOverride;
    return info.name.empty() ? fs::path (file).stem().string() : info.name;
}
std::string LibraryEntry::manufacturerName() const { return manufacturer.empty() ? info.gearName() : manufacturer; }
std::string LibraryEntry::artistName() const       { return artist.empty() ? info.modeledBy : artist; }
std::string LibraryEntry::ampTypeName() const
{
    if (! ampType.empty()) return ampType;
    return info.kind == CaptureKind::Unknown ? std::string() : std::string (captureKindName (info.kind));
}
std::string LibraryEntry::gainCategoryName() const { return gainCategory.empty() ? info.gainCategory() : gainCategory; }

bool LibraryEntry::includesCab() const noexcept
{
    if (cabMode == CabMode::AmpOnly) return false;
    if (cabMode == CabMode::IncludesCab) return true;
    return info.includesCab();
}

const char* sortKeyName (SortKey k) noexcept
{
    switch (k)
    {
        case SortKey::Manufacturer: return "Manufacturer";
        case SortKey::AmpType:      return "Amp Type";
        case SortKey::GainCategory: return "Gain";
        case SortKey::Artist:       return "Artist";
        case SortKey::Recent:       return "Recently Used";
        case SortKey::DateAdded:    return "Date Added";
        case SortKey::Name:
        default:                    return "Name";
    }
}

//==============================================================================
ModelLibrary::ModelLibrary (std::string folder) : dir (canonical (folder)) {}

std::string ModelLibrary::indexPath() const { return (fs::path (dir) / "library.json").string(); }

std::string ModelLibrary::hashFile (const std::string& path)
{
    std::ifstream f (fs::path (path), std::ios::binary);
    if (! f) return {};
    std::uint64_t h = 1469598103934665603ull; // FNV-1a 64
    char buf[65536];
    std::uint64_t total = 0;
    while (f)
    {
        f.read (buf, sizeof (buf));
        const auto n = f.gcount();
        for (std::streamsize i = 0; i < n; ++i) { h ^= static_cast<unsigned char> (buf[i]); h *= 1099511628211ull; }
        total += static_cast<std::uint64_t> (n);
    }
    char out[40];
    std::snprintf (out, sizeof (out), "%016llx%08llx", static_cast<unsigned long long> (h), static_cast<unsigned long long> (total & 0xffffffffull));
    return out;
}

bool ModelLibrary::isInsideLibrary (const std::string& path) const
{
    const auto p = fs::path (canonical (path)).lexically_normal();
    const auto d = fs::path (dir).lexically_normal();
    const auto rel = p.lexically_relative (d);
    return ! rel.empty() && rel != fs::path (".") && *rel.begin() != fs::path ("..") && ! rel.is_absolute();
}

LibraryEntry* ModelLibrary::findMutable (const std::string& id)
{
    for (auto& e : entries) if (e.id == id) return &e;
    return nullptr;
}

const LibraryEntry* ModelLibrary::find (const std::string& id) const
{
    return const_cast<ModelLibrary*> (this)->findMutable (id);
}

const LibraryEntry* ModelLibrary::findByPath (const std::string& path) const
{
    if (path.empty()) return nullptr;
    // Stored paths are canonical already, so one canonicalisation of the query is enough.
    for (const auto& e : entries)
        if (e.file == path || (! e.originalPath.empty() && e.originalPath == path)) return &e;
    const auto c = canonical (path);
    for (const auto& e : entries)
        if (e.file == c || (! e.originalPath.empty() && e.originalPath == c)) return &e;
    return nullptr;
}

//==============================================================================
bool ModelLibrary::load()
{
    std::error_code ec;
    fs::create_directories (fs::path (dir), ec);
    if (! fs::is_directory (fs::path (dir))) return false;
    entries.clear();

    try
    {
        std::ifstream f { fs::path (indexPath()) };
        if (f)
        {
            const auto root = nlohmann::json::parse (f);
            for (const auto& j : root.value ("models", nlohmann::json::array()))
            {
                LibraryEntry e;
                e.id = j.value ("id", std::string());
                const auto rel = j.value ("file", std::string());
                if (e.id.empty() || rel.empty()) continue;
                e.file = (fs::path (dir) / fromUtf8 (rel)).lexically_normal().string();
                e.originalPath = j.value ("original", std::string());
                e.favorite = j.value ("favorite", false);
                e.category = j.value ("category", std::string());
                e.manufacturer = j.value ("manufacturer", std::string());
                e.artist = j.value ("artist", std::string());
                e.ampType = j.value ("ampType", std::string());
                e.gainCategory = j.value ("gain", std::string());
                e.displayNameOverride = j.value ("name", std::string());
                e.cabMode = cabModeFromInt (j.value ("cabMode", 0));
                e.addedAt = j.value ("added", std::int64_t { 0 });
                e.lastUsedAt = j.value ("used", std::int64_t { 0 });
                if (! fs::exists (fs::path (e.file))) continue;
                const auto stamp = fileStamp (e.file);
                if (j.contains ("info") && j.value ("size", std::uintmax_t { 0 }) == stamp.first && j.value ("mtime", 0LL) == stamp.second)
                {
                    infoFromJson (j["info"], e.file, e.info); // unchanged since indexed: no need to parse the model again
                }
                else
                {
                    std::string err;
                    if (! readInfo (e.file, e.info, err)) continue;
                }
                if (findMutable (e.id) == nullptr) entries.push_back (std::move (e));
            }
        }
    }
    catch (const std::exception&)
    {
        // A damaged index is rebuilt from the files by rescan(); favourites etc. are lost, models are not.
        entries.clear();
    }

    rescan();
    return true;
}

bool ModelLibrary::save() const
{
    nlohmann::json models = nlohmann::json::array();
    for (const auto& e : entries)
    {
        nlohmann::json j;
        j["id"] = e.id;
        j["file"] = toUtf8 (fs::path (e.file).lexically_relative (fs::path (dir)));
        if (! e.originalPath.empty()) j["original"] = e.originalPath;
        if (e.favorite) j["favorite"] = true;
        if (! e.category.empty()) j["category"] = e.category;
        if (! e.manufacturer.empty()) j["manufacturer"] = e.manufacturer;
        if (! e.artist.empty()) j["artist"] = e.artist;
        if (! e.ampType.empty()) j["ampType"] = e.ampType;
        if (! e.gainCategory.empty()) j["gain"] = e.gainCategory;
        if (! e.displayNameOverride.empty()) j["name"] = e.displayNameOverride;
        if (e.cabMode != CabMode::Auto) j["cabMode"] = cabModeToInt (e.cabMode);
        j["added"] = e.addedAt;
        if (e.lastUsedAt != 0) j["used"] = e.lastUsedAt;
        const auto stamp = fileStamp (e.file);
        j["size"] = stamp.first;
        j["mtime"] = stamp.second;
        j["info"] = infoToJson (e.info);
        models.push_back (std::move (j));
    }
    nlohmann::json root;
    root["format"] = "viridian-nam-library";
    root["version"] = 1;
    root["models"] = std::move (models);

    const auto target = fs::path (indexPath());
    const auto temp = fs::path (indexPath() + ".tmp");
    {
        std::ofstream f (temp, std::ios::binary | std::ios::trunc);
        if (! f) return false;
        f << root.dump (1);
        if (! f) return false;
    }
    std::error_code ec;
    fs::rename (temp, target, ec);
    if (ec)
    {
        // Windows will not rename over an existing file in some setups: replace in two steps.
        fs::remove (target, ec);
        fs::rename (temp, target, ec);
    }
    return ! ec;
}

void ModelLibrary::rescan()
{
    // Drop entries whose file was deleted outside the app.
    entries.erase (std::remove_if (entries.begin(), entries.end(),
                                   [] (const LibraryEntry& e) { return ! fs::exists (fs::path (e.file)); }),
                   entries.end());

    std::error_code ec;
    for (fs::recursive_directory_iterator it (fs::path (dir), fs::directory_options::skip_permission_denied, ec), end;
         ! ec && it != end; it.increment (ec))
    {
        if (! it->is_regular_file (ec) || ! isNamFile (it->path())) continue;
        const auto path = canonical (it->path().string());
        bool known = false;
        for (const auto& e : entries) if (e.file == path) { known = true; break; }
        if (known) continue;

        LibraryEntry e;
        std::string err;
        if (! readInfo (path, e.info, err)) continue;
        e.id = hashFile (path);
        if (e.id.empty() || findMutable (e.id) != nullptr) continue; // an identical copy is already indexed
        e.file = path;
        e.addedAt = now();
        entries.push_back (std::move (e));
    }
}

//==============================================================================
std::string ModelLibrary::uniqueDestination (const std::string& fileName) const
{
    const auto stem = fs::path (fileName).stem().string();
    const auto ext = fs::path (fileName).extension().string();
    auto dest = fs::path (dir) / fileName;
    for (int n = 2; fs::exists (dest); ++n)
        dest = fs::path (dir) / (stem + " (" + std::to_string (n) + ")" + ext);
    return dest.string();
}

bool ModelLibrary::addFile (const std::string& source, ImportReport& report)
{
    ModelInfo info;
    std::string err;
    if (! isNamFile (fs::path (source))) { report.failed.emplace_back (source, "not a .nam file"); return false; }
    if (! readInfo (source, info, err)) { report.failed.emplace_back (source, err); return false; }

    const auto id = hashFile (source);
    if (id.empty()) { report.failed.emplace_back (source, "cannot read file"); return false; }
    if (findMutable (id) != nullptr) { report.duplicates.push_back (id); return true; }

    std::string dest;
    if (isInsideLibrary (source))
    {
        dest = canonical (source); // already in the folder (dropped in by hand): index it where it is
    }
    else
    {
        dest = uniqueDestination (fs::path (source).filename().string());
        std::error_code ec;
        if (! fs::copy_file (fs::path (source), fs::path (dest), fs::copy_options::none, ec))
        {
            report.failed.emplace_back (source, "copy failed: " + ec.message());
            return false;
        }
        dest = canonical (dest);
    }

    LibraryEntry e;
    e.id = id;
    e.file = dest;
    e.originalPath = isInsideLibrary (source) ? std::string() : canonical (source);
    readInfo (dest, e.info, err);
    e.addedAt = now();
    entries.push_back (std::move (e));
    report.added.push_back (id);
    return true;
}

ImportReport ModelLibrary::importFile (const std::string& path)
{
    ImportReport r;
    addFile (path, r);
    return r;
}

ImportReport ModelLibrary::importFiles (const std::vector<std::string>& paths)
{
    ImportReport r;
    for (const auto& p : paths) addFile (p, r);
    return r;
}

ImportReport ModelLibrary::importFolder (const std::string& folder, bool recursive)
{
    ImportReport r;
    std::vector<std::string> files;
    std::error_code ec;
    if (! fs::is_directory (fs::path (folder), ec)) { r.failed.emplace_back (folder, "not a folder"); return r; }
    if (isInsideLibrary (folder) || canonical (folder) == dir) { rescan(); return r; }

    auto collect = [&files] (const fs::directory_entry& d)
    {
        std::error_code e2;
        if (d.is_regular_file (e2) && isNamFile (d.path())) files.push_back (d.path().string());
    };
    if (recursive)
        for (fs::recursive_directory_iterator it (fs::path (folder), fs::directory_options::skip_permission_denied, ec), end; ! ec && it != end; it.increment (ec))
            collect (*it);
    else
        for (fs::directory_iterator it (fs::path (folder), fs::directory_options::skip_permission_denied, ec), end; ! ec && it != end; it.increment (ec))
            collect (*it);

    std::sort (files.begin(), files.end());
    for (const auto& f : files) addFile (f, r);
    return r;
}

bool ModelLibrary::replaceModel (const std::string& id, const std::string& newSource, std::string& error)
{
    auto* e = findMutable (id);
    if (e == nullptr) { error = "model not in library"; return false; }
    ModelInfo info;
    if (! readInfo (newSource, info, error)) return false;
    const auto newId = hashFile (newSource);
    if (newId.empty()) { error = "cannot read file"; return false; }
    if (newId != id && find (newId) != nullptr) { error = "that capture is already in the library"; return false; }

    std::string dest = uniqueDestination (fs::path (newSource).filename().string());
    std::error_code ec;
    if (! fs::copy_file (fs::path (newSource), fs::path (dest), fs::copy_options::none, ec))
    {
        error = "copy failed: " + ec.message();
        return false;
    }
    e = findMutable (id); // (no reallocation happened, but stay safe)
    const auto old = e->file;
    e->id = newId;
    e->file = canonical (dest);
    e->originalPath = canonical (newSource);
    readInfo (e->file, e->info, error);
    error.clear();
    if (isInsideLibrary (old)) fs::remove (fs::path (old), ec);
    return true;
}

bool ModelLibrary::remove (const std::string& id)
{
    auto it = std::find_if (entries.begin(), entries.end(), [&id] (const LibraryEntry& e) { return e.id == id; });
    if (it == entries.end()) return false;
    std::error_code ec;
    if (isInsideLibrary (it->file)) fs::remove (fs::path (it->file), ec);
    entries.erase (it);
    return true;
}

void ModelLibrary::setFavorite (const std::string& id, bool f)          { if (auto* e = findMutable (id)) e->favorite = f; }
void ModelLibrary::setCategory (const std::string& id, const std::string& c) { if (auto* e = findMutable (id)) e->category = c; }
void ModelLibrary::setCabMode (const std::string& id, CabMode m)        { if (auto* e = findMutable (id)) e->cabMode = m; }
void ModelLibrary::markUsed (const std::string& id, std::int64_t when)  { if (auto* e = findMutable (id)) e->lastUsedAt = when != 0 ? when : now(); }

void ModelLibrary::setOverride (const std::string& id, Field field, const std::string& value)
{
    auto* e = findMutable (id);
    if (e == nullptr) return;
    switch (field)
    {
        case Field::Name:         e->displayNameOverride = value; break;
        case Field::Manufacturer: e->manufacturer = value; break;
        case Field::Artist:       e->artist = value; break;
        case Field::AmpType:      e->ampType = value; break;
        case Field::GainCategory: e->gainCategory = value; break;
    }
}

//==============================================================================
std::vector<const LibraryEntry*> ModelLibrary::query (const LibraryQuery& q) const
{
    std::vector<std::string> words;
    {
        std::istringstream in (lower (q.text));
        for (std::string w; in >> w;) words.push_back (w);
    }

    std::vector<const LibraryEntry*> out;
    for (const auto& e : entries)
    {
        if (q.favoritesOnly && ! e.favorite) continue;
        if (q.recentOnly && e.lastUsedAt == 0) continue;
        if (! q.category.empty() && e.category != q.category) continue;
        if (! words.empty())
        {
            const auto hay = lower (e.displayName() + "\n" + fs::path (e.file).filename().string() + "\n" + e.manufacturerName() + "\n"
                                    + e.info.gearModel + "\n" + e.artistName() + "\n" + e.ampTypeName() + "\n" + e.gainCategoryName()
                                    + "\n" + e.info.toneType + "\n" + e.category + "\n" + e.info.architecture);
            bool all = true;
            for (const auto& w : words) if (hay.find (w) == std::string::npos) { all = false; break; }
            if (! all) continue;
        }
        out.push_back (&e);
    }

    const SortKey key = (q.recentOnly && q.sort == SortKey::Name) ? SortKey::Recent : q.sort;
    auto text = [key] (const LibraryEntry* e) -> std::string
    {
        switch (key)
        {
            case SortKey::Manufacturer: return lower (e->manufacturerName());
            case SortKey::AmpType:      return lower (e->ampTypeName());
            case SortKey::Artist:       return lower (e->artistName());
            case SortKey::GainCategory:
            {
                // Musical order rather than alphabetical.
                static const char* const order[] = { "clean", "crunch", "overdrive", "high gain", "fuzz" };
                const auto g = lower (e->gainCategoryName());
                for (int i = 0; i < 5; ++i) if (g == order[i]) return std::string (1, static_cast<char> ('0' + i));
                return g.empty() ? std::string() : "9" + g;
            }
            default: return {};
        }
    };
    std::stable_sort (out.begin(), out.end(), [&] (const LibraryEntry* a, const LibraryEntry* b)
    {
        if (key == SortKey::Recent && a->lastUsedAt != b->lastUsedAt) return q.descending ? a->lastUsedAt < b->lastUsedAt : a->lastUsedAt > b->lastUsedAt;
        if (key == SortKey::DateAdded && a->addedAt != b->addedAt) return q.descending ? a->addedAt < b->addedAt : a->addedAt > b->addedAt;
        if (key != SortKey::Name && key != SortKey::Recent && key != SortKey::DateAdded)
        {
            const auto ta = text (a), tb = text (b);
            if (ta.empty() != tb.empty()) return tb.empty(); // unknown values last
            if (ta != tb) return q.descending ? ta > tb : ta < tb;
        }
        const auto na = lower (a->displayName()), nb = lower (b->displayName());
        return (key == SortKey::Name && q.descending) ? na > nb : na < nb;
    });
    return out;
}

std::vector<std::string> ModelLibrary::categories() const
{
    std::vector<std::string> c;
    for (const auto& e : entries)
        if (! e.category.empty() && std::find (c.begin(), c.end(), e.category) == c.end()) c.push_back (e.category);
    std::sort (c.begin(), c.end(), [] (const std::string& a, const std::string& b) { return lower (a) < lower (b); });
    return c;
}

std::vector<const LibraryEntry*> ModelLibrary::recent (std::size_t maxCount) const
{
    LibraryQuery q;
    q.recentOnly = true;
    q.sort = SortKey::Recent;
    auto r = query (q);
    if (r.size() > maxCount) r.resize (maxCount);
    return r;
}

} // namespace vf::nam
