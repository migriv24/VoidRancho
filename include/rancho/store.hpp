/* rancho/store.hpp — the database at rest (okf/concepts/store.md).
 *
 * One SQLite file holds a whole Void Core state document as rows: a rune per
 * row, an edge per row, a tag per row, a mantle per row, and the state's other
 * top-level keys. Each rune and edge keeps the exact JSON Core wrote, so the
 * store can always give the document back (the ROUND-TRIP LAW); what it adds is
 * that the document no longer has to be in memory to be read, queried or partly
 * loaded.
 *
 * RUNG 1 IS VOID HORMIGA'S STORE (its src/store/store.hpp, 2026-10-27), and
 * every member it had is here with the same meaning, so Hormiga can move to
 * Rancho by changing a namespace. What Rancho adds, each derived from what Core
 * already wrote and never authored here:
 *
 *  - an edge's `weight` as a float64 column, with `directed` and the `i:j`
 *    ports of its relation — so weights are first-class, not text inside JSON;
 *  - a SOURCE on every tag row: who wrote it (an importer, a person, an agent,
 *    a rule). Void Squid's lesson: tags launder, unless provenance is kept;
 *  - the ROW INDEX ρ: a stable integer per (mantle, spirit.id), Rancho's own,
 *    never reused, kept across imports — so matrices from two times line up
 *    (okf/concepts/matrix-view.md);
 *  - TYPED COLUMNS for content fields a glyph annotates in `kinds`: numbers
 *    indexed for range queries and for the feature matrix. The schema is the
 *    glyph's; Rancho never declares one of its own;
 *  - QUERIES by SPEC §5 tag expression, answered exactly as `ls --tag` would;
 *  - blobs, and a store Void Hormiga wrote (format 1) opens and is migrated.
 *
 * Memory: a bounded page cache, plus whatever one call returns. Importing a
 * document reads it as a stream (rancho/json_stream.hpp), one rune at a time.
 *
 * DELETION IS SOFT until `purge`. A rune removed in the working set takes with
 * it edges to runes that were never loaded; if the removal is undone, Core
 * restores only what it held. So removed rows are marked with what removed them
 * (`rune:<id>`, `mantle:<id>`) and `restore` brings them back when that rune or
 * mantle reappears. `purge` forgets them, when no undo can reach them any more.
 *
 * ONE HOST AT A TIME. SQLite's WAL needs shared memory on one machine, and a
 * network filesystem's locks can corrupt a database two machines write
 * (okf/design/storage-research.md §1). A store on a network path is opened in
 * rollback-journal mode with an exclusive lock: one machine uses it, and a
 * second is refused rather than allowed to corrupt it. See rancho/media.hpp.
 *
 * Not thread-safe: one Store per thread, or serialize calls. */
#pragma once

#include "rancho/json_stream.hpp"
#include "rancho/media.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace rancho {

using Progress = std::function<void(double fraction, const char* phase)>;
/* A picture in a Hormiga v3 bundle: its path in the bundle ("assets/x.png") and
 * bytes. With no sink, an import keeps them in the store's blobs. */
using AssetSink = std::function<void(const std::string& path, const std::string& bytes)>;

struct ImportStats {
    bool ok = false;
    std::string error;
    long long mantles = 0, runes = 0, edges = 0, tags = 0, assets = 0;
    bool envelope = false;   // it was a .miga v3, not a bare state document
    std::string bundle_name; // the envelope's meta.name
};

/* Which runes a written state document holds. Every mantle is always present
 * (a mantle not selected is written with no runes and no edges), and an edge
 * is written only when BOTH its ends are. */
struct Selection {
    bool everything = true;
    std::set<std::string> whole;                              // mantles written whole
    std::map<std::string, std::vector<std::string>> by_tag;   // mantle -> a rune with ANY of these
    std::map<std::string, std::vector<std::string>> by_name;  // mantle -> these runes
    /* mantle -> cluster paths ("12", "12.4"): a path loads every leaf under it,
     * and the mantle's always-loaded runes (its measures) come with any. */
    std::map<std::string, std::vector<std::string>> by_cluster;
    static Selection all() { return {}; }
    static Selection none() {
        Selection s;
        s.everything = false;
        return s;
    }
};

struct MantleInfo {
    std::string name;
    long long runes = 0, edges = 0;
    long long bytes = 0;      // its runes' and edges' JSON
    long long rune_bytes = 0; // its runes' alone
};

/* One leaf of a mantle's cluster hierarchy, the unit a working set loads.
 * Paths sort so that leaves of one parent are adjacent. */
struct ClusterLeaf {
    std::string path;
    long long runes = 0, bytes = 0;
};

/* Runes of a mantle in order, from an offset, up to a byte budget. */
struct Page {
    std::vector<std::string> names;
    std::uint64_t bytes = 0;
};

/* HOW a mantle is clustered: recorded beside the result, so a re-run is
 * reproducible and anyone reading the clusters knows what they mean
 * (okf/concepts/cluster.md). */
struct ClusterOptions {
    std::uint32_t target = 2000;          // a leaf holds at most this many runes
    std::uint64_t seed = 1;               // the same database clusters the same way everywhere
    double resolution = 1.0;              // Leiden's γ
    std::vector<std::string> relations;   // the edges that count as ties; empty: every relation
    /* Use each edge's weight as its strength. Weights of different relations
     * are not commensurable (Void Core's quantity page), so this needs exactly
     * one relation in `relations`; and a weight must be positive. Off: every
     * edge is a tie of strength 1. */
    bool weighted = false;
    std::string settings_json() const;
};

struct ClusterStats {
    bool ok = false;
    std::string error;
    long long runes = 0, leaves = 0, always = 0;
    int depth = 0;
    double modularity = 0, seconds = 0;
};

/* A mantle's row: everything but its runes and edges. */
struct MantleRow {
    std::string name, mid;   // mid: the mantle's own "id"
    std::string skeleton;    // the mantle object without runes and layout
    std::optional<std::string> layout; // the layout object without edges; absent if the mantle had none
    bool has_runes = true, has_edges = true;
};

/* A tag on a rune, and who wrote it. */
struct TagRow {
    std::string tag, source;
};

struct OpenOptions {
    Sharing sharing = Sharing::Auto; // rancho/media.hpp
};

class Store {
public:
    /* 1: Void Hormiga's. 2: Hormiga's tables, plus Rancho's columns and tables;
     * a format-1 store is migrated in place when it is opened. */
    static constexpr int kFormat = 2;

    /* Opens, or creates, the store at `path`. */
    explicit Store(const std::filesystem::path& path, const OpenOptions& options = {});
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool ok() const { return db_ != nullptr && error_.empty(); }
    const std::string& error() const { return error_; }
    const std::filesystem::path& path() const { return path_; }
    /* Where the file lives, and how it was opened because of it. */
    Medium medium() const { return medium_; }
    bool exclusive() const { return exclusive_; }
    /* The format the file had when it was opened (1 if it was migrated). */
    int opened_format() const { return opened_format_; }

    /* Replace the whole store with the document `in` holds: a bare state
     * document, or a .miga v3 envelope (whose pictures go to `assets`, or to
     * the store's blobs). One transaction: on failure the store is as it was.
     * The row index survives: a rune keeps its row across imports. */
    ImportStats import(JsonStream& in, const Progress& progress = {}, const AssetSink& assets = {});

    /* The state document, whole or as `sel` chooses. */
    bool write_state(std::ostream& out, const Selection& sel = Selection::all());
    std::string state(const Selection& sel = Selection::all());

    std::vector<MantleInfo> mantles(); // exact: counts every row

    /* THE SAME, ESTIMATED, for planning: kept in two small tables, computed at
     * import and after clustering, so a plan does not read every rune of the
     * database on every open (4.7 s of a 7.4 s open at 5×10^5, in Hormiga).
     * Edits since drift them a little; a plan is a budget, not a ledger. A
     * mantle the estimates do not know yet is counted exactly. */
    std::vector<MantleInfo> estimates();
    std::vector<ClusterLeaf> leaf_estimates(const std::string& mantle);
    void refresh_estimates();

    /* A consistent, compacted copy of the whole store at `out` (SQLite's
     * VACUUM INTO), taken without stopping it. */
    bool vacuum_into(const std::filesystem::path& out);

    /* The store's own notes, as text. "" when unset. */
    std::string meta(const std::string& key);
    void set_meta(const std::string& key, const std::string& value);
    /* True if the store holds a document (an import happened, or rows were written). */
    bool has_document();
    long long rune_count(const std::string& mantle = {}); // {} = all mantles

    // ── queries: reading without loading (okf/concepts/working-set.md) ────
    /* The runes of `mantle` that a SPEC §5 tag expression selects, in rune
     * order: exactly what `ls --tag <expr>` answers on a Core holding the whole
     * mantle. The tag index narrows the candidates; Void Core's own evaluator
     * (vc_tag_match) decides every one, as SPEC §5 requires of a host — so the
     * grammar is never reimplemented here, only its AND/OR shape read for the
     * narrowing. `limit` 0: no limit. */
    std::vector<std::string> select(const std::string& mantle, const std::string& expr, long long limit = 0,
                                    long long offset = 0);
    long long count(const std::string& mantle, const std::string& expr);
    /* Runes whose typed field `field` (a glyph `kinds` annotation, see above)
     * is in [lo, hi], in value order. Dates (ISO 8601) are indexed as Unix
     * seconds, UTC. */
    std::vector<std::string> range(const std::string& mantle, const std::string& field, double lo, double hi,
                                   long long limit = 0);
    std::vector<TagRow> tags_of(const std::string& mantle, const std::string& rune);
    /* The persistent row ρ of a rune (by spirit id), -1 if it has none. */
    long long row_of(const std::string& mantle, const std::string& rid);

    // ── provenance ────────────────────────────────────────────────────────
    /* Who the tags written from now on are by: "import" during an import
     * unless set, else "" (unknown). A tag that was already there keeps the
     * source it had. The working set sets it from its actor. */
    void set_source(const std::string& source) { source_ = source; }
    const std::string& source() const { return source_; }

    // ── typed columns ─────────────────────────────────────────────────────
    /* Glyph descriptors the HOST registered (a JSON object, name → descriptor):
     * registered glyphs do not travel in the document (SPEC §3.3.3), so the
     * store would not otherwise know their `kinds`. Declared ones it reads from
     * the document itself. Reindexes the typed columns. */
    void set_registered_glyphs(const std::string& glyphs_json);
    void reindex_fields();

    // ── blobs ─────────────────────────────────────────────────────────────
    /* The bytes of pictures a bundle carried, by path. Not part of the
     * document and not authored through Core: a host's cache of its assets,
     * which it may keep elsewhere instead (an import sink, or files beside the
     * store). Dropping them changes no rune, edge or tag. */
    void put_blob(const std::string& path, const std::string& bytes);
    std::optional<std::string> blob(const std::string& path);
    std::vector<std::string> blob_paths();
    /* Rancho (2026-11-02, Void Hormiga's ask): forget one blob; true if there
     * was one. And forget them all; returns how many. The pages they held are
     * free for the store's next writes but the FILE keeps its size until
     * `compact`. */
    bool remove_blob(const std::string& path);
    std::size_t clear_blobs();

    /* Rancho: rebuild the store in place, returning its free pages to the disk
     * (SQLite's VACUUM). It rewrites the whole file and needs as much free
     * space again while it runs, so it is a host's choice when to pay for it:
     * after `clear_blobs` or `purge`, not after every edit. Not inside
     * `begin`/`commit`. */
    bool compact();

    // ── write-back (rancho/working_set.hpp is its caller) ─────────────────
    bool begin();
    bool commit();
    void rollback();

    void put_top(const std::string& key, const std::string& json);
    void keep_only_top(const std::set<std::string>& keys); // drops the others ("mantles" stays)

    /* The live mantle with this mid, else a soft-deleted one (restored, with
     * everything it took), else a new one. Returns its row id. */
    long long put_mantle(const MantleRow& m, int pos);
    void delete_mantle(long long mantle); // soft, with everything in it

    /* A rune as Core now has it. `json` is the rune's whole object. */
    void put_rune(long long mantle, const std::string& name, const std::string& json);
    void rename_rune(long long mantle, const std::string& from, const std::string& to);
    void delete_rune(long long mantle, const std::string& name); // soft, with every edge touching it
    /* Bring back a rune soft-deleted with this spirit id, and what it took. */
    bool restore_rune(long long mantle, const std::string& rid);

    /* Make the live edges between `src` and `dst` with `relation` exactly
     * these (each a whole edge object), keeping rows that already match. */
    void set_edges(long long mantle, const std::string& src, const std::string& dst, const std::string& relation,
                   const std::vector<std::string>& jsons);

    /* Forget soft-deleted rows. */
    void purge();

    /* The live mantle's row id by name, -1 if none. */
    long long mantle_row(const std::string& name);
    /* The spirit id of the live rune `name`, if there is one. */
    std::optional<std::string> live_rune_id(long long mantle, const std::string& name);

    // ── clusters: where each rune belongs (okf/concepts/cluster.md) ───────
    /* Cluster a mantle by its links (Leiden, recursively, until each leaf holds
     * at most `target` runes; rancho/graph.hpp) and store each rune's
     * path, with the settings that produced it. Derived data, like an index:
     * rebuilt, never authored, and not part of the document. Runes of a measure
     * glyph are marked always-loaded, because every value edge points at one,
     * and value edges are not ties. */
    ClusterStats cluster(const std::string& mantle, const ClusterOptions& options, const Progress& progress = {});
    ClusterStats cluster(const std::string& mantle, std::uint32_t target = 2000, const Progress& progress = {});
    /* The settings the mantle was last clustered with, "" if never. */
    std::string cluster_settings(const std::string& mantle);
    std::vector<ClusterLeaf> leaves(const std::string& mantle); // sorted by path
    /* Runes in their order from `offset`, while their bytes fit `byte_cap`
     * (at least one), and at most `max_runes` of them (0: no limit). */
    Page page(const std::string& mantle, long long offset, std::uint64_t byte_cap, long long max_runes = 0);
    bool clustered(const std::string& mantle);
    /* The leaf path a rune is in, "" if none. */
    std::string cluster_of(const std::string& mantle, const std::string& rune);
    /* File a rune under a leaf (a rune made after the clustering). */
    void assign_cluster(const std::string& mantle, const std::string& rune, const std::string& path);

    /* The key the row index files a mantle under: its id, or "name:<name>" for
     * a mantle without one. */
    std::string mantle_key(long long mantle);

    // ── bulk load: a whole document arriving as rows (rancho/segments.hpp) ─
    /* The same as `import`, for a document that arrives already in pieces —
     * rows with their positions — rather than as one JSON text. Not a second
     * door: like an import, it replaces the store with a document that some
     * Core wrote. begin_bulk clears; end_bulk indexes and commits; abort_bulk
     * leaves the store as it was. */
    bool begin_bulk();
    void bulk_top(const std::string& key, int pos, const std::optional<std::string>& json);
    long long bulk_mantle(const MantleRow& m, int pos);
    /* `sources`: tag -> who wrote it (absent: the store's source). `cluster`:
     * the leaf the rune was filed under ("!" always-loaded; "" none). */
    void bulk_rune(long long mantle, double pos, const std::string& json,
                   const std::map<std::string, std::string>& sources = {}, const std::string& cluster = {});
    void bulk_edge(long long mantle, double pos, const std::string& json);
    /* How the mantle's clusters were made (ClusterOptions::settings_json). */
    void bulk_cluster_settings(long long mantle, const std::string& settings);
    bool end_bulk();
    void abort_bulk();

    /* The connection, for Rancho's own read-side modules (the matrix view,
     * segments). READ ONLY: a write through it would be a second door. */
    sqlite3* handle() const { return db_; }
    /* Names of the glyphs declared or registered as `kind: "measure"`. */
    std::set<std::string> measure_glyphs();
    /* glyph -> field -> quantity level, for every annotated field. */
    std::map<std::string, std::map<std::string, std::string>> field_levels();

private:
    bool exec(const char* sql);
    bool migrate();
    void load_kinds();
    void index_fields(long long rune, const std::string& glyph, const std::string& json);
    void assign_rows(long long mantle);
    void assign_row(long long mantle, const std::string& rid);

    sqlite3* db_ = nullptr;
    std::string error_;
    std::filesystem::path path_;
    Medium medium_ = Medium::Unknown;
    bool exclusive_ = false;
    int opened_format_ = kFormat;
    std::string source_;
    // glyph -> field -> level, from declared and registered descriptors
    std::map<std::string, std::map<std::string, std::string>> kinds_;
    std::set<std::string> measures_;
    struct Bulk; // the bulk load's prepared statements, between begin_bulk and end_bulk
    std::unique_ptr<Bulk> bulk_;
};

} // namespace rancho
