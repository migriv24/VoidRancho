/* rancho/deed.hpp — one database, many files: the .rancho file
 * (okf/concepts/deed.md, okf/design/one-database-many-files.md).
 *
 * A database is one thing in identity and any number of things in storage. The
 * DEED is the small file that says which: who the database is (an id minted
 * once), and the PARTS its bytes live in — a live store, an immutable set of
 * segments on a shared drive, a folder of pictures — with which mantles each
 * part holds. It never lists rows, segments or files inside a part, so it stays
 * the same size however large the database grows.
 *
 *   {
 *     "rancho": 1,
 *     "id": "3f9c0a52e1b84d7d9a6c2f0e5b7d1a48",
 *     "name": "The database",
 *     "home": "main",
 *     "parts": [
 *       { "id": "main",     "form": "store",    "path": "main.db" },
 *       { "id": "archive",  "form": "segments", "path": "//nas/share/archive",
 *         "mantles": ["mantle_5e0c2a91d4"], "pin": "sha256:<64 hex>" },
 *       { "id": "pictures", "form": "blobs",    "path": "pictures" }
 *     ]
 *   }
 *
 * The rules a reader keeps (each is a test in deed_test.cpp):
 *  - EVERYTHING IS NAMED BY ITS ID (the author, 2026-11-06): the database by its
 *    id, a part by its id, a mantle by its Core id (`mantle.id`), which a rename
 *    keeps. A name is for a person. A deed that names a mantle by its name is
 *    still read (a key match wins over a name match), and is noted.
 *  - A MANTLE LIVES IN ONE PART: the part that names it, else the one part that
 *    names none. Void Core links only rune to rune within one mantle (SPEC §3.7),
 *    so splitting a database by mantle never cuts an edge; cross-mantle
 *    bindings are top-level and may dangle, which Core allows.
 *  - THE HOME holds the top-level keys and is the one part a working set
 *    writes, in this first form. Every other part is read. Declared glyphs and
 *    domains are the union of every present part's, the home's winning.
 *  - A MISSING PART IS A REGION NOT LOADED, NEVER AN ERROR. A drive that is not
 *    plugged in, a NAS that is down, a part in a form this version cannot read:
 *    the database still opens, says which parts are absent and why, and its
 *    document holds the mantles that are here. It is not complete, and a host
 *    must never read what is absent as deleted.
 *  - A PINNED PART NEVER CHANGES. A segments part with a pin is that one
 *    snapshot, checked against the SHA-256 of its manifest; a different one is a
 *    mismatch, reported and not read.
 *  - THE DOCUMENT DOES NOT DEPEND ON WHERE THE BYTES ARE. The same rows placed
 *    in one part or in three assemble to the same document (the placement law).
 *  - RANCHO IS AGNOSTIC (the author, 2026-11-06). A deed holds Rancho's own
 *    members and nothing else: whatever a host has to say about its database
 *    is data, and belongs in a mantle. A member Rancho does not know is passed
 *    through unchanged and noted, never interpreted. A deed written by a NEWER
 *    Rancho is read for what this version knows and is never rewritten by it,
 *    so nothing this version cannot judge is left behind, stale, after an edit.
 *  - A DATABASE DECLARES NO KIND (the author, 2026-11-06: "a database exists on
 *    its own"). A host that builds a file type on the rancho (a .miga, an .ink)
 *    brings the Expectations it relies on to Database::open, which checks them.
 *  - A STORE MAY CARRY ITS OWN DEED (store meta `rancho.deed`), so one SQLite
 *    file is a whole rancho with no file beside it; a store with none is a
 *    database of one part. `Database::open` takes either a deed or a store.
 *
 * Opening never creates a part's file (only `create` does), and reads a
 * segments part through a restored copy in the cache folder: derived data,
 * rebuilt whenever it is missing. A database whose home is a segments part is
 * read-only: `home()` is null, and its top-level keys come from the snapshot. */
#pragma once

#include "rancho/media.hpp"
#include "rancho/store.hpp"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace rancho {

/* One place a database's bytes live. */
struct Part {
    std::string id;   // unique in its deed
    std::string form; // "store", "segments" or "blobs"; any other is kept and read as not openable here
    /* Relative to the deed's folder, or absolute. A UNC path is a part on a
     * network, opened as rancho/media.hpp says. */
    std::string path;
    /* store and segments: the mantles this part holds, by key (MantleRef:
     * Core's mantle id, else "name:<name>"). A bare name is read too, as a
     * fallback. Empty: every mantle no other part names (at most one part may
     * be that). */
    std::vector<std::string> mantles;
    /* segments only: "sha256:<hex>" of the manifest this part must be. Empty:
     * the part follows its ROOT, whatever snapshot that is. */
    std::string pin;
    /* Rancho (2026-11-09, Void Squid's question 1): store and segments: the id
     * of the database whose part this is, when it is ANOTHER database's (a
     * library a project reads). Its store must record that id: a library moved
     * or swapped for another is a mismatch, never a silent substitute. */
    std::string database;
    /* Rancho (2026-11-09, Void Squid's specification): the id of the store
     * part (usually the home) that keeps a person's edits over this part's
     * runes, an OVERLAY (rancho/overlay.hpp). This part is then read as
     * base ⊕ overlay, and is never written. */
    std::string overlay;
    std::string extra; // members this version does not know, as a JSON object ("" if none)
    bool known_form() const { return form == "store" || form == "segments" || form == "blobs"; }
    /* A part of a form this version does not read still holds the mantles it
     * names, so a stale copy elsewhere is never read in their place; it is
     * never the part that holds the rest. */
    bool holds_rows() const { return form == "store" || form == "segments" || (!known_form() && !mantles.empty()); }
    bool holds_rest() const { return mantles.empty() && (form == "store" || form == "segments"); }
};

struct Deed {
    static constexpr int kFormat = 1;
    int format = kFormat;
    std::string id;   // who the database is: minted once, kept by every copy of this deed
    std::string name; // for a person; never an identity
    std::string home; // the part with the top-level keys; "" = the first store part (else the first segments part)
    std::vector<Part> parts;
    std::string extra; // members this version does not know, as a JSON object ("" if none)

    const Part* part(const std::string& part_id) const;
    const Part* home_part() const;
    /* The part a mantle lives in: the first that names its key, else the first
     * that names its name, else the one that names none. nullptr: no part
     * holds it. The text form is for a mantle known only by what a deed says
     * (its part is absent): it is matched as both a key and a name. */
    const Part* part_of(const MantleRef& mantle) const;
    const Part* part_of(const std::string& mantle) const { return part_of(MantleRef{mantle, mantle}); }
};

/* A new database id: 128 bits as 32 lowercase hex characters. */
std::string mint_database_id();

/* Text and files. `error` says why a text is not a deed. A deed written by a
 * newer Rancho is read for what this version knows; write_deed refuses it,
 * because this version cannot tell which of its members an edit makes false. */
std::optional<Deed> parse_deed(std::string_view text, std::string* error = nullptr);
std::string deed_json(const Deed& deed); // stable member order, two-space indent, newline-terminated
std::optional<Deed> read_deed(const std::filesystem::path& file, std::string* error = nullptr);
bool write_deed(const Deed& deed, const std::filesystem::path& file, std::string* error = nullptr); // atomic

/* What is wrong with a deed (errors) and what a person should know (notes).
 * A deed with errors still opens, by the first-part-wins rule, and says so. */
struct DeedCheck {
    std::vector<std::string> errors, notes;
    bool ok() const { return errors.empty(); }
};
DeedCheck check(const Deed& deed);

/* A deed for a store that has none: one part, the store, holding every mantle,
 * its path relative to `deed_folder` when the store is under it. The id is the
 * one the store records (meta `rancho.database`), else a new one. */
Deed deed_for_store(Store& store, const std::filesystem::path& deed_folder, const std::string& name = {});

/* A store that carries its own deed (store meta `rancho.deed`): one file that
 * is a whole rancho. The deed's home becomes this store, at its own file name,
 * so a renamed or copied file still finds itself. Refused (false, `error`) when
 * the store is the home of another database. */
bool embed_deed(Store& store, Deed deed, std::string* error = nullptr);

class Database;

/* What a host expects of a database it opens. A file type built on the
 * rancho (the author, 2026-11-06: ".miga and .ink will inherit properties from
 * the rancho file and build themselves") is the host's, and so are the
 * assumptions it makes about what it opens: mantles it needs, glyphs it reads.
 * The database declares none of them ("a database exists on its own"); the
 * host brings them, and Database::open checks each, the ones it builds on
 * first. A database that falls short still opens, and says what is missing. */
struct Expectation {
    std::string name;                   // the host's own name for it ("scene", "ink"); Rancho keeps no registry
    std::vector<std::string> builds_on; // expectations this one assumes, checked before it
    /* Run on the open database. Errors and notes are reported prefixed with
     * the expectation's name. */
    std::function<void(Database& db, DeedCheck& out)> check;
};

enum class PartStatus {
    Present,
    Missing,     // not where the deed says, or could not be opened (the reason says which)
    NotOpenable, // a form this version does not read
    Mismatch,    // there, but not what the deed names: another snapshot, another database's store
};
const char* part_status_name(PartStatus s);

struct PartState {
    std::string id;
    PartStatus status = PartStatus::Missing;
    std::string reason; // empty when present
    std::filesystem::path where;
};

struct DatabaseOptions {
    Sharing sharing = Sharing::Auto; // how store parts are opened (rancho/media.hpp)
    /* Where a segments part is restored to be read: rebuildable, so a phone's
     * cache_dir. Empty: <temp>/rancho-cache. */
    std::filesystem::path cache_dir;
    std::vector<Expectation> expect; // what this host relies on, checked at open
    std::filesystem::path temp_dir;  // SQLite's spill folder (OpenOptions::temp_dir)
};

/* A database opened from its deed. Not thread-safe, like Store. */
class Database {
public:
    /* nullptr only when the file is not a deed (`error` says why). A deed whose
     * parts are all absent still opens, empty and incomplete. `deed_file` may
     * also be a store: its embedded deed, else a deed of one part (not
     * written anywhere; `deed()` holds it). */
    static std::unique_ptr<Database> open(const std::filesystem::path& deed_file, const DatabaseOptions& options = {},
                                          std::string* error = nullptr);
    /* A new database at `deed_file`: a fresh id, and one store part beside it
     * (<stem>.db) holding every mantle. Refuses to overwrite a deed. */
    static std::unique_ptr<Database> create(const std::filesystem::path& deed_file, const std::string& name,
                                            const DatabaseOptions& options = {}, std::string* error = nullptr);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    const Deed& deed() const { return deed_; }
    const std::filesystem::path& folder() const { return folder_; }
    const std::vector<PartState>& parts() const { return states_; }
    /* Every part is here and is what the deed names. A database that is not
     * complete answers for what it holds; what it does not hold is unknown,
     * never deleted. */
    bool complete() const;
    /* What a person should know: the deed's check, shadowed mantles, glyph
     * declarations that disagree between parts. */
    const std::vector<std::string>& notes() const { return notes_; }

    /* OVERLAYS (Rancho, 2026-11-09; rancho/overlay.hpp). Record `rune`, as
     * the person's Core now has it, over the read-only part that holds
     * `mantle` (a name or a key): its difference from the base rune with the
     * same spirit id, or the whole rune when the base has none (a rune the
     * person made). A rune equal to its base forgets the record. `source` is
     * who wrote the tags it adds. False (and `error`) when no part holding
     * the mantle has a present overlay store. In this first form a host calls
     * it from its own write-back; a working set does not yet route rows of a
     * base mantle here (open question §12-13). */
    bool record_overlay(const std::string& mantle, const std::string& rune, const std::string& source,
                        std::string* error = nullptr);
    /* Forget the overlay of one rune (the person reverted it, or removed a
     * rune they made). */
    bool forget_overlay(const std::string& mantle, const std::string& rid);
    /* RESIDUALS (rule 7): edits kept for base runes the base no longer has,
     * never dropped: "<part>/<mantle key>/<spirit id>". */
    std::vector<std::string> residuals();
    /* Whether the expectation called `name` was given, and it and everything
     * it builds on found no error. */
    bool meets(const std::string& name) const { return met_.count(name) > 0; }
    /* What the expectations found, each line prefixed with its name. Also in
     * notes(). */
    const DeedCheck& expectation_check() const { return expectation_check_; }
    /* The deed with every mantle it names by its name written as its key,
     * where a present part holds it: what `rancho ids` writes. */
    Deed deed_by_id();

    /* The home store, the one a working set writes: nullptr if it is absent,
     * or if the home is a segments part (a read-only database).
     * In this first form a working set holds the home's mantles only: give
     * Core `home()->state(...)`, never `state(...)`, or a full sync would copy
     * the other parts' mantles into the home. */
    Store* home();
    /* The store a mantle's rows are read from (a segments part's restored
     * copy), nullptr if its part is absent. `mantle` is a key or a name.
     * Treat a non-home store as read-only. */
    Store* store_of(const std::string& mantle);

    struct Placed {
        std::string mantle, part; // the mantle's name (as the deed names it, when its part is absent)
        bool present = false;
        std::string key;          // its key (MantleRef); as the deed names it, when absent
    };
    /* Every mantle a present part holds, in document order, and every mantle
     * the deed names whose part is absent (present = false). */
    std::vector<Placed> placement();

    /* The database's document, whole or as `sel` chooses, written from each
     * part in turn: memory is one row at a time, as Store::write_state. */
    bool write_state(std::ostream& out, const Selection& sel = Selection::all());
    std::string state(const Selection& sel = Selection::all());

    /* A blob by path ("assets/x.png"): from a blobs part, else the home
     * store's blobs. A path that climbs out of its part is refused. */
    std::optional<std::string> blob(const std::string& path);

private:
    Store* overlay_store(const Part& base); // the present store keeping `base`'s overlay, or null

    Database() = default;
    static std::unique_ptr<Database> from_deed(Deed deed, const std::filesystem::path& folder,
                                               const DatabaseOptions& options);
    void open_parts(const DatabaseOptions& options);
    void check_expectations(const std::vector<Expectation>& given);
    Store* home_rows(); // the home's rows, whatever its form
    void merge_declarations();

    Deed deed_;
    std::filesystem::path folder_;
    std::vector<PartState> states_;
    std::vector<std::unique_ptr<Store>> stores_; // by part index; null when absent or not rows
    std::vector<std::string> notes_;
    std::string glyphs_, domains_; // the union of the present parts' declarations, "" for none
    std::set<std::string> met_; // the expectations given that held, with all they build on
    DeedCheck expectation_check_;
};

} // namespace rancho
