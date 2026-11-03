/* working_set.cpp — see rancho/working_set.hpp. Ported from Void Hormiga's
 * src/store/working_set.cpp (2026-10-27): the logic is unchanged; the Core is a
 * CoreLink, and a sync writes tags under the working set's actor. */
#include "rancho/working_set.hpp"


#include "json.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace rancho {

namespace {

std::uint64_t hash_of(std::string_view s) { return (std::uint64_t)std::hash<std::string_view>{}(s); }

std::string str_of(const nlohmann::json& j, const char* k) {
    const auto it = j.find(k);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::string edge_key(const std::string& src, const std::string& dst, const std::string& rel) {
    return src + '\x1f' + dst + '\x1f' + rel;
}

struct NowRune {
    std::string name, id, raw;
    std::uint64_t hash = 0;
};
struct NowEdge {
    std::string src, dst, rel, raw;
    std::uint64_t hash = 0;
};
struct NowMantle {
    MantleRow row;
    std::uint64_t shape = 0;
    std::vector<NowRune> runes;
    std::vector<NowEdge> edges;
};

/* What Core holds now: its exported document, walked one value at a time. */
bool read_core(const std::string& text, std::vector<NowMantle>& mantles,
               std::vector<std::pair<std::string, std::string>>& top, std::string& error) {
    JsonStream in = JsonStream::from_text(text);
    std::string raw;
    auto read_mantle = [&]() -> bool {
        NowMantle m;
        std::string skeleton = "{", layout;
        bool has_layout = false;
        m.row.has_runes = m.row.has_edges = false;
        const bool ok = in.object([&](const std::string& key) -> bool {
            if (key == "runes") {
                m.row.has_runes = true;
                return in.array([&] {
                    NowRune r;
                    if (!in.capture(r.raw)) return false;
                    const auto j = nlohmann::json::parse(r.raw, nullptr, false);
                    if (!j.is_object()) return in.fail("a rune that is not an object");
                    const auto sp = j.value("spirit", nlohmann::json::object());
                    r.name = str_of(sp, "name");
                    r.id = str_of(sp, "id");
                    r.hash = hash_of(r.raw);
                    m.runes.push_back(std::move(r));
                    return true;
                });
            }
            if (key == "layout" && in.peek() == '{') {
                has_layout = true;
                layout = "{";
                return in.object([&](const std::string& lk) -> bool {
                    if (lk == "edges") {
                        m.row.has_edges = true;
                        return in.array([&] {
                            NowEdge e;
                            if (!in.capture(e.raw)) return false;
                            const auto j = nlohmann::json::parse(e.raw, nullptr, false);
                            if (!j.is_object()) return in.fail("an edge that is not an object");
                            e.src = str_of(j, "from");
                            e.dst = str_of(j, "to");
                            e.rel = str_of(j, "relation");
                            e.hash = hash_of(e.raw);
                            m.edges.push_back(std::move(e));
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
                if (j.is_string()) (key == "name" ? m.row.name : m.row.mid) = j.get<std::string>();
            }
            if (skeleton.size() > 1) skeleton += ",";
            skeleton += nlohmann::json(key).dump() + ":" + raw;
            return true;
        });
        if (!ok) return false;
        m.row.skeleton = skeleton + "}";
        if (has_layout) m.row.layout = layout;
        m.shape = hash_of(m.row.skeleton) ^ (hash_of(has_layout ? layout : std::string("-")) * 3) ^
                  (m.row.has_runes ? 5u : 0u) ^ (m.row.has_edges ? 7u : 0u);
        mantles.push_back(std::move(m));
        return true;
    };
    const bool ok = in.object([&](const std::string& key) -> bool {
        if (key == "mantles") return in.array([&] { return read_mantle(); });
        if (key == "_baseline") return in.skip();
        if (!in.capture(raw)) return false;
        top.emplace_back(key, raw);
        return true;
    });
    if (!ok) error = in.error();
    return ok;
}

} // namespace

bool WorkingSet::load(const Selection& sel) {
    error_.clear();
    store_.purge(); // no undo can reach the last session's removals any more
    const std::string text = store_.state(sel);
    if (!core_.replace_state(text)) {
        error_ = "Void Core could not take the working set";
        return false;
    }
    return adopt(sel);
}

bool WorkingSet::adopt(const Selection& sel) {
    error_.clear();
    sel_ = sel;
    snap_ = {};
    synced_rev_ = 0;
    // what Core holds now is what the store holds: the first snapshot, nothing written
    std::vector<NowMantle> now;
    std::vector<std::pair<std::string, std::string>> top;
    if (!read_core(core_.export_state(), now, top, error_)) return false;
    for (const auto& [k, v] : top) {
        snap_.top[k] = hash_of(v);
        if (k == "active") {
            const auto j = nlohmann::json::parse(v, nullptr, false);
            active_ = j.is_object() ? str_of(j, "mantle") : std::string();
        }
    }
    for (auto& m : now) {
        MantleSnap s;
        s.name = m.row.name;
        s.mid = m.row.mid;
        s.row = store_.mantle_row(m.row.name);
        s.shape = m.shape;
        for (const auto& r : m.runes) s.runes[r.name] = {r.id, r.hash};
        for (const auto& e : m.edges) s.edges[edge_key(e.src, e.dst, e.rel)].push_back(e.hash);
        for (auto& [k, v] : s.edges) std::sort(v.begin(), v.end());
        snap_.mantles.push_back(std::move(s));
    }
    synced_rev_ = expected_rev_ = core_.revision();
    touched_ = {};
    full_ = false;
    return true;
}

Reply WorkingSet::dispatch(const std::string& command) {
    if (core_.revision() != expected_rev_) full_ = true; // Core was changed some other way since
    Reply r = core_.dispatch(command);
    if (r.ok) { // a command that failed changed nothing (Core rolls a failed batch back)
        const Argv a = split_argv(command);
        if (!a.ok || !note(a.argv)) full_ = true;
    }
    expected_rev_ = core_.revision();
    return r;
}

/* What a command names, for the fast path. False sends the next sync to the
 * full comparison: anything not listed here may change more than it names. */
bool WorkingSet::note(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    const std::string& v = argv[0];
    if (v == "use" && argv.size() == 2) {
        active_ = argv[1];
        return true;
    }
    if (v == "config" && argv.size() >= 3 && argv[1] == "set") { // a session setting: the config object
        touched_.config = true;
        return true;
    }
    if ((v == "set" || v == "setjson" || v == "facet" || v == "tag" || v == "measure") && argv.size() >= 2) {
        touched_.runes[active_].push_back(argv[1]);
        return true;
    }
    if (v == "rune" && argv.size() >= 4 && argv[1] == "new") {
        touched_.runes[active_].push_back(argv[3]);
        return true;
    }
    if ((v == "link" || v == "unlink") && argv.size() >= 3 && argv[1].rfind("--", 0) != 0) {
        touched_.pairs[active_].emplace_back(argv[1], argv[2]);
        return true;
    }
    if (v == "batch" && argv.size() == 2) {
        const auto list = nlohmann::json::parse(argv[1], nullptr, false);
        if (!list.is_array()) return false;
        for (const auto& c : list) {
            if (!c.is_string()) return false;
            const Argv a = split_argv(c.get<std::string>());
            if (!a.ok || !note(a.argv)) return false;
        }
        return true;
    }
    return false;
}

/* The runes and edges the commands named, asked of Core one by one (`cat`,
 * `links`: reads, which move nothing), and written. Anything unexpected — a
 * rune Core no longer has under that name, a mantle that is not the active
 * one — and the full comparison does it instead. */
bool WorkingSet::sync_targeted() {
    error_.clear();
    for (const auto& [m, _] : touched_.runes)
        if (m != active_) return sync_full();
    for (const auto& [m, _] : touched_.pairs)
        if (m != active_) return sync_full();
    MantleSnap* snap = nullptr;
    for (auto& ms : snap_.mantles)
        if (ms.name == active_) snap = &ms;
    if (!snap || snap->row < 0) return sync_full();

    struct Got {
        std::string name, id, json;
        std::uint64_t hash;
    };
    std::vector<Got> runes;
    std::set<std::string> seen;
    for (const auto& name : touched_.runes[active_]) {
        if (!seen.insert(name).second) continue;
        const Reply c = core_.dispatch("cat " + arg(name));
        if (!c.ok) return sync_full(); // removed, renamed or not a name: let the comparison see what happened
        const auto j = nlohmann::json::parse(c.data, nullptr, false);
        if (!j.is_object()) return sync_full();
        const auto sp = j.value("spirit", nlohmann::json::object());
        if (str_of(sp, "name") != name) return sync_full();
        runes.push_back({name, str_of(sp, "id"), c.data, hash_of(c.data)});
    }
    struct Pair {
        std::string key, src, dst, rel;
        std::vector<std::string> jsons;
    };
    std::map<std::string, Pair> keys; // every key between each touched pair, as Core has it now
    std::set<std::pair<std::string, std::string>> pairs;
    for (const auto& [a, b] : touched_.pairs[active_]) {
        if (!pairs.insert({std::min(a, b), std::max(a, b)}).second) continue;
        const Reply l = core_.dispatch("links " + arg(a));
        if (!l.ok) return sync_full();
        const auto list = nlohmann::json::parse(l.data, nullptr, false);
        if (!list.is_array()) return sync_full();
        for (const auto& e : list) {
            const std::string f = str_of(e, "from"), t = str_of(e, "to");
            if (!((f == a && t == b) || (f == b && t == a))) continue;
            const std::string k = edge_key(f, t, str_of(e, "relation"));
            auto& p = keys[k];
            p.key = k;
            p.src = f;
            p.dst = t;
            p.rel = str_of(e, "relation");
            p.jsons.push_back(e.dump());
        }
        // keys the snapshot had between them that Core no longer has
        for (const auto& [k, _] : snap->edges) {
            const auto x = k.find('\x1f'), y = k.find('\x1f', x + 1);
            const std::string f = k.substr(0, x), t = k.substr(x + 1, y - x - 1);
            if (((f == a && t == b) || (f == b && t == a)) && !keys.count(k))
                keys[k] = Pair{k, f, t, k.substr(y + 1), {}};
        }
    }

    std::string config;
    if (touched_.config) {
        const Reply c = core_.dispatch("config"); // bare: every key, as an object (a read)
        if (!c.ok) return sync_full();
        config = c.data;
    }

    if (!store_.begin()) {
        error_ = store_.error();
        return false;
    }
    ++stats.syncs;
    ++stats.targeted;
    if (touched_.config) {
        store_.put_top("config", config);
        snap_.top["config"] = hash_of(config);
    }
    for (const auto& g : runes) {
        const auto was = snap->runes.find(g.name);
        if (was != snap->runes.end() && was->second.hash == g.hash) continue;
        if (was == snap->runes.end()) { // new to Core
            if (store_.restore_rune(snap->row, g.id)) {
                ++stats.restores;
            } else if (const auto clash = store_.live_rune_id(snap->row, g.name); clash && *clash != g.id) {
                ++stats.refused;
                error_ = "\"" + g.name + "\" is the name of a rune that is not loaded; the new rune was not stored";
                continue;
            }
        }
        store_.put_rune(snap->row, g.name, g.json);
        ++stats.runes_written;
        snap->runes[g.name] = {g.id, g.hash};
    }
    for (const auto& [k, p] : keys) {
        store_.set_edges(snap->row, p.src, p.dst, p.rel, p.jsons);
        ++stats.edge_keys;
        if (p.jsons.empty()) {
            snap->edges.erase(k);
        } else {
            std::vector<std::uint64_t> hs;
            for (const auto& js : p.jsons) hs.push_back(hash_of(js));
            std::sort(hs.begin(), hs.end());
            snap->edges[k] = std::move(hs);
        }
    }
    if (!store_.commit()) {
        error_ = store_.error();
        store_.rollback();
        return false;
    }
    synced_rev_ = core_.revision();
    return error_.empty();
}

bool WorkingSet::name_taken(const std::string& mantle, const std::string& name) {
    const long long row = store_.mantle_row(mantle);
    return row >= 0 && store_.live_rune_id(row, name).has_value();
}

bool WorkingSet::sync() {
    if (core_.revision() == synced_rev_) { // nothing was dispatched since
        touched_ = {};
        full_ = false;
        return true;
    }
    store_.set_source(actor_); // the tags this writes for the first time are the actor's
    const bool targeted = !full_ && core_.revision() == expected_rev_;
    const bool ok = targeted ? sync_targeted() : sync_full();
    touched_ = {};
    full_ = false;
    expected_rev_ = synced_rev_;
    return ok;
}

bool WorkingSet::sync_full() {
    error_.clear();
    ++stats.full;
    std::vector<NowMantle> now;
    std::vector<std::pair<std::string, std::string>> top;
    const std::string text = core_.export_state();
    if (text.empty()) { // Core could not export: out of memory, most likely
        error_ = "Void Core gave no document to write back (out of memory?); nothing was written";
        return false;
    }
    if (!read_core(text, now, top, error_)) return false;
    if (!store_.begin()) {
        error_ = store_.error();
        return false;
    }
    ++stats.syncs;

    // ── the state's other keys ──
    std::set<std::string> keys;
    bool keys_changed = top.size() != snap_.top.size();
    Snapshot next;
    for (const auto& [k, v] : top) {
        keys.insert(k);
        const std::uint64_t h = hash_of(v);
        const auto it = snap_.top.find(k);
        if (it == snap_.top.end()) keys_changed = true;
        if (it == snap_.top.end() || it->second != h) store_.put_top(k, v);
        next.top[k] = h;
    }
    if (keys_changed) store_.keep_only_top(keys);

    // ── mantles ──
    std::vector<bool> seen(snap_.mantles.size(), false);
    auto find_prev = [&](const NowMantle& m) -> int {
        for (std::size_t i = 0; i < snap_.mantles.size(); ++i) {
            const auto& p = snap_.mantles[i];
            if (seen[i]) continue;
            if (!m.row.mid.empty() ? p.mid == m.row.mid : p.name == m.row.name) return (int)i;
        }
        return -1;
    };
    for (std::size_t mi = 0; mi < now.size(); ++mi) {
        NowMantle& m = now[mi];
        const int pi = find_prev(m);
        static const MantleSnap kNone;
        const MantleSnap& prev = pi >= 0 ? snap_.mantles[(std::size_t)pi] : kNone;
        if (pi >= 0) seen[(std::size_t)pi] = true;

        MantleSnap s;
        s.name = m.row.name;
        s.mid = m.row.mid;
        s.shape = m.shape;
        s.row = (pi >= 0 && prev.shape == m.shape && prev.name == m.row.name && prev.row >= 0)
                    ? prev.row
                    : store_.put_mantle(m.row, (int)mi);
        const long long row = s.row;

        // runes: follow each by its id
        std::unordered_map<std::string, const NowRune*> now_by_name;
        for (const auto& r : m.runes) now_by_name[r.name] = &r;
        std::unordered_map<std::string, std::string> prev_by_id; // id -> name
        for (const auto& [name, r] : prev.runes)
            if (!r.id.empty()) prev_by_id[r.id] = name;
        std::set<std::string> touched; // names whose edges must be looked at again
        std::set<std::string> handled_prev;

        for (const auto& r : m.runes) {
            const auto was = prev.runes.find(r.name);
            if (was != prev.runes.end()) { // still there under the same name
                handled_prev.insert(r.name);
                if (was->second.hash != r.hash) {
                    store_.put_rune(row, r.name, r.raw);
                    ++stats.runes_written;
                }
                s.runes[r.name] = {r.id, r.hash};
                continue;
            }
            // a rename: the same id under a name it did not have, and the old name gone
            const auto old = r.id.empty() ? prev_by_id.end() : prev_by_id.find(r.id);
            if (old != prev_by_id.end() && !now_by_name.count(old->second)) {
                const std::string& from = old->second;
                if (const auto clash = store_.live_rune_id(row, r.name); clash && *clash != r.id) {
                    ++stats.refused; // the new name belongs to a rune Core cannot see
                    error_ = "\"" + r.name + "\" is the name of a rune that is not loaded; not renamed in the store";
                    s.runes[from] = prev.runes.at(from);
                    handled_prev.insert(from);
                    continue;
                }
                store_.rename_rune(row, from, r.name);
                store_.put_rune(row, r.name, r.raw);
                ++stats.renames;
                handled_prev.insert(from);
                touched.insert(from);
                touched.insert(r.name);
                s.runes[r.name] = {r.id, r.hash};
                continue;
            }
            // new to Core: a rune whose removal was undone, or a new one
            if (store_.restore_rune(row, r.id)) {
                ++stats.restores;
            } else if (const auto clash = store_.live_rune_id(row, r.name); clash && *clash != r.id) {
                ++stats.refused;
                error_ = "\"" + r.name + "\" is the name of a rune that is not loaded; the new rune was not stored";
                continue;
            }
            store_.put_rune(row, r.name, r.raw);
            ++stats.runes_written;
            touched.insert(r.name);
            s.runes[r.name] = {r.id, r.hash};
        }
        for (const auto& [name, r] : prev.runes) {
            if (handled_prev.count(name) || now_by_name.count(name)) continue;
            store_.delete_rune(row, name); // and every edge touching it, loaded or not
            ++stats.runes_removed;
        }

        // edges: by key, where the multiset changed or an end was renamed or restored
        std::unordered_map<std::string, std::vector<const NowEdge*>> now_edges;
        for (const auto& e : m.edges) now_edges[edge_key(e.src, e.dst, e.rel)].push_back(&e);
        std::set<std::string> keys_to_fix;
        for (const auto& [k, list] : now_edges) {
            std::vector<std::uint64_t> hs;
            for (const auto* e : list) hs.push_back(e->hash);
            std::sort(hs.begin(), hs.end());
            const auto was = prev.edges.find(k);
            if (was == prev.edges.end() || was->second != hs) keys_to_fix.insert(k);
            const auto& e0 = *list.front();
            if (touched.count(e0.src) || touched.count(e0.dst)) keys_to_fix.insert(k);
            s.edges[k] = std::move(hs);
        }
        for (const auto& [k, hs] : prev.edges)
            if (!now_edges.count(k)) keys_to_fix.insert(k);
        for (const auto& k : keys_to_fix) {
            const auto a = k.find('\x1f'), b = k.find('\x1f', a + 1);
            const std::string src = k.substr(0, a), dst = k.substr(a + 1, b - a - 1), rel = k.substr(b + 1);
            // an edge whose end was just removed went with it; and one whose end was renamed is
            // stored under the new name now, so the old key has nothing live to change
            std::vector<std::string> raws;
            if (const auto it = now_edges.find(k); it != now_edges.end())
                for (const auto* e : it->second) raws.push_back(e->raw);
            store_.set_edges(row, src, dst, rel, raws);
            ++stats.edge_keys;
        }
        next.mantles.push_back(std::move(s));
    }
    for (std::size_t i = 0; i < snap_.mantles.size(); ++i)
        if (!seen[i] && snap_.mantles[i].row >= 0) store_.delete_mantle(snap_.mantles[i].row);

    if (!store_.commit()) {
        error_ = store_.error();
        store_.rollback();
        return false;
    }
    snap_ = std::move(next);
    synced_rev_ = core_.revision();
    return error_.empty();
}

} // namespace rancho
