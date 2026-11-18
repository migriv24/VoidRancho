/* rodeo_test.cpp — el rodeo and el embarque (okf/concepts/rodeo.md).
 *
 * The promises: a rodeo reads every rune and every edge of a store exactly
 * once, whatever is clustered and whatever is not; a rodeo stopped anywhere
 * resumes from its cursor to the same end; an embarque is named by its
 * manifest, stores identical partidas once, resumes without rewriting, and
 * refuses a damaged file. */
#include "check.hpp"

#include "rancho/rodeo.hpp"
#include "rancho/store.hpp"

#include <map>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace rancho;

namespace {

fs::path scratch(const std::string& name) { return test::scratch("rodeo-test", name); }

const char* kThing = R"({"glyph":"thing","label":"Thing","fields":["note"]})";

/* A store with every kind of herd: a measure (always-loaded), clustered
 * leaves, runes made after the clustering (no leaf), an edge from a name that
 * is no rune, and a second mantle never clustered. */
void fill(Store& st) {
    VcCore c;
    c.register_glyph(kThing);
    c.dispatch("mantle new org");
    CHECK(c.dispatch("glyph declare " + arg(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})")).ok);
    std::vector<std::string> cmds;
    for (int i = 0; i < 300; ++i) {
        cmds.push_back("rune new thing r" + std::to_string(i));
        cmds.push_back("set r" + std::to_string(i) + " note " + arg(std::string(100, 'n')));
    }
    for (int i = 0; i < 300; ++i) {
        cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string((i * 7 + 1) % 300));
        if (i % 10 == 0) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(i / 10) + " --relation knows");
    }
    cmds.push_back("rune new metric score");
    for (int i = 0; i < 300; i += 3) cmds.push_back("link r" + std::to_string(i) + " score --weight 2");
    CHECK(c.dispatch(batch(cmds)).ok);
    c.dispatch("mantle new notes");
    for (int i = 0; i < 40; ++i) c.dispatch("rune new thing n" + std::to_string(i));
    c.dispatch("use org");
    json doc = json::parse(c.export_state());
    doc["mantles"][0]["layout"]["edges"].push_back({{"from", "ghost"}, {"to", "r1"}}); // SPEC §3.7 allows it
    JsonStream in = JsonStream::from_text(doc.dump());
    CHECK(st.import(in).ok);
    CHECK(st.cluster("org", 40).ok);
    CHECK(st.begin());
    const long long org = st.mantle_row("org");
    for (int i = 0; i < 25; ++i) {
        const std::string n = "late" + std::to_string(i);
        st.put_rune(org, n, json({{"glyph", "thing"}, {"spirit", {{"name", n}, {"id", "late-" + std::to_string(i)}}}}).dump());
    }
    st.set_edges(org, "late0", "r5", "", {R"({"from":"late0","to":"r5"})"});
    CHECK(st.commit());
}

struct Seen {
    std::multiset<std::string> runes;          // mantle/name
    std::multiset<std::string> edges;          // mantle/from>to:relation
    std::set<std::string> paths;
};

void take(const Herd& h, Seen& s) {
    for (const auto& r : h.runes) s.runes.insert(h.mantle + "/" + json::parse(r)["spirit"]["name"].get<std::string>());
    for (const auto& e : h.edges) {
        const json j = json::parse(e);
        s.edges.insert(h.mantle + "/" + j["from"].get<std::string>() + ">" + j["to"].get<std::string>() + ":" +
                       j.value("relation", ""));
    }
    s.paths.insert(h.path);
}

Seen expected(Store& st) {
    Seen s;
    const json doc = json::parse(st.state());
    for (const auto& m : doc["mantles"]) {
        const std::string name = m["name"];
        for (const auto& r : m.value("runes", json::array())) s.runes.insert(name + "/" + r["spirit"]["name"].get<std::string>());
        if (m.contains("layout"))
            for (const auto& e : m["layout"].value("edges", json::array()))
                s.edges.insert(name + "/" + e["from"].get<std::string>() + ">" + e["to"].get<std::string>() + ":" +
                               e.value("relation", ""));
    }
    return s;
}

void a_rodeo_reads_every_rune_and_every_edge_once() {
    Store st(scratch("once.store"));
    fill(st);
    RodeoOptions o;
    o.page_bytes = 2000; // several pages of the runes in no leaf
    Rodeo r(st, {}, o);
    Seen got;
    Herd h;
    std::size_t herds = 0, largest = 0;
    while (r.next(h)) {
        take(h, got);
        ++herds;
        largest = std::max(largest, h.runes.size());
    }
    CHECK(r.done() && r.error().empty());
    const Seen want = expected(st);
    CHECK(got.runes == want.runes);
    CHECK(got.edges == want.edges);
    CHECK(got.paths.count("!") && got.paths.count("page") && got.paths.count("edges"));
    CHECK(largest <= 41); // a herd is a leaf or a page, never the mantle
    std::printf("    %zu herds, %zu runes, %zu edges\n", herds, got.runes.size(), got.edges.size());
}

void a_stopped_rodeo_resumes_where_it_stood() {
    Store st(scratch("resume.store"));
    fill(st);
    RodeoOptions o;
    o.page_bytes = 2000;
    const Seen want = expected(st);
    for (int stop : {1, 2, 5, 9, 14}) {
        Seen got;
        Herd h;
        std::string cursor;
        {
            Rodeo r(st, {}, o);
            for (int i = 0; i < stop && r.next(h); ++i) take(h, got);
            cursor = r.cursor();
        }
        Rodeo again(st, cursor, o); // another process, later: only the cursor crosses
        while (again.next(h)) take(h, got);
        CHECK(again.done() && again.error().empty());
        CHECK(got.runes == want.runes && got.edges == want.edges);
    }
    Rodeo bad(st, "not a cursor");
    Herd h;
    CHECK(!bad.next(h) && !bad.error().empty());
}

void an_embarque_is_one_name_and_many_content_named_files() {
    const fs::path dir = scratch("embarque");
    std::string id;
    {
        EmbarqueWriter w(dir);
        CHECK(w.add("pages/index.html", "<h1>hola</h1>"));
        CHECK(w.add("pages/about.html", "<p>about</p>"));
        CHECK(w.add("pages/copy.html", "<h1>hola</h1>")); // the same bytes: stored once
        CHECK(!w.add("pages/index.html", "again"));        // a name is used once
        CHECK(w.written() == 2 && w.reused() == 1);
    }
    {
        EmbarqueWriter w(dir);
        CHECK(w.add("pages/index.html", "<h1>hola</h1>"));
        CHECK(w.add("pages/about.html", "<p>about</p>"));
        CHECK(w.add("pages/copy.html", "<h1>hola</h1>"));
        CHECK(w.written() == 0); // a resumed export rewrites nothing
        id = w.finish(R"({"site":"demo"})");
        CHECK(id.size() == 64);
        EmbarqueWriter same(dir);
        same.add("pages/index.html", "<h1>hola</h1>");
        same.add("pages/about.html", "<p>about</p>");
        same.add("pages/copy.html", "<h1>hola</h1>");
        CHECK(same.finish(R"({"site":"demo"})") == id); // the same embarque has the same name
    }
    std::string why;
    const auto e = read_embarque(dir, id, &why);
    CHECK(e.has_value());
    if (!e) return;
    CHECK(e->partidas.size() == 3 && e->bytes == 13 + 12 + 13);
    CHECK(json::parse(e->meta)["site"] == "demo");
    CHECK(e->partidas[0].name == "pages/index.html" && e->partidas[2].sha256 == e->partidas[0].sha256);
    CHECK(read_partida(dir, e->partidas[1].sha256) == std::optional<std::string>("<p>about</p>"));
    // a damaged partida is reported, never returned
    { std::ofstream(dir / "partidas" / e->partidas[1].sha256, std::ios::binary) << "<p>abuot</p>"; }
    CHECK(!read_partida(dir, e->partidas[1].sha256, &why).has_value());
    CHECK(why.find("damaged") != std::string::npos);
    CHECK(!read_embarque(dir, std::string(64, '0')).has_value());
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"a rodeo reads every rune and every edge once", a_rodeo_reads_every_rune_and_every_edge_once},
        {"a stopped rodeo resumes where it stood", a_stopped_rodeo_resumes_where_it_stood},
        {"an embarque is one name and many content-named files", an_embarque_is_one_name_and_many_content_named_files},
    };
    return test::run(cases);
}
