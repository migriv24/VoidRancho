/* store.cpp — see rancho/store.hpp and okf/concepts/store.md.
 *
 * Ported from Void Hormiga's src/store/store.cpp (2026-10-27). Where a function
 * is unchanged, so is its comment; Rancho's additions say so. */
#include "rancho/store.hpp"

#include "rancho/graph.hpp"
#include "voidcore.h"

#include "sql.hpp"

#include "json.hpp"
#include "sqlite3.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace rancho {

namespace {

/* Hormiga's tables (format 1) with Rancho's columns in them (format 2): edges
 * gain weight/directed/ports, tags gain source. A format-1 file has the tables
 * without the columns; `migrate` adds them. */
const char* kTables =
    "CREATE TABLE IF NOT EXISTS store_meta(key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS top(key TEXT PRIMARY KEY, pos INTEGER NOT NULL, json TEXT);"
    "CREATE TABLE IF NOT EXISTS mantles(id INTEGER PRIMARY KEY, pos INTEGER NOT NULL, name TEXT NOT NULL DEFAULT '',"
    " json TEXT NOT NULL DEFAULT '{}', layout TEXT, has_runes INTEGER NOT NULL DEFAULT 1,"
    " has_edges INTEGER NOT NULL DEFAULT 1, mid TEXT, deleted INTEGER NOT NULL DEFAULT 0, deleted_by TEXT);"
    "CREATE TABLE IF NOT EXISTS runes(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " name TEXT NOT NULL, rid TEXT, glyph TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS edges(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " src TEXT NOT NULL, dst TEXT NOT NULL, relation TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0, weight REAL NOT NULL DEFAULT 1.0,"
    " directed INTEGER NOT NULL DEFAULT 1, from_port INTEGER, to_port INTEGER);"
    "CREATE TABLE IF NOT EXISTS clusters(rune INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, path TEXT NOT NULL,"
    " always INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS clusters_by_path ON clusters(mantle, path);"
    /* Rancho (2026-11-09): the always-loaded runes found without reading
     * every cluster row of the mantle (Hormiga measured ~300 ms per call). */
    "CREATE INDEX IF NOT EXISTS clusters_always ON clusters(mantle) WHERE always=1;"
    "CREATE TABLE IF NOT EXISTS est_mantles(mantle INTEGER PRIMARY KEY, runes INTEGER, edges INTEGER,"
    " rune_bytes INTEGER, edge_bytes INTEGER);"
    "CREATE TABLE IF NOT EXISTS est_leaves(mantle INTEGER, path TEXT, runes INTEGER, bytes INTEGER,"
    " PRIMARY KEY(mantle, path));"
    "CREATE TABLE IF NOT EXISTS tags(rune INTEGER NOT NULL, tag TEXT NOT NULL, source TEXT);";

/* Rancho's own tables. `rows` is keyed by the mantle's id and the rune's spirit
 * id rather than by row ids, and an import does not clear it: a rune keeps its
 * row for as long as the store exists. */
const char* kRanchoTables =
    "CREATE TABLE IF NOT EXISTS rows(mkey TEXT NOT NULL, rid TEXT NOT NULL, row INTEGER NOT NULL,"
    " PRIMARY KEY(mkey, rid));"
    "CREATE INDEX IF NOT EXISTS rows_by_row ON rows(mkey, row);"
    "CREATE TABLE IF NOT EXISTS fields(rune INTEGER NOT NULL, field TEXT NOT NULL, num REAL, text TEXT,"
    " PRIMARY KEY(rune, field));"
    "CREATE INDEX IF NOT EXISTS fields_by_num ON fields(field, num);"
    "CREATE TABLE IF NOT EXISTS cluster_runs(mantle INTEGER PRIMARY KEY, settings TEXT NOT NULL,"
    " modularity REAL, leaves INTEGER, depth INTEGER);"
    "CREATE TABLE IF NOT EXISTS blobs(path TEXT PRIMARY KEY, bytes BLOB NOT NULL, size INTEGER NOT NULL);"
    /* Rancho (2026-11-09): THE RESEÑA, a summary per node of a mantle's
     * cluster tree ("" is the mantle itself), and the ties between siblings.
     * Derived, like the clusters: made when clustering runs, kept by
     * write-back. A tie is an edge with both ends clustered (value edges to
     * measures are not ties). */
    "CREATE TABLE IF NOT EXISTS cluster_nodes(mantle INTEGER NOT NULL, path TEXT NOT NULL, parent TEXT,"
    " kind INTEGER NOT NULL DEFAULT 0, runes INTEGER NOT NULL DEFAULT 0, internal INTEGER NOT NULL DEFAULT 0,"
    " outgoing INTEGER NOT NULL DEFAULT 0, internal_weight REAL NOT NULL DEFAULT 0,"
    " outgoing_weight REAL NOT NULL DEFAULT 0, PRIMARY KEY(mantle, path));"
    "CREATE INDEX IF NOT EXISTS cluster_nodes_by_parent ON cluster_nodes(mantle, parent);"
    /* Rancho (2026-11-09, Void Squid's specification): a person's edits over
     * a part they do not own, per base rune (rancho/overlay.hpp). Not derived:
     * an import does not clear them, because the document imported is the
     * person's own part, and these are their edits to another. */
    "CREATE TABLE IF NOT EXISTS overlays(base TEXT NOT NULL, mkey TEXT NOT NULL, rid TEXT NOT NULL,"
    " diff TEXT NOT NULL, PRIMARY KEY(base, mkey, rid));"
    "CREATE TABLE IF NOT EXISTS cluster_ties(mantle INTEGER NOT NULL, parent TEXT NOT NULL, a TEXT NOT NULL,"
    " b TEXT NOT NULL, links INTEGER NOT NULL DEFAULT 0, weight REAL NOT NULL DEFAULT 0,"
    " PRIMARY KEY(mantle, parent, a, b));";

// built after an import: inserting into indexed tables is several times slower
const char* kIndexes =
    "CREATE INDEX IF NOT EXISTS runes_by_name ON runes(mantle, name);"
    "CREATE INDEX IF NOT EXISTS runes_by_pos ON runes(mantle, pos);"
    "CREATE INDEX IF NOT EXISTS edges_by_src ON edges(mantle, src);"
    "CREATE INDEX IF NOT EXISTS edges_by_dst ON edges(mantle, dst);"
    "CREATE INDEX IF NOT EXISTS edges_by_pos ON edges(mantle, pos);"
    "CREATE INDEX IF NOT EXISTS edges_by_relation ON edges(mantle, relation);"
    "CREATE INDEX IF NOT EXISTS tags_by_tag ON tags(tag, rune);"
    "CREATE INDEX IF NOT EXISTS tags_by_rune ON tags(rune);"
    "CREATE INDEX IF NOT EXISTS runes_by_rid ON runes(mantle, rid);"
    "CREATE INDEX IF NOT EXISTS runes_by_glyph ON runes(mantle, glyph);"
    "CREATE INDEX IF NOT EXISTS runes_deleted_by ON runes(deleted_by) WHERE deleted_by IS NOT NULL;"
    "CREATE INDEX IF NOT EXISTS edges_deleted_by ON edges(deleted_by) WHERE deleted_by IS NOT NULL;";

const char* kDropAll =
    "DROP INDEX IF EXISTS runes_by_name; DROP INDEX IF EXISTS runes_by_pos;"
    "DROP INDEX IF EXISTS edges_by_src; DROP INDEX IF EXISTS edges_by_dst; DROP INDEX IF EXISTS edges_by_pos;"
    "DROP INDEX IF EXISTS edges_by_relation; DROP INDEX IF EXISTS runes_by_glyph;"
    "DROP INDEX IF EXISTS tags_by_tag; DROP INDEX IF EXISTS tags_by_rune; DROP INDEX IF EXISTS runes_by_rid;"
    "DROP INDEX IF EXISTS runes_deleted_by; DROP INDEX IF EXISTS edges_deleted_by;"
    "DELETE FROM top; DELETE FROM mantles; DELETE FROM runes; DELETE FROM edges; DELETE FROM tags;"
    "DELETE FROM clusters; DELETE FROM est_mantles; DELETE FROM est_leaves; DELETE FROM fields;"
    "DELETE FROM cluster_runs; DELETE FROM cluster_nodes; DELETE FROM cluster_ties;";

std::string str_of(const nlohmann::json& j, const char* k) {
    const auto it = j.find(k);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

int base64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* Rancho: an edge's typed columns, read from the object Core wrote. Absent
 * members take SPEC §3.7's defaults (weight 1.0, directed true). A relation
 * spelled `i:j` names a port pair (SPEC §4 port signatures). */
struct EdgeCols {
    std::string src, dst, rel;
    double weight = 1.0;
    bool directed = true;
    long long from_port = -1, to_port = -1; // -1: no ports
};

bool parse_ports(const std::string& rel, long long& a, long long& b) {
    const auto colon = rel.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= rel.size()) return false;
    for (std::size_t i = 0; i < rel.size(); ++i)
        if (i != colon && (rel[i] < '0' || rel[i] > '9')) return false;
    if (colon > 9 || rel.size() - colon - 1 > 9) return false;
    a = std::stoll(rel.substr(0, colon));
    b = std::stoll(rel.substr(colon + 1));
    return true;
}

EdgeCols edge_cols(const nlohmann::json& j) {
    EdgeCols c;
    c.src = str_of(j, "from");
    c.dst = str_of(j, "to");
    c.rel = str_of(j, "relation");
    if (const auto w = j.find("weight"); w != j.end() && w->is_number()) c.weight = w->get<double>();
    if (const auto d = j.find("directed"); d != j.end() && d->is_boolean()) c.directed = d->get<bool>();
    parse_ports(c.rel, c.from_port, c.to_port);
    return c;
}

/* Bind an edge's columns at 3..10: src, dst, relation, json, bytes, weight, directed, from_port, to_port. */
void bind_edge(Stmt& s, int first, const EdgeCols& c, const std::string& raw) {
    s.text(first, c.src).text(first + 1, c.dst).text(first + 2, c.rel).text(first + 3, raw)
        .integer(first + 4, (long long)raw.size()).real(first + 5, c.weight).integer(first + 6, c.directed ? 1 : 0);
    if (c.from_port >= 0) s.integer(first + 7, c.from_port).integer(first + 8, c.to_port);
    else s.null(first + 7).null(first + 8);
}

/* Rancho: an ISO 8601 date or date-time as Unix seconds, UTC; false if `s` is
 * not one. Dates are interval-level POINTS (Void Core's quantity page): indexed
 * as a number so a calendar can ask for a month, never added or scaled. */
bool iso_seconds(const std::string& s, double& out) {
    int y, mo, d, h = 0, mi = 0;
    double sec = 0;
    if (s.size() < 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (s[(std::size_t)i] < '0' || s[(std::size_t)i] > '9') return false;
    y = std::stoi(s.substr(0, 4));
    mo = std::stoi(s.substr(5, 2));
    d = std::stoi(s.substr(8, 2));
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    std::size_t i = 10;
    if (i < s.size() && (s[i] == 'T' || s[i] == ' ')) {
        if (s.size() < i + 6 || s[i + 3] != ':') return false;
        h = std::atoi(s.substr(i + 1, 2).c_str());
        mi = std::atoi(s.substr(i + 4, 2).c_str());
        i += 6;
        if (i < s.size() && s[i] == ':') {
            char* end = nullptr;
            sec = std::strtod(s.c_str() + i + 1, &end);
            i = (std::size_t)(end - s.c_str());
        }
        if (i < s.size() && s[i] == 'Z') ++i;
    }
    if (i != s.size()) return false;
    // days from civil (Howard Hinnant's algorithm)
    y -= mo <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (unsigned)(mo + (mo > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = (long long)era * 146097 + (long long)doe - 719468;
    out = (double)days * 86400.0 + h * 3600.0 + mi * 60.0 + sec;
    return true;
}

/* A content value as a typed column: a number (or a numeric string, or a date)
 * for every level but nominal, whose values are only ever compared for
 * equality; text for strings. */
bool field_value(const nlohmann::json& v, const std::string& level, std::optional<double>& num,
                 std::optional<std::string>& text) {
    if (v.is_number()) num = v.get<double>();
    else if (v.is_boolean()) num = v.get<bool>() ? 1.0 : 0.0;
    else if (v.is_string()) {
        const std::string s = v.get<std::string>();
        text = s;
        if (level != "nominal" && !s.empty()) {
            char* end = nullptr;
            const double x = std::strtod(s.c_str(), &end);
            double t = 0;
            if (end && *end == 0 && std::isfinite(x)) num = x;
            else if (iso_seconds(s, t)) num = t;
        }
    } else {
        return false;
    }
    return true;
}

/* Rancho (2026-11-09): the reseña's arithmetic. A cluster path "3.12.4" has
 * the ancestors "", "3", "3.12" and itself. */
std::vector<std::string> prefixes(std::string_view path) {
    std::vector<std::string> out{std::string()};
    if (path.empty()) return out;
    for (std::size_t i = 0; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '.') out.emplace_back(path.substr(0, i));
    return out;
}

std::string parent_of(std::string_view path) {
    const auto dot = path.rfind('.');
    return dot == std::string_view::npos ? std::string() : std::string(path.substr(0, dot));
}

/* What one tie between leaves `pu` and `pv` adds: one internal tie to every
 * common ancestor (the mantle among them), one outgoing tie to every node on
 * either side below their lowest common ancestor, and one tie between the two
 * children of that ancestor that hold its ends. */
struct TieEffect {
    std::vector<std::string> internal, outgoing;
    std::string parent, a, b; // empty a: the ends share a leaf
};

TieEffect tie_effect(std::string_view pu, std::string_view pv) {
    TieEffect t;
    const auto u = prefixes(pu), v = prefixes(pv);
    std::size_t common = 0;
    while (common < u.size() && common < v.size() && u[common] == v[common]) ++common;
    t.internal.assign(u.begin(), u.begin() + (std::ptrdiff_t)common);
    t.outgoing.insert(t.outgoing.end(), u.begin() + (std::ptrdiff_t)common, u.end());
    t.outgoing.insert(t.outgoing.end(), v.begin() + (std::ptrdiff_t)common, v.end());
    if (common < u.size() && common < v.size()) {
        t.parent = u[common - 1];
        t.a = u[common];
        t.b = v[common];
        if (t.b < t.a) std::swap(t.a, t.b);
    }
    return t;
}

} // namespace

Store::Store(const std::filesystem::path& path, const OpenOptions& options) : path_(path) {
    medium_ = detect_medium(path);
    if (!options.temp_dir.empty() && sqlite3_temp_directory == nullptr) {
        /* SQLite's process-wide temp folder: set once, before anything
         * spills, and never freed (SQLite reads it for the process's life). */
        std::error_code ec;
        std::filesystem::create_directories(options.temp_dir, ec);
        sqlite3_temp_directory = sqlite3_mprintf("%s", options.temp_dir.string().c_str());
    }
    if (sqlite3_open(path.string().c_str(), &db_) != SQLITE_OK) {
        error_ = db_ ? sqlite3_errmsg(db_) : "cannot open the store";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    sqlite3_busy_timeout(db_, 2000);
    /* Rancho (2026-11-09, Void Squid's finding): decide what the file is
     * BEFORE writing to it. A store is an empty file, a file whose store_meta
     * names a format, or a file with Hormiga's format-1 tables. Anything else
     * (another program's SQLite whose table names happen to be ours, or not a
     * database at all) is refused untouched: no journal mode, no table, no
     * byte changed. */
    if (const std::string why = foreign(); !why.empty()) {
        error_ = why;
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    /* A bounded page cache (8 MB) whatever the database's size: the store is
     * how a database larger than memory is read at all. */
    exec("PRAGMA synchronous=NORMAL; PRAGMA cache_size=-8192; PRAGMA temp_store=FILE;");
    /* Rancho: one host at a time where the medium cannot share (media.hpp).
     * The exclusive lock is taken now, by a write, and held until close: a
     * second machine is refused at open, not corrupted at its first edit. */
    exclusive_ = options.sharing == Sharing::SingleHost || medium_ == Medium::Network;
    if (exclusive_) {
        if (!exec("PRAGMA locking_mode=EXCLUSIVE; PRAGMA journal_mode=DELETE;") ||
            !exec("BEGIN EXCLUSIVE; CREATE TABLE IF NOT EXISTS store_meta(key TEXT PRIMARY KEY, value TEXT);"
                  " COMMIT;")) {
            error_ = "the store is in use by another process or machine, or its folder cannot be locked (" +
                     error_ + ")";
            sqlite3_close(db_);
            db_ = nullptr;
            return;
        }
    }
    if ((!exclusive_ && !exec("PRAGMA journal_mode=WAL;")) || !exec(kTables) || !exec(kRanchoTables) || !migrate() ||
        !exec(kIndexes)) {
        const int code = sqlite3_errcode(db_) & 0xFF;
        if (code == SQLITE_BUSY || code == SQLITE_LOCKED) // someone holds it exclusively: say who might
            error_ = "the store is in use by another process or machine (" + error_ + ")";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    Stmt m(db_, "INSERT OR IGNORE INTO store_meta VALUES('format', ?)");
    m.text(1, std::to_string(kFormat)).run();
    load_kinds();
}

Store::~Store() {
    if (db_) sqlite3_close(db_);
}

std::string Store::foreign() {
    std::set<std::string> tables;
    {
        // a file another connection holds exclusively cannot be read: that is "in use", not "foreign"
        auto failed = [&](int rc) {
            const std::string msg = sqlite3_errmsg(db_);
            if ((rc & 0xFF) == SQLITE_BUSY || (rc & 0xFF) == SQLITE_LOCKED)
                return "the store is in use by another process or machine (" + msg + ")";
            return "is not a Rancho store: " + msg;
        };
        sqlite3_stmt* q = nullptr;
        if (const int rc = sqlite3_prepare_v2(db_, "SELECT name FROM sqlite_master WHERE type='table'", -1, &q,
                                              nullptr);
            rc != SQLITE_OK) {
            const std::string why = failed(rc);
            sqlite3_finalize(q);
            return why;
        }
        int rc;
        while ((rc = sqlite3_step(q)) == SQLITE_ROW)
            tables.insert(reinterpret_cast<const char*>(sqlite3_column_text(q, 0)));
        const std::string why = rc == SQLITE_DONE ? std::string() : failed(rc);
        sqlite3_finalize(q);
        if (!why.empty()) return why;
    }
    if (tables.empty()) return {}; // a new store
    if (tables.count("store_meta")) {
        sqlite3_stmt* q = nullptr;
        bool has_format = false;
        if (sqlite3_prepare_v2(db_, "SELECT 1 FROM store_meta WHERE key='format'", -1, &q, nullptr) == SQLITE_OK)
            has_format = sqlite3_step(q) == SQLITE_ROW;
        sqlite3_finalize(q);
        if (has_format) return {};
    }
    // Void Hormiga's format 1, written before store_meta held a format
    bool hormiga = true;
    for (const char* t : {"top", "mantles", "runes", "edges", "tags"}) hormiga = hormiga && tables.count(t);
    if (hormiga) return {};
    std::string names;
    for (const auto& t : tables) names += (names.empty() ? "" : ", ") + t;
    return "is not a Rancho store: it holds another program's tables (" + names + "), and was not changed";
}

bool Store::exec(const char* sql) {
    char* msg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &msg) != SQLITE_OK) {
        error_ = msg ? msg : "sqlite failed";
        sqlite3_free(msg);
        return false;
    }
    return true;
}

/* Rancho: a store Void Hormiga wrote (format 1) has the tables and not the
 * columns. Add them, fill them from the JSON every row already keeps, and give
 * every rune its row. Nothing that was there changes. */
bool Store::migrate() {
    bool has_weight = false, has_source = false;
    {
        Stmt q(db_, "SELECT name FROM pragma_table_info('edges')");
        while (q.row()) has_weight = has_weight || q.col(0) == "weight";
    }
    {
        Stmt q(db_, "SELECT name FROM pragma_table_info('tags')");
        while (q.row()) has_source = has_source || q.col(0) == "source";
    }
    if (has_weight && has_source) return true;
    opened_format_ = 1;
    if (!exec("BEGIN")) return false;
    bool ok = true;
    if (!has_weight)
        ok = exec("ALTER TABLE edges ADD COLUMN weight REAL NOT NULL DEFAULT 1.0;"
                  "ALTER TABLE edges ADD COLUMN directed INTEGER NOT NULL DEFAULT 1;"
                  "ALTER TABLE edges ADD COLUMN from_port INTEGER;"
                  "ALTER TABLE edges ADD COLUMN to_port INTEGER;"
                  "UPDATE edges SET weight=json_extract(json,'$.weight')"
                  " WHERE json_type(json,'$.weight') IN ('integer','real');"
                  "UPDATE edges SET directed=0 WHERE json_type(json,'$.directed')='false';");
    if (ok && !has_source) ok = exec("ALTER TABLE tags ADD COLUMN source TEXT;");
    if (ok) {
        std::vector<std::pair<long long, std::string>> ported;
        {
            Stmt q(db_, "SELECT id, relation FROM edges WHERE relation LIKE '%:%'");
            while (q.row()) ported.emplace_back(q.col_int(0), std::string(q.col(1)));
        }
        Stmt up(db_, "UPDATE edges SET from_port=?, to_port=? WHERE id=?");
        for (const auto& [id, rel] : ported) {
            long long a, b;
            if (parse_ports(rel, a, b)) up.integer(1, a).integer(2, b).integer(3, id).run();
        }
        std::vector<long long> ms;
        {
            Stmt q(db_, "SELECT id FROM mantles");
            while (q.row()) ms.push_back(q.col_int(0));
        }
        for (const long long m : ms) assign_rows(m);
        ok = error_.empty();
    }
    if (ok) {
        Stmt f(db_, "INSERT OR REPLACE INTO store_meta VALUES('format', ?)");
        f.text(1, std::to_string(kFormat)).run();
        Stmt w(db_, "INSERT OR REPLACE INTO store_meta VALUES('migrated_from', '1')");
        w.run();
    }
    if (!ok || !exec("COMMIT")) {
        exec("ROLLBACK");
        if (error_.empty()) error_ = "could not migrate a format-1 store";
        return false;
    }
    load_kinds();
    reindex_fields();
    return true;
}

ImportStats Store::import(JsonStream& in, const Progress& progress, const AssetSink& assets) {
    ImportStats st;
    if (!db_) {
        st.error = error_;
        return st;
    }
    error_.clear();
    if (!exec("BEGIN") || !exec(kDropAll)) {
        st.error = error_;
        return st;
    }
    const std::string source = source_.empty() ? std::string("import") : source_;
    Stmt top(db_, "INSERT OR REPLACE INTO top VALUES(?,?,?)");
    Stmt mantle_new(db_, "INSERT INTO mantles(pos) VALUES(?)");
    Stmt mantle_done(db_, "UPDATE mantles SET name=?, json=?, layout=?, has_runes=?, has_edges=?, mid=? WHERE id=?");
    Stmt rune(db_, "INSERT INTO runes(mantle,pos,name,rid,glyph,json,bytes) VALUES(?,?,?,?,?,?,?)");
    Stmt tag(db_, "INSERT INTO tags VALUES(?,?,?)");
    Stmt edge(db_, "INSERT INTO edges(mantle,pos,src,dst,relation,json,bytes,weight,directed,from_port,to_port)"
                   " VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    Stmt meta(db_, "INSERT OR REPLACE INTO store_meta VALUES(?,?)");
    Stmt blob(db_, "INSERT OR REPLACE INTO blobs VALUES(?,?,?)");

    int top_pos = 0;
    auto tick = [&](const char* phase) {
        if (progress && in.size()) progress((double)in.offset() / (double)in.size(), phase);
    };

    /* One mantle: its runes and edges become rows as they are read; every other
     * member is kept as written, for the mantle's own row. */
    auto read_mantle = [&](int pos) -> bool {
        mantle_new.integer(1, pos).run();
        const long long id = sqlite3_last_insert_rowid(db_);
        std::string skeleton = "{", layout, name, mid, raw;
        bool has_layout = false, has_runes = false, has_edges = false;
        long long rpos = 0, epos = 0;
        const bool ok = in.object([&](const std::string& key) -> bool {
            if (key == "runes") {
                has_runes = true;
                return in.array([&] {
                    if (!in.capture(raw)) return false;
                    nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
                    if (j.is_discarded() || !j.is_object()) return in.fail("a rune that is not an object");
                    const nlohmann::json sp = j.value("spirit", nlohmann::json::object());
                    rune.integer(1, id).real(2, (double)rpos++).text(3, str_of(sp, "name")).text(4, str_of(sp, "id"))
                        .text(5, str_of(j, "glyph")).text(6, raw).integer(7, (long long)raw.size()).run();
                    const long long rid = sqlite3_last_insert_rowid(db_);
                    if (const auto t = j.find("tags"); t != j.end() && t->is_array())
                        for (const auto& x : *t)
                            if (x.is_string()) {
                                tag.integer(1, rid).text(2, x.get<std::string>()).text(3, source).run();
                                ++st.tags;
                            }
                    if (++st.runes % 4096 == 0) tick("Reading the records");
                    return true;
                });
            }
            if (key == "layout" && in.peek() == '{') {
                has_layout = true;
                layout = "{";
                return in.object([&](const std::string& lk) -> bool {
                    if (lk == "edges") {
                        has_edges = true;
                        return in.array([&] {
                            if (!in.capture(raw)) return false;
                            nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
                            if (j.is_discarded() || !j.is_object()) return in.fail("an edge that is not an object");
                            edge.integer(1, id).real(2, (double)epos++);
                            bind_edge(edge, 3, edge_cols(j), raw);
                            edge.run();
                            ++st.edges;
                            return true;
                        });
                    }
                    if (!in.capture(raw)) return false;
                    if (layout.size() > 1) layout += ",";
                    layout += nlohmann::json(lk).dump() + ":" + raw;
                    return true;
                }) && (layout += "}", true);
            }
            if (!in.capture(raw)) return false;
            if (key == "name" || key == "id") {
                const auto j = nlohmann::json::parse(raw, nullptr, false);
                if (j.is_string()) (key == "name" ? name : mid) = j.get<std::string>();
            }
            if (skeleton.size() > 1) skeleton += ",";
            skeleton += nlohmann::json(key).dump() + ":" + raw;
            return true;
        });
        if (!ok) return false;
        skeleton += "}";
        mantle_done.text(1, name).text(2, skeleton);
        if (has_layout) mantle_done.text(3, layout);
        else mantle_done.null(3);
        mantle_done.integer(4, has_runes ? 1 : 0).integer(5, has_edges ? 1 : 0).text(6, mid).integer(7, id).run();
        assign_rows(id); // Rancho: a row for every rune not seen before, in document order
        ++st.mantles;
        return true;
    };

    auto read_state = [&]() -> bool {
        std::string raw;
        return in.object([&](const std::string& key) -> bool {
            if (key == "mantles") {
                top.text(1, "mantles").integer(2, top_pos++).null(3).run();
                int pos = 0;
                return in.array([&] { return read_mantle(pos++); });
            }
            if (key == "_baseline") return in.skip(); // the store is the baseline
            if (!in.capture(raw)) return false;
            top.text(1, key).integer(2, top_pos++).text(3, raw).run();
            return true;
        });
    };

    /* The outer object is a bare state or an envelope; which is decided by its
     * keys, and both start with "version". Members that are not the state are
     * kept aside and filed once that is known. */
    std::vector<std::pair<std::string, std::string>> held;
    bool is_state = false;
    const bool ok = in.object([&](const std::string& key) -> bool {
        if (key == "state") {
            st.envelope = true;
            return read_state();
        }
        if (key == "magic") st.envelope = true;
        if (key == "assets" && in.peek() == '{') {
            st.envelope = true;
            return in.object([&](const std::string& path) -> bool {
                std::string bytes;
                unsigned acc = 0;
                int bits = 0;
                const bool good = in.string_chunks([&](std::string_view chunk) {
                    for (const char c : chunk) {
                        const int v = base64_value((unsigned char)c);
                        if (v < 0) continue; // padding, line breaks
                        acc = (acc << 6) | (unsigned)v;
                        bits += 6;
                        if (bits >= 8) {
                            bits -= 8;
                            bytes += (char)((acc >> bits) & 0xFF);
                        }
                    }
                });
                if (!good) return false;
                if (assets) assets(path, bytes);
                else blob.text(1, path).blob(2, bytes).integer(3, (long long)bytes.size()).run();
                ++st.assets;
                tick("Restoring pictures");
                return true;
            });
        }
        if (key == "mantles") {
            is_state = true;
            top.text(1, "mantles").integer(2, top_pos++).null(3).run();
            int pos = 0;
            return in.array([&] { return read_mantle(pos++); });
        }
        if (key == "_baseline") return in.skip();
        std::string raw;
        if (!in.capture(raw)) return false;
        held.emplace_back(key, std::move(raw));
        return true;
    });

    if (ok) {
        if (st.envelope && !is_state) {
            for (const auto& [k, v] : held) {
                meta.text(1, "bundle." + k).text(2, v).run();
                if (k == "meta") {
                    const auto j = nlohmann::json::parse(v, nullptr, false);
                    if (j.is_object()) st.bundle_name = str_of(j, "name");
                }
            }
        } else {
            for (const auto& [k, v] : held) top.text(1, k).integer(2, top_pos++).text(3, v).run();
        }
    }
    if (!ok || !error_.empty()) {
        st.error = !in.ok() ? in.error() : (!error_.empty() ? error_ : "the document could not be read");
        exec("ROLLBACK");
        exec(kIndexes);
        load_kinds();
        return st;
    }
    if (progress) progress(1.0, "Indexing");
    if (!exec(kIndexes)) {
        st.error = error_;
        exec("ROLLBACK");
        return st;
    }
    load_kinds();     // the document's own declarations, which may come after its mantles
    reindex_fields(); // Rancho: typed columns, now that the glyphs are known
    refresh_estimates();
    if (!exec("COMMIT")) {
        st.error = error_;
        exec("ROLLBACK");
        return st;
    }
    st.ok = true;
    return st;
}

// ── bulk load (Rancho) ───────────────────────────────────────────────────────

struct Store::Bulk {
    Stmt rune, tag, cluster, edge;
    explicit Bulk(sqlite3* db)
        : rune(db, "INSERT INTO runes(mantle,pos,name,rid,glyph,json,bytes) VALUES(?,?,?,?,?,?,?)"),
          tag(db, "INSERT INTO tags VALUES(?,?,?)"), cluster(db, "INSERT OR REPLACE INTO clusters VALUES(?,?,?,?)"),
          edge(db, "INSERT INTO edges(mantle,pos,src,dst,relation,json,bytes,weight,directed,from_port,to_port)"
                   " VALUES(?,?,?,?,?,?,?,?,?,?,?)") {}
};

bool Store::begin_bulk() {
    if (!db_) return false;
    error_.clear();
    if (!exec("BEGIN") || !exec(kDropAll)) {
        exec("ROLLBACK");
        return false;
    }
    bulk_ = std::make_unique<Bulk>(db_);
    return true;
}

void Store::bulk_cluster_settings(long long mantle, const std::string& settings) {
    Stmt q(db_, "INSERT OR REPLACE INTO cluster_runs(mantle, settings) VALUES(?,?)");
    q.integer(1, mantle).text(2, settings).run();
}

void Store::bulk_top(const std::string& key, int pos, const std::optional<std::string>& json) {
    Stmt q(db_, "INSERT OR REPLACE INTO top VALUES(?,?,?)");
    q.text(1, key).integer(2, pos);
    if (json) q.text(3, *json);
    else q.null(3);
    q.run();
}

long long Store::bulk_mantle(const MantleRow& m, int pos) {
    Stmt q(db_, "INSERT INTO mantles(pos, name, json, layout, has_runes, has_edges, mid) VALUES(?,?,?,?,?,?,?)");
    q.integer(1, pos).text(2, m.name).text(3, m.skeleton);
    if (m.layout) q.text(4, *m.layout);
    else q.null(4);
    q.integer(5, m.has_runes ? 1 : 0).integer(6, m.has_edges ? 1 : 0).text(7, m.mid).run();
    return sqlite3_last_insert_rowid(db_);
}

void Store::bulk_rune(long long mantle, double pos, const std::string& json,
                      const std::map<std::string, std::string>& sources, const std::string& cluster) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) {
        error_ = "a rune that is not an object";
        return;
    }
    const nlohmann::json sp = j.value("spirit", nlohmann::json::object());
    Stmt& rune = bulk_->rune;
    rune.integer(1, mantle).real(2, pos).text(3, str_of(sp, "name")).text(4, str_of(sp, "id"))
        .text(5, str_of(j, "glyph")).text(6, json).integer(7, (long long)json.size()).run();
    const long long row = sqlite3_last_insert_rowid(db_);
    Stmt& tag = bulk_->tag;
    if (const auto t = j.find("tags"); t != j.end() && t->is_array())
        for (const auto& x : *t)
            if (x.is_string()) {
                const auto s = sources.find(x.get<std::string>());
                tag.integer(1, row).text(2, x.get<std::string>()).text(3, s != sources.end() ? s->second : source_).run();
            }
    if (!cluster.empty()) {
        Stmt& c = bulk_->cluster;
        c.integer(1, row).integer(2, mantle).text(3, cluster).integer(4, cluster == "!" ? 1 : 0).run();
    }
}

void Store::bulk_edge(long long mantle, double pos, const std::string& json) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) {
        error_ = "an edge that is not an object";
        return;
    }
    Stmt& edge = bulk_->edge;
    edge.integer(1, mantle).real(2, pos);
    bind_edge(edge, 3, edge_cols(j), json);
    edge.run();
}

bool Store::end_bulk() {
    bulk_.reset(); // its statements, before the indexes are built and the transaction ends
    if (!error_.empty()) {
        abort_bulk();
        return false;
    }
    std::vector<long long> ms;
    {
        Stmt q(db_, "SELECT id FROM mantles ORDER BY pos");
        while (q.row()) ms.push_back(q.col_int(0));
    }
    for (const long long m : ms) assign_rows(m);
    if (!error_.empty() || !exec(kIndexes)) {
        abort_bulk();
        return false;
    }
    load_kinds();
    reindex_fields();
    /* Rancho: rows that arrived filed under leaves get their reseña. Packing
     * marks do not travel with the rows: a restored tree's bundles and slices
     * read as communities until the mantle is clustered again. */
    for (const long long m : ms) {
        Stmt q(db_, "SELECT 1 FROM clusters WHERE mantle=? LIMIT 1");
        q.integer(1, m);
        if (q.row()) summarize(m, {});
    }
    refresh_estimates();
    if (!exec("COMMIT")) {
        abort_bulk();
        return false;
    }
    return true;
}

void Store::abort_bulk() {
    bulk_.reset();
    const std::string why = error_;
    exec("ROLLBACK");
    exec(kIndexes);
    load_kinds();
    error_ = why;
}

bool Store::write_state(std::ostream& out, const Selection& sel) {
    if (!db_) return false;
    // write_mantles' temp table, made before a statement is reading: no schema change mid-step
    exec("CREATE TEMP TABLE IF NOT EXISTS ws(name TEXT PRIMARY KEY)");
    Stmt tops(db_, "SELECT key, json FROM top ORDER BY pos");
    out << "{";
    bool first_top = true;
    while (tops.row()) {
        const std::string key(tops.col(0));
        if (!first_top) out << ",";
        first_top = false;
        out << nlohmann::json(key).dump() << ":";
        if (key != "mantles" || !tops.is_null(1)) {
            out << tops.col(1);
            continue;
        }
        out << "[";
        bool first_m = true;
        write_mantles(out, sel, {}, first_m);
        out << "]";
    }
    tops.reset();
    out << "}";
    return (bool)out;
}

/* Rancho (2026-11-06, the deed): the mantle objects of write_state, alone, so a
 * database whose mantles live in several stores is written as one document
 * without any of them being one string (rancho/deed.hpp). */
bool Store::write_mantles(std::ostream& out, const Selection& sel, const std::function<bool(const MantleRef&)>& keep,
                          bool& first_m) {
    if (!db_) return false;
    exec("CREATE TEMP TABLE IF NOT EXISTS ws(name TEXT PRIMARY KEY);" // before the statements that read them
         "CREATE TEMP TABLE IF NOT EXISTS far(l TEXT NOT NULL, f TEXT NOT NULL, PRIMARY KEY(l, f));");
    Stmt ms(db_, "SELECT id, name, json, layout, has_runes, has_edges, mid FROM mantles WHERE deleted=0 ORDER BY pos");
    Stmt all_runes(db_, "SELECT json FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos");
    Stmt all_edges(db_, "SELECT json FROM edges WHERE mantle=? AND deleted=0 ORDER BY pos");
    /* A selection is read FROM the loaded names, through the name indexes, so it
     * costs what it loads and not what the mantle holds. The order is forced
     * (CROSS JOIN, INDEXED BY): the planner, knowing nothing of the temp table's
     * size, walked the whole mantle in order to skip a sort (0.9 s for 1 MB of a
     * 300 MB mantle), and for edges chose the dst index and rescanned the loaded
     * set per row (40 s). Both now take ~0.03 s. */
    Stmt ws_runes(db_, "SELECT r.json FROM temp.ws w CROSS JOIN runes r INDEXED BY runes_by_name"
                       " ON r.mantle=?1 AND r.name=w.name WHERE r.deleted=0 ORDER BY r.pos");
    Stmt ws_edges(db_, "SELECT e.json FROM temp.ws w CROSS JOIN edges e INDEXED BY edges_by_src"
                       " ON e.mantle=?1 AND e.src=w.name CROSS JOIN temp.ws w2 ON w2.name=e.dst"
                       " WHERE e.deleted=0 ORDER BY e.pos");
    /* Rancho (2026-11-09): the fringe. Every edge with its source held whose
     * other end is held or a kept far end, and every edge with its target
     * held whose source is a kept far end; each edge once, in order. */
    Stmt ws_fringe(db_, "SELECT e.json FROM edges e WHERE e.id IN ("
                        " SELECT e2.id FROM temp.ws w CROSS JOIN edges e2 INDEXED BY edges_by_src"
                        " ON e2.mantle=?1 AND e2.src=w.name WHERE e2.deleted=0 AND ("
                        "  EXISTS(SELECT 1 FROM temp.ws x WHERE x.name=e2.dst)"
                        "  OR EXISTS(SELECT 1 FROM temp.far p WHERE p.l=e2.src AND p.f=e2.dst))"
                        " UNION ALL SELECT e3.id FROM temp.ws w CROSS JOIN edges e3 INDEXED BY edges_by_dst"
                        " ON e3.mantle=?1 AND e3.dst=w.name WHERE e3.deleted=0"
                        "  AND EXISTS(SELECT 1 FROM temp.far p WHERE p.l=e3.dst AND p.f=e3.src))"
                        " ORDER BY e.pos");

    /* Rows straight to `out`: a mantle of 5×10^5 runes is written without its
     * runes ever being one string. */
    auto list = [&](Stmt* q, long long id) {
        out << "[";
        if (q) {
            q->integer(1, id);
            bool first = true;
            while (q->row()) {
                if (!first) out << ",";
                out << q->col(0);
                first = false;
            }
            q->reset();
        }
        out << "]";
    };
    // "{a,b" of an object written as "{a,b}": the members, without the brace; and whether there are any
    auto open_object = [&](std::string_view object) {
        const auto end = object.rfind('}');
        const std::string_view body = end == std::string_view::npos ? std::string_view("{") : object.substr(0, end);
        out << body;
        for (std::size_t i = body.find('{') + 1; i < body.size(); ++i)
            if (body[i] != ' ' && body[i] != '\n' && body[i] != '\r' && body[i] != '\t') return true;
        return false;
    };

    while (ms.row()) {
        const long long id = ms.col_int(0);
        const std::string name(ms.col(1));
        if (keep) {
            const std::string mid(ms.col(6));
            if (!keep(MantleRef{mid.empty() ? "name:" + name : mid, name})) continue;
        }
        const std::string skeleton(ms.col(2));
        const bool has_layout = !ms.is_null(3);
        const std::string layout = has_layout ? std::string(ms.col(3)) : std::string();
        const bool has_runes = ms.col_int(4) != 0, has_edges = ms.col_int(5) != 0;

        Stmt* runes = nullptr;
        Stmt* edges = nullptr;
        if (sel.everything || sel.whole.count(name)) {
            runes = &all_runes;
            edges = &all_edges;
        } else if (fill_held(id, name, sel)) {
            runes = &ws_runes;
            edges = &ws_edges;
            if (sel.fringe) {
                fill_far(id, sel.fringe_cap);
                edges = &ws_fringe;
            }
        }

        if (!first_m) out << ",";
        first_m = false;
        bool comma = open_object(skeleton);
        if (has_runes) {
            out << (comma ? "," : "") << "\"runes\":";
            list(runes, id);
            comma = true;
        }
        if (has_layout) {
            out << (comma ? "," : "") << "\"layout\":";
            const bool lcomma = open_object(layout);
            if (has_edges) {
                out << (lcomma ? "," : "") << "\"edges\":";
                list(edges, id);
            }
            out << "}";
        }
        out << "}";
    }
    ms.reset();
    return (bool)out;
}

bool Store::fill_held(long long id, const std::string& name, const Selection& sel) {
    const auto t = sel.by_tag.find(name);
    const auto n = sel.by_name.find(name);
    const auto c = sel.by_cluster.find(name);
    if (t == sel.by_tag.end() && n == sel.by_name.end() && c == sel.by_cluster.end()) return false;
    exec("CREATE TEMP TABLE IF NOT EXISTS ws(name TEXT PRIMARY KEY); DELETE FROM temp.ws;");
    if (t != sel.by_tag.end()) {
        Stmt q(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM tags t JOIN runes r ON r.id=t.rune"
                    " WHERE t.tag=? AND r.mantle=? AND r.deleted=0");
        for (const auto& tg : t->second) q.text(1, tg).integer(2, id).run();
    }
    if (n != sel.by_name.end()) {
        Stmt q(db_, "INSERT OR IGNORE INTO temp.ws VALUES(?)");
        for (const auto& nm : n->second) q.text(1, nm).run();
    }
    if (c != sel.by_cluster.end()) {
        /* A path and every leaf under it, as ONE index range ("12" .. "12.~": a
         * digit sorts after '.', so "120" is outside it); the second condition
         * only keeps "12" from matching "12x". Written with OR, the planner read
         * the mantle's whole index per path: 58 ms a leaf, 28 s for the ~480
         * small leaves a 2 GB machine's plan holds at 5×10^5 runes. Now 0.5 ms. */
        Stmt q(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM clusters c INDEXED BY clusters_by_path"
                    " JOIN runes r ON r.id=c.rune WHERE c.mantle=?1 AND c.path>=?2 AND c.path<=?2||'.~'"
                    " AND (c.path=?2 OR c.path>?2||'.') AND r.deleted=0");
        for (const auto& path : c->second) q.integer(1, id).text(2, path).run();
        Stmt always(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM clusters c JOIN runes r ON r.id=c.rune"
                         " WHERE c.mantle=?1 AND r.deleted=0 AND c.always=1");
        always.integer(1, id).run();
    }
    return true;
}

namespace {
/* Every (held, far) pair of a mantle's edges that leave temp.ws. */
const char* kFarPairs =
    "SELECT w.name AS l, e.dst AS f FROM temp.ws w CROSS JOIN edges e INDEXED BY edges_by_src"
    " ON e.mantle=?1 AND e.src=w.name WHERE e.deleted=0 AND NOT EXISTS(SELECT 1 FROM temp.ws x WHERE x.name=e.dst)"
    " UNION SELECT w.name, e.src FROM temp.ws w CROSS JOIN edges e INDEXED BY edges_by_dst"
    " ON e.mantle=?1 AND e.dst=w.name WHERE e.deleted=0 AND NOT EXISTS(SELECT 1 FROM temp.ws x WHERE x.name=e.src)";
} // namespace

void Store::fill_far(long long id, std::uint32_t cap) {
    exec("CREATE TEMP TABLE IF NOT EXISTS far(l TEXT NOT NULL, f TEXT NOT NULL, PRIMARY KEY(l, f));"
         "DELETE FROM temp.far;");
    Stmt q(db_, std::string("INSERT INTO temp.far SELECT l, f FROM (SELECT l, f, ROW_NUMBER() OVER"
                            " (PARTITION BY l ORDER BY f) AS rn FROM (") + kFarPairs + ")) WHERE rn<=?2");
    q.integer(1, id).integer(2, (long long)cap).run();
}

bool Frontier::complete(const std::string& mantle, const std::string& leaf) const {
    const auto m = mantles.find(mantle);
    if (m == mantles.end()) return false;
    if (m->second.whole) return true;
    if (leaf.empty()) return false;
    return std::binary_search(m->second.complete_leaves.begin(), m->second.complete_leaves.end(), leaf);
}

Frontier Store::frontier(const Selection& sel) {
    Frontier out;
    if (!db_) return out;
    std::vector<std::pair<long long, std::string>> ms;
    {
        Stmt q(db_, "SELECT id, name FROM mantles WHERE deleted=0 ORDER BY pos");
        while (q.row()) ms.emplace_back(q.col_int(0), std::string(q.col(1)));
    }
    for (const auto& [id, name] : ms) {
        MantleFrontier f;
        if (sel.everything || sel.whole.count(name)) {
            f.whole = true;
            Stmt q(db_, "SELECT COUNT(*) FROM runes WHERE mantle=? AND deleted=0");
            q.integer(1, id);
            if (q.row()) f.held = q.col_int(0);
            out.mantles.emplace(name, std::move(f));
            continue;
        }
        if (!fill_held(id, name, sel)) {
            out.mantles.emplace(name, std::move(f)); // held: nothing of it
            continue;
        }
        {
            Stmt q(db_, "SELECT COUNT(*) FROM temp.ws w JOIN runes r INDEXED BY runes_by_name"
                        " ON r.mantle=? AND r.name=w.name AND r.deleted=0");
            q.integer(1, id);
            if (q.row()) f.held = q.col_int(0);
        }
        // every pair that leaves, and who has too many to list
        exec("CREATE TEMP TABLE IF NOT EXISTS far_all(l TEXT NOT NULL, f TEXT NOT NULL, PRIMARY KEY(l, f));"
             "DELETE FROM temp.far_all;");
        {
            Stmt q(db_, std::string("INSERT INTO temp.far_all ") + kFarPairs);
            q.integer(1, id).run();
        }
        {
            Stmt q(db_, "SELECT COUNT(*) FROM temp.far_all");
            if (q.row()) f.leaving = q.col_int(0);
        }
        const long long cap = sel.fringe ? (long long)sel.fringe_cap : 0;
        if (sel.fringe) {
            Stmt q(db_, "SELECT l, COUNT(*) FROM temp.far_all GROUP BY l HAVING COUNT(*)>? ORDER BY l");
            q.integer(1, cap);
            while (q.row()) f.hubs.emplace(std::string(q.col(0)), q.col_int(1) - cap);
            fill_far(id, sel.fringe_cap);
            Stmt stubs(db_, "SELECT p.f, r.id, COALESCE(r.glyph,''), COALESCE(c.path,'') FROM"
                            " (SELECT DISTINCT f FROM temp.far) p LEFT JOIN runes r INDEXED BY runes_by_name"
                            " ON r.mantle=?1 AND r.name=p.f AND r.deleted=0"
                            " LEFT JOIN clusters c ON c.rune=r.id AND c.always=0 ORDER BY p.f");
            Stmt tags(db_, "SELECT tag FROM tags WHERE rune=? ORDER BY rowid");
            stubs.integer(1, id);
            while (stubs.row()) {
                Stub s;
                s.name = std::string(stubs.col(0));
                s.exists = !stubs.is_null(1);
                s.glyph = std::string(stubs.col(2));
                s.leaf = std::string(stubs.col(3));
                if (s.exists) {
                    tags.integer(1, stubs.col_int(1));
                    while (tags.row()) s.tags.emplace_back(tags.col(0));
                    tags.reset();
                }
                f.stubs.push_back(std::move(s));
            }
        }
        /* La cuenta: a leaf is complete when all its runes are held and none
         * of them has an edge the document leaves out (any that leaves, with
         * no fringe; a hub's unlisted ones, with one). */
        if (sel.by_cluster.count(name)) {
            std::map<std::string, long long> held;
            {
                Stmt q(db_, "SELECT c.path, COUNT(*) FROM temp.ws w JOIN runes r INDEXED BY runes_by_name"
                            " ON r.mantle=?1 AND r.name=w.name AND r.deleted=0 JOIN clusters c ON c.rune=r.id"
                            " WHERE c.always=0 GROUP BY c.path ORDER BY c.path");
                q.integer(1, id);
                while (q.row()) held.emplace(std::string(q.col(0)), q.col_int(1));
            }
            std::set<std::string> open;
            {
                Stmt q(db_, sel.fringe ? "SELECT DISTINCT c.path FROM (SELECT l FROM temp.far_all GROUP BY l"
                                         " HAVING COUNT(*)>?2) h JOIN runes r INDEXED BY runes_by_name"
                                         " ON r.mantle=?1 AND r.name=h.l AND r.deleted=0 JOIN clusters c ON c.rune=r.id"
                                       : "SELECT DISTINCT c.path FROM (SELECT DISTINCT l FROM temp.far_all) h"
                                         " JOIN runes r INDEXED BY runes_by_name ON r.mantle=?1 AND r.name=h.l"
                                         " AND r.deleted=0 JOIN clusters c ON c.rune=r.id");
                q.integer(1, id);
                if (sel.fringe) q.integer(2, cap);
                while (q.row()) open.emplace(q.col(0));
            }
            Stmt size(db_, "SELECT COUNT(*) FROM clusters c INDEXED BY clusters_by_path JOIN runes r ON r.id=c.rune"
                           " WHERE c.mantle=? AND c.path=? AND r.deleted=0");
            for (const auto& [path, k] : held) {
                if (open.count(path)) continue;
                size.integer(1, id).text(2, path);
                const long long all = size.row() ? size.col_int(0) : -1;
                size.reset();
                if (all == k) f.complete_leaves.push_back(path);
            }
        }
        out.mantles.emplace(name, std::move(f));
    }
    return out;
}

std::string Store::state(const Selection& sel) {
    std::ostringstream out;
    write_state(out, sel);
    return out.str();
}

std::vector<MantleInfo> Store::mantles() {
    std::vector<MantleInfo> out;
    if (!db_) return out;
    Stmt q(db_, "SELECT name, (SELECT COUNT(*) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
                " (SELECT COUNT(*) FROM edges e WHERE e.mantle=m.id AND e.deleted=0),"
                " (SELECT COALESCE(SUM(bytes),0) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
                " (SELECT COALESCE(SUM(bytes),0) FROM edges e WHERE e.mantle=m.id AND e.deleted=0)"
                " FROM mantles m WHERE m.deleted=0 ORDER BY pos");
    while (q.row())
        out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2), q.col_int(3) + q.col_int(4), q.col_int(3)});
    return out;
}

/* Rancho (2026-11-06): the live mantles' names in order, without counting
 * their rows (mantles() reads every one). */
std::vector<std::string> Store::mantle_names() {
    std::vector<std::string> out;
    if (!db_) return out;
    Stmt q(db_, "SELECT name FROM mantles WHERE deleted=0 ORDER BY pos");
    while (q.row()) out.emplace_back(q.col(0));
    return out;
}

std::vector<MantleRef> Store::mantle_refs() {
    std::vector<MantleRef> out;
    if (!db_) return out;
    Stmt q(db_, "SELECT mid, name FROM mantles WHERE deleted=0 ORDER BY pos");
    while (q.row()) {
        const std::string mid(q.col(0)), name(q.col(1));
        out.push_back({mid.empty() ? "name:" + name : mid, name});
    }
    return out;
}

std::vector<std::string> Store::top_keys() {
    std::vector<std::string> out;
    if (!db_) return out;
    Stmt q(db_, "SELECT key FROM top ORDER BY pos");
    while (q.row()) out.emplace_back(q.col(0));
    return out;
}

std::optional<std::string> Store::top(const std::string& key) {
    if (!db_) return std::nullopt;
    Stmt q(db_, "SELECT json FROM top WHERE key=?");
    q.text(1, key);
    if (!q.row() || q.is_null(0)) return std::nullopt;
    return std::string(q.col(0));
}

long long Store::rune_count(const std::string& mantle) {
    if (!db_) return 0;
    if (mantle.empty()) {
        Stmt q(db_, "SELECT COUNT(*) FROM runes WHERE deleted=0");
        return q.row() ? q.col_int(0) : 0;
    }
    Stmt q(db_, "SELECT COUNT(*) FROM runes WHERE deleted=0 AND mantle=(SELECT id FROM mantles WHERE name=?"
                " AND deleted=0)");
    q.text(1, mantle);
    return q.row() ? q.col_int(0) : 0;
}

// ── queries (Rancho) ─────────────────────────────────────────────────────────

namespace {

/* THE NARROWING, not the grammar. SPEC §5 requires a host to evaluate tag
 * expressions through vc_tag_match, and every candidate below is decided by it.
 * What this reads is only the expression's AND/OR shape, to ask the tag index
 * for a SUPERSET of the answer: an atom's candidates are the runes carrying the
 * tag (or named it, or of that glyph); AND intersects, OR unites, and NOT, or
 * anything this does not follow, gives up narrowing ("every rune"), which is
 * always safe. The tokenizer and the descent copy Void Core's tags/tag.c
 * (0.2.15) step for step, implicit AND and all, so the shape read is the shape
 * Core evaluates; query_test.cpp checks the answers against `ls --tag`. */
struct Cand {
    bool all = true;
    std::string sql; // a compound SELECT of rune ids, when !all
};

struct Narrow {
    std::vector<std::string> toks, params; // params bind from ?2 (?1 is the mantle)
    std::size_t p = 0;

    static bool ci_eq(const std::string& a, const char* b) {
        std::size_t i = 0;
        for (; i < a.size() && b[i]; ++i)
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
        return i == a.size() && !b[i];
    }
    static bool is_or(const std::string& s) { return s == "||" || ci_eq(s, "OR"); }
    static bool is_and(const std::string& s) { return s == "&&" || ci_eq(s, "AND"); }
    static bool is_not(const std::string& s) { return s == "!" || ci_eq(s, "NOT"); }
    static bool starts_atom(const std::string& s) { return !is_or(s) && !is_and(s) && s != ")"; }

    explicit Narrow(const std::string& e) {
        std::size_t i = 0;
        while (i < e.size()) {
            while (i < e.size() && std::isspace((unsigned char)e[i])) ++i;
            if (i >= e.size()) break;
            const char c = e[i];
            if (c == '(' || c == ')' || c == '!') {
                toks.emplace_back(1, c);
                ++i;
                continue;
            }
            if (c == '&' && i + 1 < e.size() && e[i + 1] == '&') {
                toks.emplace_back("&&");
                i += 2;
                continue;
            }
            if (c == '|' && i + 1 < e.size() && e[i + 1] == '|') {
                toks.emplace_back("||");
                i += 2;
                continue;
            }
            const std::size_t start = i;
            while (i < e.size() && !std::isspace((unsigned char)e[i]) && e[i] != '(' && e[i] != ')') ++i;
            toks.push_back(e.substr(start, i - start));
        }
    }

    std::string param(const std::string& v) {
        params.push_back(v);
        return "?" + std::to_string(params.size() + 1);
    }
    static Cand none() { return {false, "SELECT id FROM runes WHERE 0"}; }
    static Cand both(Cand a, Cand b) {
        if (a.all) return b;
        if (b.all) return a;
        return {false, "SELECT id FROM (" + a.sql + ") INTERSECT SELECT id FROM (" + b.sql + ")"};
    }
    static Cand either(Cand a, Cand b) {
        if (a.all || b.all) return {};
        return {false, "SELECT id FROM (" + a.sql + ") UNION SELECT id FROM (" + b.sql + ")"};
    }

    Cand tag(const std::string& t) {
        const std::string k = param(t);
        /* From the covering tag index alone: rows of other mantles and soft-
         * deleted ones are dropped by the outer query, once, not per atom. */
        std::string sql = "SELECT rune AS id FROM tags WHERE tag=" + k +
                          " UNION SELECT id FROM runes WHERE mantle=?1 AND name=" + k;
        if (t.rfind("glyph:", 0) == 0 && t.size() > 6)
            sql += " UNION SELECT id FROM runes WHERE mantle=?1 AND glyph=" + param(t.substr(6));
        return {false, sql};
    }
    Cand atom() {
        if (p >= toks.size()) return none();
        const std::string s = toks[p];
        if (s == "(") {
            ++p;
            Cand r = any();
            if (p < toks.size() && toks[p] == ")") ++p;
            return r;
        }
        ++p;
        return tag(s);
    }
    Cand negation() {
        if (p < toks.size() && is_not(toks[p])) {
            ++p;
            negation(); // consumed as Core consumes it; a NOT narrows nothing
            return {};
        }
        return atom();
    }
    Cand all_of() {
        Cand r = negation();
        while (p < toks.size()) {
            if (is_and(toks[p])) {
                ++p;
                r = both(r, negation());
            } else if (starts_atom(toks[p])) {
                r = both(r, negation());
            } else {
                break;
            }
        }
        return r;
    }
    Cand any() {
        Cand r = all_of();
        while (p < toks.size() && is_or(toks[p])) {
            ++p;
            r = either(r, all_of());
        }
        return r;
    }
};

std::string quoted(const std::string& s) { return nlohmann::json(s).dump(); }

} // namespace

std::vector<std::string> Store::select(const std::string& mantle, const std::string& expr, long long limit,
                                       long long offset) {
    std::vector<std::string> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Narrow n(expr);
    const bool empty = n.toks.empty(); // matches every rune
    const Cand c = empty ? Cand{} : n.any();
    /* Narrowed, the query is driven FROM the candidates (CROSS JOIN fixes the
     * order), so it costs what it finds: written as `r.id IN (...)`, the
     * planner walked the whole mantle in order to skip a sort — the trap
     * Hormiga's selections fell into, 13x slower at 8×10^4 runes. */
    std::string sql = "SELECT r.name, r.glyph, (SELECT json_group_array(tag) FROM tags WHERE rune=r.id)";
    if (c.all) sql += " FROM runes r WHERE r.mantle=?1 AND r.deleted=0";
    else sql += " FROM (" + c.sql + ") cand CROSS JOIN runes r ON r.id=cand.id WHERE r.mantle=?1 AND r.deleted=0";
    sql += " ORDER BY r.pos";
    Stmt q(db_, sql);
    if (!q.ok()) {
        error_ = sqlite3_errmsg(db_);
        return out;
    }
    q.integer(1, mid);
    for (std::size_t i = 0; i < n.params.size(); ++i) q.text((int)i + 2, n.params[i]);
    long long skipped = 0;
    while (q.row()) {
        const std::string name(q.col(0)), glyph(q.col(1));
        if (!empty) {
            /* The rune as `ls --tag` sees it: its tags, its name, glyph:<g>. */
            const std::string_view arr = q.col(2);
            std::string tags = "[" + quoted(name);
            if (!glyph.empty()) tags += "," + quoted("glyph:" + glyph);
            if (arr.size() > 2) tags += "," + std::string(arr.substr(1));
            else tags += "]";
            if (vc_tag_match(expr.c_str(), tags.c_str()) != 1) continue;
        }
        if (skipped++ < offset) continue;
        out.push_back(name);
        if (limit > 0 && (long long)out.size() >= limit) break;
    }
    return out;
}

long long Store::count(const std::string& mantle, const std::string& expr) {
    return (long long)select(mantle, expr).size();
}

std::vector<std::string> Store::range(const std::string& mantle, const std::string& field, double lo, double hi,
                                      long long limit) {
    std::vector<std::string> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT r.name FROM fields f INDEXED BY fields_by_num JOIN runes r ON r.id=f.rune"
                " WHERE f.field=?2 AND f.num BETWEEN ?3 AND ?4 AND r.mantle=?1 AND r.deleted=0 ORDER BY f.num, r.pos");
    q.integer(1, mid).text(2, field).real(3, lo).real(4, hi);
    while (q.row()) {
        out.emplace_back(q.col(0));
        if (limit > 0 && (long long)out.size() >= limit) break;
    }
    return out;
}

std::vector<TagRow> Store::tags_of(const std::string& mantle, const std::string& rune) {
    std::vector<TagRow> out;
    Stmt q(db_, "SELECT t.tag, COALESCE(t.source,'') FROM tags t JOIN runes r ON r.id=t.rune"
                " WHERE r.mantle=(SELECT id FROM mantles WHERE name=?1 AND deleted=0) AND r.name=?2 AND r.deleted=0"
                " ORDER BY t.rowid");
    q.text(1, mantle).text(2, rune);
    while (q.row()) out.push_back({std::string(q.col(0)), std::string(q.col(1))});
    return out;
}

long long Store::row_of(const std::string& mantle, const std::string& rid) {
    const long long mid = mantle_row(mantle);
    if (mid < 0) return -1;
    Stmt q(db_, "SELECT row FROM rows WHERE mkey=? AND rid=?");
    q.text(1, mantle_key(mid)).text(2, rid);
    return q.row() ? q.col_int(0) : -1;
}

// ── the row index (Rancho) ───────────────────────────────────────────────────

std::string Store::mantle_key(long long mantle) {
    Stmt q(db_, "SELECT mid, name FROM mantles WHERE id=?");
    q.integer(1, mantle);
    if (!q.row()) return {};
    const std::string mid(q.col(0));
    return mid.empty() ? "name:" + std::string(q.col(1)) : mid;
}

void Store::assign_rows(long long mantle) {
    const std::string key = mantle_key(mantle);
    if (key.empty()) return;
    long long base = 0;
    {
        Stmt q(db_, "SELECT COALESCE(MAX(row), -1) + 1 FROM rows WHERE mkey=?");
        q.text(1, key);
        if (q.row()) base = q.col_int(0);
    }
    Stmt add(db_, "INSERT OR IGNORE INTO rows(mkey, rid, row) SELECT ?1, rid, ?2 + ROW_NUMBER() OVER (ORDER BY pos) - 1"
                  " FROM (SELECT r.rid, MIN(r.pos) AS pos FROM runes r WHERE r.mantle=?3 AND r.deleted=0"
                  " AND r.rid IS NOT NULL AND r.rid<>'' AND NOT EXISTS(SELECT 1 FROM rows x WHERE x.mkey=?1"
                  " AND x.rid=r.rid) GROUP BY r.rid)");
    if (!add.text(1, key).integer(2, base).integer(3, mantle).run()) error_ = sqlite3_errmsg(db_);
}

void Store::assign_row(long long mantle, const std::string& rid) {
    if (rid.empty()) return;
    const std::string key = mantle_key(mantle);
    Stmt add(db_, "INSERT OR IGNORE INTO rows(mkey, rid, row)"
                  " VALUES(?1, ?2, (SELECT COALESCE(MAX(row), -1) + 1 FROM rows WHERE mkey=?1))");
    add.text(1, key).text(2, rid).run();
}

// ── typed columns (Rancho) ───────────────────────────────────────────────────

void Store::load_kinds() {
    kinds_.clear();
    measures_.clear();
    auto take = [&](const std::string& text) {
        const auto g = nlohmann::json::parse(text, nullptr, false);
        if (!g.is_object()) return;
        for (const auto& [name, d] : g.items()) {
            if (!d.is_object()) continue;
            if (str_of(d, "kind") == "measure") measures_.insert(name);
            if (const auto k = d.find("kinds"); k != d.end() && k->is_object())
                for (const auto& [field, q] : k->items())
                    if (q.is_object()) kinds_[name][field] = str_of(q, "level");
        }
    };
    { // registered first: a declaration in the document wins, as it does in Core
        Stmt q(db_, "SELECT value FROM store_meta WHERE key='registered_glyphs'");
        if (q.row()) take(std::string(q.col(0)));
    }
    Stmt q(db_, "SELECT json FROM top WHERE key='glyphs'");
    if (q.row()) take(std::string(q.col(0)));
}

void Store::index_fields(long long rune, const std::string& glyph, const std::string& json) {
    Stmt del(db_, "DELETE FROM fields WHERE rune=?");
    del.integer(1, rune).run();
    const auto k = kinds_.find(glyph);
    if (k == kinds_.end()) return;
    const auto j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) return;
    const auto content = j.find("content");
    if (content == j.end() || !content->is_object()) return;
    Stmt add(db_, "INSERT OR REPLACE INTO fields VALUES(?,?,?,?)");
    for (const auto& [field, level] : k->second) {
        const auto v = content->find(field);
        if (v == content->end()) continue;
        std::optional<double> num;
        std::optional<std::string> text;
        if (!field_value(*v, level, num, text)) continue;
        add.integer(1, rune).text(2, field);
        if (num) add.real(3, *num);
        else add.null(3);
        if (text) add.text(4, *text);
        else add.null(4);
        add.run();
    }
}

void Store::reindex_fields() {
    if (!db_) return;
    exec("DELETE FROM fields");
    if (kinds_.empty()) return;
    Stmt q(db_, "SELECT id, json FROM runes WHERE deleted=0 AND glyph=?");
    for (const auto& [glyph, _] : kinds_) {
        q.text(1, glyph);
        std::vector<std::pair<long long, std::string>> rows;
        while (q.row()) rows.emplace_back(q.col_int(0), std::string(q.col(1)));
        q.reset();
        for (const auto& [id, js] : rows) index_fields(id, glyph, js);
    }
}

void Store::set_registered_glyphs(const std::string& glyphs_json) {
    set_meta("registered_glyphs", glyphs_json);
    load_kinds();
    reindex_fields();
}

std::set<std::string> Store::measure_glyphs() { return measures_; }

std::map<std::string, std::map<std::string, std::string>> Store::field_levels() { return kinds_; }

// ── blobs (Rancho) ───────────────────────────────────────────────────────────

void Store::put_blob(const std::string& path, const std::string& bytes) {
    Stmt q(db_, "INSERT OR REPLACE INTO blobs VALUES(?,?,?)");
    q.text(1, path).blob(2, bytes).integer(3, (long long)bytes.size()).run();
}

std::optional<std::string> Store::blob(const std::string& path) {
    Stmt q(db_, "SELECT bytes FROM blobs WHERE path=?");
    q.text(1, path);
    if (!q.row()) return std::nullopt;
    return q.col_blob(0);
}

std::vector<std::string> Store::blob_paths() {
    std::vector<std::string> out;
    Stmt q(db_, "SELECT path FROM blobs ORDER BY path");
    while (q.row()) out.emplace_back(q.col(0));
    return out;
}

bool Store::remove_blob(const std::string& path) {
    if (!db_) return false;
    Stmt q(db_, "DELETE FROM blobs WHERE path=?");
    q.text(1, path).run();
    return sqlite3_changes(db_) > 0;
}

std::size_t Store::clear_blobs() {
    if (!db_) return 0;
    if (!exec("DELETE FROM blobs")) return 0;
    return (std::size_t)sqlite3_changes(db_);
}

bool Store::compact() { return db_ && exec("VACUUM"); }

long long Store::release_memory() {
    if (!db_) return 0;
    const sqlite3_int64 before = sqlite3_memory_used();
    sqlite3_db_release_memory(db_);
    const sqlite3_int64 after = sqlite3_memory_used();
    return before > after ? (long long)(before - after) : 0;
}

// ── write-back ───────────────────────────────────────────────────────────────

bool Store::begin() { return db_ && exec("BEGIN"); }
bool Store::commit() { return db_ && exec("COMMIT"); }
void Store::rollback() {
    if (db_) exec("ROLLBACK");
}

void Store::put_top(const std::string& key, const std::string& json) {
    Stmt q(db_, "INSERT INTO top(key, pos, json) VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM top), ?2)"
                " ON CONFLICT(key) DO UPDATE SET json=excluded.json");
    q.text(1, key).text(2, json).run();
    if (key == "glyphs") { // Rancho: a declaration changed what the typed columns are
        load_kinds();
        reindex_fields();
    }
}

void Store::keep_only_top(const std::set<std::string>& keys) {
    std::vector<std::string> gone;
    {
        Stmt q(db_, "SELECT key FROM top");
        while (q.row()) {
            const std::string k(q.col(0));
            if (k != "mantles" && !keys.count(k)) gone.push_back(k);
        }
    }
    Stmt del(db_, "DELETE FROM top WHERE key=?");
    for (const auto& k : gone) del.text(1, k).run();
    Stmt m(db_, "INSERT OR IGNORE INTO top VALUES('mantles', (SELECT COALESCE(MAX(pos), -1) + 1 FROM top), NULL)");
    m.run();
    if (std::find(gone.begin(), gone.end(), "glyphs") != gone.end()) {
        load_kinds();
        reindex_fields();
    }
}

namespace {
std::string mantle_tag(const std::string& mid, long long row) {
    return mid.empty() ? "mantle#" + std::to_string(row) : "mantle:" + mid;
}
} // namespace

long long Store::put_mantle(const MantleRow& m, int pos) {
    long long id = -1;
    if (!m.mid.empty()) {
        Stmt live(db_, "SELECT id FROM mantles WHERE mid=? AND deleted=0");
        live.text(1, m.mid);
        if (live.row()) id = live.col_int(0);
        live.reset();
        if (id < 0) { // a mantle removed in this session and now back (an undo): with what it took
            Stmt gone(db_, "SELECT id FROM mantles WHERE mid=? AND deleted=1 ORDER BY id DESC LIMIT 1");
            gone.text(1, m.mid);
            if (gone.row()) id = gone.col_int(0);
            gone.reset();
            if (id >= 0) {
                const std::string tag = mantle_tag(m.mid, id);
                Stmt a(db_, "UPDATE mantles SET deleted=0, deleted_by=NULL WHERE id=?");
                a.integer(1, id).run();
                Stmt b(db_, "UPDATE runes SET deleted=0, deleted_by=NULL WHERE deleted_by=?");
                b.text(1, tag).run();
                Stmt c(db_, "UPDATE edges SET deleted=0, deleted_by=NULL WHERE deleted_by=?");
                c.text(1, tag).run();
            }
        }
    } else {
        Stmt byname(db_, "SELECT id FROM mantles WHERE name=? AND deleted=0");
        byname.text(1, m.name);
        if (byname.row()) id = byname.col_int(0);
        byname.reset();
    }
    if (id < 0) {
        Stmt add(db_, "INSERT INTO mantles(pos) VALUES(?)");
        add.integer(1, pos).run();
        id = sqlite3_last_insert_rowid(db_);
    }
    Stmt up(db_, "UPDATE mantles SET pos=?, name=?, json=?, layout=?, has_runes=?, has_edges=?, mid=? WHERE id=?");
    up.integer(1, pos).text(2, m.name).text(3, m.skeleton);
    if (m.layout) up.text(4, *m.layout);
    else up.null(4);
    up.integer(5, m.has_runes ? 1 : 0).integer(6, m.has_edges ? 1 : 0).text(7, m.mid).integer(8, id).run();
    return id;
}

void Store::delete_mantle(long long mantle) {
    std::string mid;
    {
        Stmt q(db_, "SELECT mid FROM mantles WHERE id=?");
        q.integer(1, mantle);
        if (q.row()) mid = std::string(q.col(0));
    }
    const std::string tag = mantle_tag(mid, mantle);
    Stmt a(db_, "UPDATE mantles SET deleted=1, deleted_by=?1 WHERE id=?2");
    a.text(1, tag).integer(2, mantle).run();
    Stmt b(db_, "UPDATE runes SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0");
    b.text(1, tag).integer(2, mantle).run();
    Stmt c(db_, "UPDATE edges SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0");
    c.text(1, tag).integer(2, mantle).run();
}

void Store::put_rune(long long mantle, const std::string& name, const std::string& json) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    const nlohmann::json sp = j.is_object() ? j.value("spirit", nlohmann::json::object()) : nlohmann::json::object();
    const std::string rid = str_of(sp, "id"), glyph = j.is_object() ? str_of(j, "glyph") : std::string();
    long long row = -1;
    {
        Stmt q(db_, "SELECT id FROM runes WHERE mantle=? AND name=? AND deleted=0");
        q.integer(1, mantle).text(2, name);
        if (q.row()) row = q.col_int(0);
    }
    /* Rancho: a tag that was already there keeps who wrote it. */
    std::unordered_map<std::string, std::string> had;
    if (row >= 0) {
        {
            Stmt q(db_, "SELECT tag, source FROM tags WHERE rune=?");
            q.integer(1, row);
            while (q.row()) had.emplace(std::string(q.col(0)), q.is_null(1) ? std::string("\x01") : std::string(q.col(1)));
        }
        Stmt up(db_, "UPDATE runes SET rid=?, glyph=?, json=?, bytes=? WHERE id=?");
        up.text(1, rid).text(2, glyph).text(3, json).integer(4, (long long)json.size()).integer(5, row).run();
        Stmt drop(db_, "DELETE FROM tags WHERE rune=?");
        drop.integer(1, row).run();
    } else {
        Stmt add(db_, "INSERT INTO runes(mantle, pos, name, rid, glyph, json, bytes)"
                      " VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM runes WHERE mantle=?1), ?2, ?3, ?4, ?5, ?6)");
        add.integer(1, mantle).text(2, name).text(3, rid).text(4, glyph).text(5, json)
            .integer(6, (long long)json.size()).run();
        row = sqlite3_last_insert_rowid(db_);
    }
    Stmt tag(db_, "INSERT INTO tags VALUES(?,?,?)");
    if (j.is_object())
        if (const auto t = j.find("tags"); t != j.end() && t->is_array())
            for (const auto& x : *t)
                if (x.is_string()) {
                    const std::string s = x.get<std::string>();
                    const auto h = had.find(s);
                    tag.integer(1, row).text(2, s);
                    if (h == had.end()) tag.text(3, source_);
                    else if (h->second == "\x01") tag.null(3);
                    else tag.text(3, h->second);
                    tag.run();
                }
    assign_row(mantle, rid);
    index_fields(row, glyph, json);
}

void Store::rename_rune(long long mantle, const std::string& from, const std::string& to) {
    Stmt r(db_, "UPDATE runes SET name=?1 WHERE mantle=?2 AND name=?3 AND deleted=0");
    r.text(1, to).integer(2, mantle).text(3, from).run();
    // every live edge that names it, loaded or not: Core would have renamed them all had it held them
    std::vector<std::pair<long long, std::string>> rows;
    {
        Stmt q(db_, "SELECT id, json FROM edges WHERE mantle=?1 AND deleted=0 AND (src=?2 OR dst=?2)");
        q.integer(1, mantle).text(2, from);
        while (q.row()) rows.emplace_back(q.col_int(0), std::string(q.col(1)));
    }
    Stmt up(db_, "UPDATE edges SET src=?1, dst=?2, json=?3, bytes=length(?3) WHERE id=?4");
    for (auto& [id, js] : rows) {
        nlohmann::json e = nlohmann::json::parse(js, nullptr, false);
        if (!e.is_object()) continue;
        if (str_of(e, "from") == from) e["from"] = to;
        if (str_of(e, "to") == from) e["to"] = to;
        const std::string dumped = e.dump();
        up.text(1, str_of(e, "from")).text(2, str_of(e, "to")).text(3, dumped).integer(4, id).run();
    }
}

void Store::delete_rune(long long mantle, const std::string& name) {
    long long row = -1;
    std::string rid;
    {
        Stmt q(db_, "SELECT id, rid FROM runes WHERE mantle=? AND name=? AND deleted=0");
        q.integer(1, mantle).text(2, name);
        if (q.row()) {
            row = q.col_int(0);
            rid = std::string(q.col(1));
        }
    }
    if (row < 0) return;
    if (summarized(mantle)) tally_rune(mantle, name, -1); // Rancho: its count and its ties leave the reseña
    const std::string tag = rid.empty() ? "rune#" + std::to_string(row) : "rune:" + rid;
    Stmt a(db_, "UPDATE runes SET deleted=1, deleted_by=?1 WHERE id=?2");
    a.text(1, tag).integer(2, row).run();
    Stmt b(db_, "UPDATE edges SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0 AND (src=?3 OR dst=?3)");
    b.text(1, tag).integer(2, mantle).text(3, name).run();
}

bool Store::restore_rune(long long mantle, const std::string& rid) {
    if (rid.empty()) return false;
    long long row = -1;
    std::string name;
    {
        Stmt q(db_, "SELECT id, name FROM runes WHERE mantle=? AND rid=? AND deleted=1 ORDER BY id DESC LIMIT 1");
        q.integer(1, mantle).text(2, rid);
        if (q.row()) {
            row = q.col_int(0);
            name = std::string(q.col(1));
        }
    }
    if (row < 0) return false;
    {
        Stmt clash(db_, "SELECT 1 FROM runes WHERE mantle=? AND name=? AND deleted=0");
        clash.integer(1, mantle).text(2, name);
        if (clash.row()) return false; // its name was taken since: it comes back as a new rune
    }
    Stmt a(db_, "UPDATE runes SET deleted=0, deleted_by=NULL WHERE id=?");
    a.integer(1, row).run();
    // its edges, where the other end is still there
    Stmt b(db_, "UPDATE edges SET deleted=0, deleted_by=NULL WHERE deleted_by=?1 AND mantle=?2 AND EXISTS("
                " SELECT 1 FROM runes r WHERE r.mantle=?2 AND r.deleted=0"
                " AND r.name=(CASE WHEN edges.src=?3 THEN edges.dst ELSE edges.src END))");
    b.text(1, "rune:" + rid).integer(2, mantle).text(3, name).run();
    if (summarized(mantle)) tally_rune(mantle, name, +1);
    return true;
}

void Store::set_edges(long long mantle, const std::string& src, const std::string& dst, const std::string& relation,
                      const std::vector<std::string>& jsons) {
    struct Row {
        long long id;
        nlohmann::json j;
        double weight;
        bool used = false;
    };
    std::vector<Row> have;
    {
        Stmt q(db_, "SELECT id, json, weight FROM edges WHERE mantle=? AND src=? AND dst=? AND relation=?"
                    " AND deleted=0 ORDER BY pos");
        q.integer(1, mantle).text(2, src).text(3, dst).text(4, relation);
        while (q.row()) have.push_back({q.col_int(0), nlohmann::json::parse(q.col(1), nullptr, false), q.col_real(2)});
    }
    /* Rancho: the reseña follows every tie made, reweighed or removed. */
    std::string pu, pv;
    if (summarized(mantle)) {
        pu = leaf_of(mantle, src);
        pv = src == dst ? pu : leaf_of(mantle, dst);
    }
    std::vector<const std::string*> missing;
    for (const auto& js : jsons) {
        const nlohmann::json want = nlohmann::json::parse(js, nullptr, false);
        auto it = std::find_if(have.begin(), have.end(), [&](const Row& r) { return !r.used && r.j == want; });
        if (it != have.end()) it->used = true;
        else missing.push_back(&js);
    }
    Stmt up(db_, "UPDATE edges SET json=?1, bytes=length(?1), weight=?2, directed=?3 WHERE id=?4");
    Stmt add(db_, "INSERT INTO edges(mantle, pos, src, dst, relation, json, bytes, weight, directed, from_port, to_port)"
                  " VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM edges WHERE mantle=?1), ?2, ?3, ?4, ?5, ?6,"
                  " ?7, ?8, ?9, ?10)");
    for (const std::string* js : missing) {
        const EdgeCols c = edge_cols(nlohmann::json::parse(*js, nullptr, false));
        auto spare = std::find_if(have.begin(), have.end(), [](const Row& r) { return !r.used; });
        if (spare != have.end()) {
            spare->used = true;
            up.text(1, *js).real(2, c.weight).integer(3, c.directed ? 1 : 0).integer(4, spare->id).run();
            tally(mantle, pu, pv, spare->weight, -1);
        } else {
            add.integer(1, mantle);
            bind_edge(add, 2, c, *js);
            add.run();
        }
        tally(mantle, pu, pv, c.weight, +1);
    }
    Stmt del(db_, "DELETE FROM edges WHERE id=?");
    for (const auto& r : have)
        if (!r.used) {
            del.integer(1, r.id).run();
            tally(mantle, pu, pv, r.weight, -1);
        }
}

void Store::purge() {
    exec("DELETE FROM tags WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
         "DELETE FROM clusters WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
         "DELETE FROM fields WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
         "DELETE FROM runes WHERE deleted=1; DELETE FROM edges WHERE deleted=1; DELETE FROM mantles WHERE deleted=1;");
}

long long Store::mantle_row(const std::string& name) {
    Stmt q(db_, "SELECT id FROM mantles WHERE name=? AND deleted=0");
    q.text(1, name);
    return q.row() ? q.col_int(0) : -1;
}

std::optional<std::string> Store::live_rune_id(long long mantle, const std::string& name) {
    Stmt q(db_, "SELECT rid FROM runes WHERE mantle=? AND name=? AND deleted=0");
    q.integer(1, mantle).text(2, name);
    if (!q.row()) return std::nullopt;
    return std::string(q.col(0));
}

// ── clusters ─────────────────────────────────────────────────────────────────

std::string ClusterOptions::settings_json() const {
    nlohmann::json j = {{"algorithm", "leiden"}, {"implementation", "rancho/graph.hpp"}, {"target", target},
                        {"seed", seed}, {"resolution", resolution}, {"relations", relations},
                        {"weighted", weighted}, {"ties", "undirected; value edges excluded"},
                        {"fanout", fanout}};
    return j.dump();
}

ClusterStats Store::cluster(const std::string& mantle, std::uint32_t target, const Progress& progress) {
    ClusterOptions o;
    o.target = target;
    return cluster(mantle, o, progress);
}

ClusterStats Store::cluster(const std::string& mantle, const ClusterOptions& opt, const Progress& progress) {
    ClusterStats out;
    const auto t0 = std::chrono::steady_clock::now();
    const long long mid = mantle_row(mantle);
    if (mid < 0) {
        out.error = "no such mantle: " + mantle;
        return out;
    }
    /* Rancho: weights of different relations are not commensurable (Void
     * Core's quantity page; SPEC §3.7.1), so a weighted clustering names the
     * one relation whose weights it reads. */
    if (opt.weighted && opt.relations.size() != 1) {
        out.error = "a weighted clustering reads ONE relation's weights: weights of different relations are not "
                    "commensurable";
        return out;
    }
    const std::set<std::string> rels(opt.relations.begin(), opt.relations.end());

    // the glyphs declared as measures: their runes are values, not places in the graph
    const std::set<std::string>& measures = measures_;
    if (progress) progress(0.05, "Reading the links");
    std::vector<long long> rows;
    std::vector<char> always;
    std::unordered_map<std::string, std::uint32_t> index;
    {
        Stmt q(db_, "SELECT id, name, glyph FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos");
        q.integer(1, mid);
        while (q.row()) {
            index.emplace(std::string(q.col(1)), (std::uint32_t)rows.size());
            rows.push_back(q.col_int(0));
            always.push_back(measures.count(std::string(q.col(2))) ? 1 : 0);
        }
    }
    std::vector<graph::Edge> edges;
    {
        Stmt q(db_, "SELECT src, dst, relation, weight FROM edges WHERE mantle=? AND deleted=0");
        q.integer(1, mid);
        while (q.row()) {
            if (!rels.empty() && !rels.count(std::string(q.col(2)))) continue;
            const auto a = index.find(std::string(q.col(0))), b = index.find(std::string(q.col(1)));
            if (a == index.end() || b == index.end() || a->second == b->second) continue;
            if (always[a->second] || always[b->second]) continue; // a value edge is not a tie
            const double w = opt.weighted ? q.col_real(3) : 1.0;
            if (!(w > 0) || !std::isfinite(w)) {
                out.error = "a weighted clustering needs positive weights; " + std::string(q.col(0)) + " -> " +
                            std::string(q.col(1)) + " has " + std::to_string(w);
                return out;
            }
            edges.push_back({a->second, b->second, w});
        }
    }
    index.clear();
    if (progress) progress(0.3, "Finding the clusters");
    const auto g = graph::Graph::from_edges((std::uint32_t)rows.size(), edges);
    edges.clear();
    edges.shrink_to_fit();
    graph::LeidenOptions lo;
    lo.seed = opt.seed; // the same database clusters the same way on every device
    lo.resolution = opt.resolution;
    auto h = graph::hierarchy(g, opt.target, lo);
    std::vector<std::uint32_t> level1(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) level1[i] = h.path[i].empty() ? 0 : h.path[i][0];
    out.modularity = graph::modularity(g, level1, opt.resolution); // of Leiden's communities, before packing
    out.depth = h.depth;
    /* Rancho (2026-11-09): levels above, so every node of the tree has at
     * most `fanout` children (Hormiga's "the overview"). */
    std::map<std::string, int> kinds;
    if (opt.fanout > 0) {
        if (progress) progress(0.7, "Building the levels above");
        graph::Bounded b = graph::bound_fanout(g, h, opt.fanout);
        h.path = std::move(b.path);
        out.depth = b.depth;
        for (const auto& [p, k] : b.packing) {
            std::string s;
            for (std::size_t i = 0; i < p.size(); ++i) s += (i ? "." : "") + std::to_string(p[i]);
            kinds.emplace(std::move(s), (int)k);
        }
    }
    {
        std::set<std::uint32_t> tops;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (!always[i] && !h.path[i].empty()) tops.insert(h.path[i][0]);
        out.roots = (long long)tops.size();
    }

    if (progress) progress(0.8, "Filing the records");
    if (!begin()) {
        out.error = error_;
        return out;
    }
    {
        Stmt del(db_, "DELETE FROM clusters WHERE mantle=?");
        del.integer(1, mid).run();
    }
    std::set<std::string> leaves;
    Stmt add(db_, "INSERT OR REPLACE INTO clusters VALUES(?,?,?,?)");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::string path;
        if (always[i]) {
            path = "!"; // sorts before every number: loaded with any cluster
            ++out.always;
        } else {
            for (std::size_t k = 0; k < h.path[i].size(); ++k)
                path += (k ? "." : "") + std::to_string(h.path[i][k]);
            leaves.insert(path);
        }
        add.integer(1, rows[i]).integer(2, mid).text(3, path).integer(4, always[i] ? 1 : 0).run();
    }
    {
        Stmt run(db_, "INSERT OR REPLACE INTO cluster_runs VALUES(?,?,?,?,?)");
        run.integer(1, mid).text(2, opt.settings_json()).real(3, out.modularity)
            .integer(4, (long long)leaves.size()).integer(5, out.depth).run();
    }
    if (progress) progress(0.9, "Counting the ties");
    summarize(mid, kinds);
    refresh_estimates();
    if (!commit()) {
        out.error = error_;
        rollback();
        return out;
    }
    out.ok = true;
    out.runes = (long long)rows.size();
    out.leaves = (long long)leaves.size();
    out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (progress) progress(1.0, "Filing the records");
    return out;
}

std::string Store::cluster_settings(const std::string& mantle) {
    const long long mid = mantle_row(mantle);
    Stmt q(db_, "SELECT settings FROM cluster_runs WHERE mantle=?");
    q.integer(1, mid);
    return q.row() ? std::string(q.col(0)) : std::string();
}

std::vector<ClusterLeaf> Store::leaves(const std::string& mantle) {
    std::vector<ClusterLeaf> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT c.path, COUNT(*), SUM(r.bytes) FROM clusters c JOIN runes r ON r.id=c.rune"
                " WHERE c.mantle=? AND r.deleted=0 AND c.always=0 GROUP BY c.path ORDER BY c.path");
    q.integer(1, mid);
    while (q.row()) out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2)});
    return out;
}

bool Store::clustered(const std::string& mantle) {
    const long long mid = mantle_row(mantle);
    if (mid < 0) return false;
    Stmt q(db_, "SELECT 1 FROM clusters WHERE mantle=? LIMIT 1");
    q.integer(1, mid);
    return q.row();
}

std::string Store::cluster_of(const std::string& mantle, const std::string& rune) {
    const long long mid = mantle_row(mantle);
    Stmt q(db_, "SELECT c.path FROM runes r JOIN clusters c ON c.rune=r.id WHERE r.mantle=? AND r.name=?"
                " AND r.deleted=0");
    q.integer(1, mid).text(2, rune);
    return q.row() ? std::string(q.col(0)) : std::string();
}

void Store::assign_cluster(const std::string& mantle, const std::string& rune, const std::string& path) {
    const long long mid = mantle_row(mantle);
    /* Rancho: the reseña moves with it. Its ties leave the old leaf's nodes
     * and join the new one's. */
    const bool summed = summarized(mid);
    if (summed) tally_rune(mid, rune, -1);
    Stmt q(db_, "INSERT OR REPLACE INTO clusters SELECT id, mantle, ?3, 0 FROM runes"
                " WHERE mantle=?1 AND name=?2 AND deleted=0");
    q.integer(1, mid).text(2, rune).text(3, path).run();
    if (summed) tally_rune(mid, rune, +1);
}

// ── the reseña (Rancho, 2026-11-09) ───────────────────────────────────────

std::map<std::string, std::string> Store::clusters_of(const std::string& mantle, const std::vector<std::string>& runes) {
    std::map<std::string, std::string> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0 || runes.empty()) return out;
    Stmt q(db_, "SELECT r.name, c.path FROM json_each(?2) j JOIN runes r INDEXED BY runes_by_name"
                " ON r.mantle=?1 AND r.name=j.value AND r.deleted=0 JOIN clusters c ON c.rune=r.id");
    q.integer(1, mid).text(2, nlohmann::json(runes).dump());
    while (q.row()) out.emplace(std::string(q.col(0)), std::string(q.col(1)));
    return out;
}

bool Store::summarized(long long mid) {
    Stmt q(db_, "SELECT 1 FROM cluster_nodes WHERE mantle=? AND path=''");
    q.integer(1, mid);
    return q.row();
}

std::string Store::leaf_of(long long mid, const std::string& name) {
    Stmt q(db_, "SELECT c.path FROM runes r INDEXED BY runes_by_name CROSS JOIN clusters c ON c.rune=r.id"
                " WHERE r.mantle=? AND r.name=? AND r.deleted=0 AND c.always=0");
    q.integer(1, mid).text(2, name);
    return q.row() ? std::string(q.col(0)) : std::string();
}

void Store::tally(long long mid, const std::string& pu, const std::string& pv, double w, int sign) {
    if (pu.empty() || pv.empty()) return; // not a tie: an end is unclustered, a measure, or no rune
    const TieEffect t = tie_effect(pu, pv);
    Stmt node(db_, "INSERT INTO cluster_nodes(mantle, path, parent, internal, outgoing, internal_weight,"
                   " outgoing_weight) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7) ON CONFLICT(mantle, path) DO UPDATE SET"
                   " internal=internal+?4, outgoing=outgoing+?5, internal_weight=internal_weight+?6,"
                   " outgoing_weight=outgoing_weight+?7");
    auto put = [&](const std::string& p, int in, int outg) {
        node.integer(1, mid).text(2, p);
        if (p.empty()) node.null(3);
        else node.text(3, parent_of(p));
        node.integer(4, in * sign).integer(5, outg * sign).real(6, in * sign * w).real(7, outg * sign * w).run();
    };
    for (const auto& p : t.internal) put(p, 1, 0);
    for (const auto& p : t.outgoing) put(p, 0, 1);
    if (!t.a.empty()) {
        Stmt tie(db_, "INSERT INTO cluster_ties VALUES(?1, ?2, ?3, ?4, ?5, ?6) ON CONFLICT(mantle, parent, a, b)"
                      " DO UPDATE SET links=links+?5, weight=weight+?6");
        tie.integer(1, mid).text(2, t.parent).text(3, t.a).text(4, t.b).integer(5, sign).real(6, sign * w).run();
        if (sign < 0) {
            Stmt gone(db_, "DELETE FROM cluster_ties WHERE mantle=? AND parent=? AND a=? AND b=? AND links<=0");
            gone.integer(1, mid).text(2, t.parent).text(3, t.a).text(4, t.b).run();
        }
    }
}

void Store::tally_rune(long long mid, const std::string& name, int sign) {
    const std::string mine = leaf_of(mid, name);
    if (mine.empty()) return;
    Stmt runes(db_, "INSERT INTO cluster_nodes(mantle, path, parent, runes) VALUES(?1, ?2, ?3, ?4)"
                    " ON CONFLICT(mantle, path) DO UPDATE SET runes=runes+?4");
    for (const auto& p : prefixes(mine)) {
        runes.integer(1, mid).text(2, p);
        if (p.empty()) runes.null(3);
        else runes.text(3, parent_of(p));
        runes.integer(4, sign).run();
    }
    std::vector<std::tuple<std::string, std::string, double>> ties;
    {
        Stmt q(db_, "SELECT src, dst, weight FROM edges WHERE mantle=?1 AND deleted=0 AND (src=?2 OR dst=?2)");
        q.integer(1, mid).text(2, name);
        while (q.row()) ties.emplace_back(std::string(q.col(0)), std::string(q.col(1)), q.col_real(2));
    }
    for (const auto& [s, d, w] : ties) {
        const std::string other = s == name ? d : s;
        tally(mid, mine, other == name ? mine : leaf_of(mid, other), w, sign);
    }
}

void Store::summarize(long long mid, const std::map<std::string, int>& kinds) {
    Stmt c1(db_, "DELETE FROM cluster_nodes WHERE mantle=?");
    c1.integer(1, mid).run();
    Stmt c2(db_, "DELETE FROM cluster_ties WHERE mantle=?");
    c2.integer(1, mid).run();
    struct Node {
        long long runes = 0, internal = 0, outgoing = 0;
        double internal_w = 0, outgoing_w = 0;
    };
    std::map<std::string, Node> nodes;
    std::map<std::tuple<std::string, std::string, std::string>, std::pair<long long, double>> ties;
    nodes[""]; // the mantle itself, even when it has no tie
    {
        Stmt q(db_, "SELECT c.path, COUNT(*) FROM clusters c INDEXED BY clusters_by_path CROSS JOIN runes r"
                    " ON r.id=c.rune WHERE c.mantle=? AND c.always=0 AND r.deleted=0 GROUP BY c.path");
        q.integer(1, mid);
        while (q.row()) {
            const long long k = q.col_int(1);
            for (const auto& p : prefixes(q.col(0))) nodes[p].runes += k;
        }
    }
    {
        /* One pass over the mantle's edges, each end's leaf found through
         * the name index: bounded memory, whatever the mantle's size. The
         * order is forced: left to itself, with no statistics, the planner
         * walked the mantle's runes once per edge (quadratic). */
        Stmt q(db_, "SELECT ca.path, cb.path, e.weight FROM edges e INDEXED BY edges_by_pos"
                    " CROSS JOIN runes ra INDEXED BY runes_by_name ON ra.mantle=e.mantle AND ra.name=e.src"
                    " CROSS JOIN clusters ca ON ca.rune=ra.id"
                    " CROSS JOIN runes rb INDEXED BY runes_by_name ON rb.mantle=e.mantle AND rb.name=e.dst"
                    " CROSS JOIN clusters cb ON cb.rune=rb.id"
                    " WHERE e.mantle=? AND e.deleted=0 AND ra.deleted=0 AND rb.deleted=0 AND ca.always=0"
                    " AND cb.always=0");
        q.integer(1, mid);
        while (q.row()) {
            const double w = q.col_real(2);
            const TieEffect t = tie_effect(q.col(0), q.col(1));
            for (const auto& p : t.internal) {
                auto& n = nodes[p];
                ++n.internal;
                n.internal_w += w;
            }
            for (const auto& p : t.outgoing) {
                auto& n = nodes[p];
                ++n.outgoing;
                n.outgoing_w += w;
            }
            if (!t.a.empty()) {
                auto& x = ties[{t.parent, t.a, t.b}];
                ++x.first;
                x.second += w;
            }
        }
    }
    Stmt node(db_, "INSERT INTO cluster_nodes VALUES(?,?,?,?,?,?,?,?,?)");
    for (const auto& [p, n] : nodes) {
        node.integer(1, mid).text(2, p);
        if (p.empty()) node.null(3);
        else node.text(3, parent_of(p));
        const auto k = kinds.find(p);
        node.integer(4, k == kinds.end() ? 0 : k->second).integer(5, n.runes).integer(6, n.internal)
            .integer(7, n.outgoing).real(8, n.internal_w).real(9, n.outgoing_w).run();
    }
    Stmt tie(db_, "INSERT INTO cluster_ties VALUES(?,?,?,?,?,?)");
    for (const auto& [k, v] : ties)
        tie.integer(1, mid).text(2, std::get<0>(k)).text(3, std::get<1>(k)).text(4, std::get<2>(k))
            .integer(5, v.first).real(6, v.second).run();
}

bool Store::refresh_summaries(const std::string& mantle) {
    const long long mid = mantle_row(mantle);
    if (mid < 0 || !clustered(mantle)) return false;
    std::map<std::string, int> kinds; // packing survives a refresh
    {
        Stmt q(db_, "SELECT path, kind FROM cluster_nodes WHERE mantle=? AND kind<>0");
        q.integer(1, mid);
        while (q.row()) kinds.emplace(std::string(q.col(0)), (int)q.col_int(1));
    }
    if (!begin()) return false;
    summarize(mid, kinds);
    if (!commit()) {
        rollback();
        return false;
    }
    return true;
}

namespace {
const char* kKinds[] = {"community", "bundle", "slice"};

ClusterNode node_from(Stmt& q) {
    ClusterNode n;
    n.path = std::string(q.col(0));
    n.parent = std::string(q.col(1));
    const long long k = q.col_int(2);
    n.kind = k >= 0 && k <= 2 ? kKinds[k] : "community";
    n.runes = q.col_int(3);
    n.internal = q.col_int(4);
    n.outgoing = q.col_int(5);
    n.internal_weight = q.col_real(6);
    n.outgoing_weight = q.col_real(7);
    n.children = q.col_int(8);
    return n;
}

const char* kNodeColumns = "SELECT n.path, COALESCE(n.parent,''), n.kind, n.runes, n.internal, n.outgoing,"
                           " n.internal_weight, n.outgoing_weight,"
                           " (SELECT COUNT(*) FROM cluster_nodes k WHERE k.mantle=n.mantle AND k.parent=n.path)"
                           " FROM cluster_nodes n";
} // namespace

std::optional<ClusterNode> Store::cluster_node(const std::string& mantle, const std::string& path) {
    const long long mid = mantle_row(mantle);
    if (mid < 0) return std::nullopt;
    Stmt q(db_, std::string(kNodeColumns) + " WHERE n.mantle=? AND n.path=?");
    q.integer(1, mid).text(2, path);
    if (!q.row()) return std::nullopt;
    return node_from(q);
}

std::vector<ClusterNode> Store::cluster_children(const std::string& mantle, const std::string& path) {
    std::vector<ClusterNode> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, std::string(kNodeColumns) + " WHERE n.mantle=? AND n.parent=? ORDER BY n.path");
    q.integer(1, mid).text(2, path);
    while (q.row()) out.push_back(node_from(q));
    return out;
}

std::vector<ClusterTie> Store::cluster_ties(const std::string& mantle, const std::string& parent, long long min_links) {
    std::vector<ClusterTie> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT a, b, links, weight FROM cluster_ties WHERE mantle=? AND parent=? AND links>=?"
                " ORDER BY a, b");
    q.integer(1, mid).text(2, parent).integer(3, std::max(1LL, min_links));
    while (q.row()) out.push_back({std::string(q.col(0)), std::string(q.col(1)), q.col_int(2), q.col_real(3)});
    return out;
}

// ── overlays (Rancho, 2026-11-09) ─────────────────────────────────────────

void Store::put_overlay(const std::string& base, const std::string& mkey, const std::string& rid,
                        const std::string& diff) {
    if (diff.empty()) {
        Stmt q(db_, "DELETE FROM overlays WHERE base=? AND mkey=? AND rid=?");
        q.text(1, base).text(2, mkey).text(3, rid).run();
        return;
    }
    Stmt q(db_, "INSERT OR REPLACE INTO overlays VALUES(?,?,?,?)");
    q.text(1, base).text(2, mkey).text(3, rid).text(4, diff).run();
}

std::map<std::string, std::string> Store::overlays(const std::string& base, const std::string& mkey) {
    std::map<std::string, std::string> out;
    Stmt q(db_, "SELECT rid, diff FROM overlays WHERE base=? AND mkey=?");
    q.text(1, base).text(2, mkey);
    while (q.row()) out.emplace(std::string(q.col(0)), std::string(q.col(1)));
    return out;
}

std::optional<std::string> Store::rune_by_id(const std::string& mkey, const std::string& rid) {
    Stmt q(db_, "SELECT r.json FROM mantles m JOIN runes r INDEXED BY runes_by_rid ON r.mantle=m.id AND r.rid=?2"
                " WHERE m.deleted=0 AND (m.mid=?1 OR 'name:'||m.name=?1) AND r.deleted=0 LIMIT 1");
    q.text(1, mkey).text(2, rid);
    if (!q.row()) return std::nullopt;
    return std::string(q.col(0));
}

// ── finding runes by name (Rancho, 2026-11-09) ────────────────────────────

std::vector<std::string> Store::find_names(const std::string& mantle, const std::string& text, NameMatch how,
                                           long long limit, long long offset) {
    std::vector<std::string> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    if (how == NameMatch::Prefix) {
        /* One index range: every name that starts with `text` sorts in
         * [text, text + U+10FFFF), byte for byte. */
        Stmt q(db_, "SELECT name FROM runes INDEXED BY runes_by_name WHERE mantle=?1 AND name>=?2 AND name<?3"
                    " AND deleted=0 ORDER BY name LIMIT ?4 OFFSET ?5");
        q.integer(1, mid).text(2, text).text(3, text + "\xF4\x8F\xBF\xBF").integer(4, limit > 0 ? limit : -1)
            .integer(5, offset);
        while (q.row()) out.emplace_back(q.col(0));
        return out;
    }
    std::string pattern = "%";
    for (const char c : text) {
        if (c == '%' || c == '_' || c == '\\') pattern += '\\';
        pattern += c;
    }
    pattern += "%";
    Stmt q(db_, "SELECT name FROM runes WHERE mantle=?1 AND deleted=0 AND name LIKE ?2 ESCAPE '\\'"
                " ORDER BY pos LIMIT ?3 OFFSET ?4");
    q.integer(1, mid).text(2, pattern).integer(3, limit > 0 ? limit : -1).integer(4, offset);
    while (q.row()) out.emplace_back(q.col(0));
    return out;
}

Page Store::page(const std::string& mantle, long long offset, std::uint64_t byte_cap, long long max_runes) {
    Page out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT name, bytes FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos LIMIT -1 OFFSET ?");
    q.integer(1, mid).integer(2, offset);
    while (q.row()) {
        const auto b = (std::uint64_t)q.col_int(1);
        if (!out.names.empty() && out.bytes + b > byte_cap) break;
        out.names.emplace_back(q.col(0));
        out.bytes += b;
        if (max_runes > 0 && (long long)out.names.size() >= max_runes) break;
    }
    return out;
}

void Store::refresh_estimates() {
    exec("DELETE FROM est_mantles; DELETE FROM est_leaves;"
         "INSERT INTO est_mantles SELECT m.id,"
         " (SELECT COUNT(*) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
         " (SELECT COUNT(*) FROM edges e WHERE e.mantle=m.id AND e.deleted=0),"
         " (SELECT COALESCE(SUM(bytes),0) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
         " (SELECT COALESCE(SUM(bytes),0) FROM edges e WHERE e.mantle=m.id AND e.deleted=0)"
         " FROM mantles m WHERE m.deleted=0;"
         "INSERT INTO est_leaves SELECT c.mantle, c.path, COUNT(*), SUM(r.bytes) FROM clusters c"
         " JOIN runes r ON r.id=c.rune WHERE r.deleted=0 AND c.always=0 GROUP BY c.mantle, c.path;");
}

std::vector<MantleInfo> Store::estimates() {
    std::vector<MantleInfo> out;
    if (!db_) return out;
    std::vector<std::pair<long long, std::string>> unknown;
    {
        Stmt q(db_, "SELECT m.id, m.name, e.runes, e.edges, e.rune_bytes, e.edge_bytes FROM mantles m"
                    " LEFT JOIN est_mantles e ON e.mantle=m.id WHERE m.deleted=0 ORDER BY m.pos");
        while (q.row()) {
            if (q.is_null(2)) {
                unknown.emplace_back(q.col_int(0), std::string(q.col(1)));
                out.push_back({std::string(q.col(1))});
                continue;
            }
            out.push_back({std::string(q.col(1)), q.col_int(2), q.col_int(3), q.col_int(4) + q.col_int(5), q.col_int(4)});
        }
    }
    for (const auto& [id, name] : unknown) { // a mantle made since: small, as a rule; counted exactly
        Stmt q(db_, "SELECT (SELECT COUNT(*) FROM runes WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COUNT(*) FROM edges WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COALESCE(SUM(bytes),0) FROM runes WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COALESCE(SUM(bytes),0) FROM edges WHERE mantle=?1 AND deleted=0)");
        q.integer(1, id);
        if (!q.row()) continue;
        for (auto& m : out)
            if (m.name == name) m = {name, q.col_int(0), q.col_int(1), q.col_int(2) + q.col_int(3), q.col_int(2)};
    }
    return out;
}

std::vector<ClusterLeaf> Store::leaf_estimates(const std::string& mantle) {
    std::vector<ClusterLeaf> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT path, runes, bytes FROM est_leaves WHERE mantle=? ORDER BY path");
    q.integer(1, mid);
    while (q.row()) out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2)});
    if (out.empty() && clustered(mantle)) return leaves(mantle); // clustered before the estimates existed
    return out;
}

bool Store::vacuum_into(const std::filesystem::path& out) {
    if (!db_) return false;
    Stmt q(db_, "VACUUM INTO ?");
    q.text(1, out.string());
    if (q.run()) return true;
    error_ = sqlite3_errmsg(db_);
    return false;
}

bool Store::pack_into(const std::filesystem::path& out) {
    if (!vacuum_into(out)) return false;
    /* The copy without what every open rebuilds (kIndexes): a third of a
     * store's size, for a file that is carried rather than worked in. */
    sqlite3* copy = nullptr;
    bool ok = sqlite3_open(out.string().c_str(), &copy) == SQLITE_OK;
    char* msg = nullptr;
    if (ok) {
        std::string sql;
        for (std::string_view rest = kIndexes; !rest.empty();) {
            const auto at = rest.find("EXISTS ");
            if (at == std::string_view::npos) break;
            const auto name = rest.substr(at + 7, rest.find(' ', at + 7) - at - 7);
            sql += "DROP INDEX IF EXISTS " + std::string(name) + ";";
            rest = rest.substr(at + 7);
        }
        sql += "VACUUM;";
        ok = sqlite3_exec(copy, sql.c_str(), nullptr, nullptr, &msg) == SQLITE_OK;
    }
    if (!ok) error_ = msg ? msg : (copy ? sqlite3_errmsg(copy) : "cannot open the copy");
    sqlite3_free(msg);
    sqlite3_close(copy);
    return ok;
}

std::string Store::meta(const std::string& key) {
    if (!db_) return {};
    Stmt q(db_, "SELECT value FROM store_meta WHERE key=?");
    q.text(1, key);
    return q.row() ? std::string(q.col(0)) : std::string();
}

void Store::set_meta(const std::string& key, const std::string& value) {
    if (!db_) return;
    Stmt q(db_, "INSERT OR REPLACE INTO store_meta VALUES(?,?)");
    q.text(1, key).text(2, value).run();
}

bool Store::has_document() {
    if (!db_) return false;
    Stmt q(db_, "SELECT 1 FROM top LIMIT 1");
    return q.row();
}

} // namespace rancho
