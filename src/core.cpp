/* core.cpp — see rancho/core.hpp. */
#include "rancho/core.hpp"

#include "voidcore.h"

#include "json.hpp"

#include <set>
#include <stdexcept>

namespace rancho {

namespace {

/* Everything vc_* hands out goes back through vc_free_str. */
std::string owned(char* p) {
    std::string s = p ? p : "";
    vc_free_str(p);
    return s;
}

/* Void Core's query family writes nothing (its dispatch/verbs_query.c), nor do
 * these reads of the graph and the history; anything else may have changed the
 * state. A spare revision costs one comparison; a missing one is a wrong store,
 * so the list holds only verbs read in Core's own source (the list Void Maiz's
 * embed.cpp keeps, plus `links` and `values`, which Rancho itself sends;
 * `related` and `bindings` read-only in verbs_graph.c, as Maiz's list has them).
 * Both lists retire when Core says which verbs mutate. */
bool is_read(const std::vector<std::string>& argv) {
    static const std::set<std::string> reads = {"ls",   "cat",      "get",  "find", "glyphs", "mantles",
                                                "tree", "where",    "describe", "axes", "help", "version",
                                                "history", "links", "values", "related", "bindings"};
    if (argv.empty()) return false;
    if (reads.count(argv[0])) return true;
    return argv[0] == "config" && (argv.size() == 1 || argv[1] == "get");
}

Reply parse_envelope(const std::string& raw) {
    Reply r;
    const auto j = nlohmann::json::parse(raw, nullptr, false);
    if (!j.is_object()) {
        r.text = raw;
        return r;
    }
    r.ok = j.value("ok", false);
    if (const auto d = j.find("data"); d != j.end()) r.data = d->dump();
    if (const auto l = j.find("lines"); l != j.end() && l->is_array())
        for (const auto& line : *l) {
            if (!r.text.empty()) r.text += '\n';
            r.text += line.is_string() ? line.get<std::string>() : line.dump();
        }
    return r;
}

} // namespace

std::string arg(std::string_view value) {
    const std::string in(value);
    char* q = vc_arg_quote(in.c_str());
    if (!q) throw std::runtime_error("rancho: vc_arg_quote failed");
    return owned(q);
}

std::string batch(const std::vector<std::string>& commands) {
    return "batch " + arg(nlohmann::json(commands).dump());
}

Argv split_argv(std::string_view line) {
    Argv out;
    const std::string in(line);
    const auto j = nlohmann::json::parse(owned(vc_argv_split_json(in.c_str())), nullptr, false);
    if (!j.is_object() || !j.value("ok", false)) return out;
    if (const auto a = j.find("argv"); a != j.end() && a->is_array())
        for (const auto& s : *a)
            if (s.is_string()) out.argv.push_back(s.get<std::string>());
    out.ok = true;
    return out;
}

bool tag_match(std::string_view expr, const std::vector<std::string>& tags) {
    const std::string e(expr), t = nlohmann::json(tags).dump();
    return vc_tag_match(e.c_str(), t.c_str()) == 1;
}

std::string_view core_version() { return vc_version(); }

VcCore::VcCore() : m_(vc_create(nullptr)) {
    if (!m_) throw std::runtime_error("rancho: vc_create failed");
}

VcCore::VcCore(std::string_view state_json) {
    const std::string s(state_json);
    m_ = vc_create(s.c_str());
    if (!m_) throw std::runtime_error("rancho: vc_create failed");
}

VcCore::~VcCore() {
    if (m_) vc_destroy(m_);
}

Reply VcCore::dispatch(std::string_view command) {
    const std::string c(command);
    const Argv a = split_argv(c);
    if (!a.ok || !is_read(a.argv)) ++rev_;
    return parse_envelope(owned(vc_dispatch(m_, c.c_str())));
}

std::string VcCore::export_state() { return owned(vc_export_state(m_)); }

bool VcCore::replace_state(std::string_view state_json) {
    const std::string s(state_json);
    VC_Manager* fresh = vc_create(s.empty() ? nullptr : s.c_str());
    if (!fresh) return false;
    for (const auto& g : glyphs_) vc_register_glyph(fresh, g.c_str());
    if (undo_ >= 0) vc_set_undo(fresh, undo_);
    if (undo_depth_ > 0) vc_set_undo_depth(fresh, undo_depth_);
    if (journal_ >= 0) vc_set_journal(fresh, journal_);
    vc_destroy(m_);
    m_ = fresh;
    ++rev_;
    return true;
}

bool VcCore::register_glyph(std::string_view glyph_json) {
    const std::string g(glyph_json);
    if (vc_register_glyph(m_, g.c_str()) != 1) return false;
    glyphs_.push_back(g);
    ++rev_;
    return true;
}

void VcCore::set_undo(bool on) {
    undo_ = on ? 1 : 0;
    vc_set_undo(m_, undo_);
}

void VcCore::set_undo_depth(int depth) {
    undo_depth_ = depth;
    vc_set_undo_depth(m_, depth);
}

void VcCore::set_journal(bool on) {
    journal_ = on ? 1 : 0;
    vc_set_journal(m_, journal_);
}

std::string VcCore::export_journal() {
    const std::string j = owned(vc_export_journal(m_));
    return j.empty() ? "[]" : j;
}

} // namespace rancho
