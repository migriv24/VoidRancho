/* overlay.cpp — see rancho/overlay.hpp. */
#include "rancho/overlay.hpp"

#include "json.hpp"

#include <algorithm>
#include <set>
#include <vector>

namespace rancho::overlay {

using nlohmann::json;

namespace {

std::vector<std::string> tags_of(const json& rune) {
    std::vector<std::string> out;
    if (const auto t = rune.find("tags"); t != rune.end() && t->is_array())
        for (const auto& x : *t)
            if (x.is_string()) out.push_back(x.get<std::string>());
    return out;
}

json parse_object(const std::string& text) {
    json j = json::parse(text, nullptr, false);
    return j.is_object() ? j : json::object();
}

} // namespace

std::string diff(const std::string& base_text, const std::string& mine_text, const std::string& source,
                 const std::map<std::string, std::string>& sources) {
    const json base = parse_object(base_text), mine = parse_object(mine_text);
    json d = json::object();
    // tags: added (with who added them) and removed, each recorded as itself
    const auto bt = tags_of(base), mt = tags_of(mine);
    const std::set<std::string> bs(bt.begin(), bt.end()), ms(mt.begin(), mt.end());
    json added = json::object(), removed = json::array();
    for (const auto& t : mt)
        if (!bs.count(t)) {
            const auto s = sources.find(t);
            added[t] = s != sources.end() ? s->second : source;
        }
    for (const auto& t : bt)
        if (!ms.count(t)) removed.push_back(t);
    if (!added.empty()) d["tags_added"] = added;
    if (!removed.empty()) d["tags_removed"] = removed;
    // members: an object member key by key (content, facets), any other whole (glyph, placement, ...)
    json set = json::object(), unset = json::array();
    for (auto it = mine.begin(); it != mine.end(); ++it) {
        const std::string& k = it.key();
        if (k == "tags" || k == "spirit") continue;
        const auto b = base.find(k);
        if (it->is_object() && b != base.end() && b->is_object()) {
            for (auto f = it->begin(); f != it->end(); ++f) {
                const auto bf = b->find(f.key());
                if (bf == b->end() || *bf != *f) set[k + "/" + f.key()] = *f;
            }
            for (auto f = b->begin(); f != b->end(); ++f)
                if (!it->contains(f.key())) unset.push_back(k + "/" + f.key());
        } else if (b == base.end() || *b != *it) {
            set[k] = *it;
        }
    }
    // the name is the person's too; the id never differs (it is the key)
    if (mine.contains("spirit") && base.contains("spirit") &&
        mine["spirit"].value("name", "") != base["spirit"].value("name", ""))
        set["spirit/name"] = mine["spirit"].value("name", "");
    if (!set.empty()) d["set"] = set;
    if (!unset.empty()) d["unset"] = unset;
    return d.empty() ? std::string() : d.dump();
}

std::string made(const std::string& rune, const std::string& source) {
    json d = json::object();
    d["made"] = parse_object(rune);
    d["source"] = source;
    return d.dump();
}

bool is_made(const std::string& diff) { return parse_object(diff).contains("made"); }

std::string apply(const std::string& base_text, const std::string& diff_text) {
    const json d = parse_object(diff_text);
    if (d.contains("made")) return d["made"].dump();
    json r = parse_object(base_text);
    // tags: the base's, without the removed, then the added in their order
    std::set<std::string> removed;
    for (const auto& t : d.value("tags_removed", json::array()))
        if (t.is_string()) removed.insert(t.get<std::string>());
    json tags = json::array();
    std::set<std::string> have;
    for (const auto& t : tags_of(r))
        if (!removed.count(t) && have.insert(t).second) tags.push_back(t);
    const json added = d.value("tags_added", json::object()); // held: two temporaries' iterators never meet
    for (auto it = added.begin(); it != added.end(); ++it)
        if (have.insert(it.key()).second) tags.push_back(it.key());
    if (r.contains("tags") || !tags.empty()) r["tags"] = tags;
    const json set = d.value("set", json::object());
    for (auto it = set.begin(); it != set.end(); ++it) {
        const std::string& k = it.key();
        const auto slash = k.find('/');
        if (slash == std::string::npos) {
            r[k] = *it;
            continue;
        }
        json& member = r[k.substr(0, slash)];
        if (!member.is_object()) member = json::object();
        member[k.substr(slash + 1)] = *it;
    }
    for (const auto& u : d.value("unset", json::array())) {
        if (!u.is_string()) continue;
        const std::string k = u.get<std::string>();
        const auto slash = k.find('/');
        if (slash == std::string::npos) continue;
        if (auto m = r.find(k.substr(0, slash)); m != r.end() && m->is_object()) m->erase(k.substr(slash + 1));
    }
    return r.dump();
}

std::map<std::string, std::string> added_tags(const std::string& diff_text) {
    std::map<std::string, std::string> out;
    const json d = parse_object(diff_text);
    const json a = d.value("tags_added", json::object());
    for (auto it = a.begin(); it != a.end(); ++it)
        if (it->is_string()) out.emplace(it.key(), it->get<std::string>());
    return out;
}

} // namespace rancho::overlay
