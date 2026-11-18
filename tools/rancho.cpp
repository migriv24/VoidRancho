/* rancho — make and inspect a database's deed (rancho/deed.hpp,
 * okf/concepts/deed.md). A tool for a person; everything it does is the
 * library's, so a host can do the same in code.
 *
 *   rancho new   <file.rancho> [name]      a new database: a deed and its store
 *   rancho deed  <store.db> [file.rancho]  a deed for a store that has none
 *   rancho check <file.rancho|store.db>    its parts, where each mantle lives, what is absent
 *   rancho ids   <file.rancho>             write the mantles it names by name as their ids
 *   rancho embed <file.rancho>             put the deed inside its home store: one file
 *
 * `check` exits 0 when every part is present, 1 when the database is
 * incomplete (it still opened), 2 when the file is not a deed. */
#include "rancho/deed.hpp"
#include "rancho/store.hpp"

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
using namespace rancho;

namespace {

int usage() {
    std::fputs("usage:\n"
               "  rancho new   <file.rancho> [name]\n"
               "  rancho deed  <store.db> [file.rancho]\n"
               "  rancho check <file.rancho|store.db>\n"
               "  rancho ids   <file.rancho>\n"
               "  rancho embed <file.rancho>\n",
               stderr);
    return 2;
}

int cmd_new(const fs::path& file, const std::string& name) {
    std::string err;
    auto db = Database::create(file, name.empty() ? file.stem().string() : name, {}, &err);
    if (!db) {
        std::fprintf(stderr, "rancho: %s\n", err.c_str());
        return 2;
    }
    std::printf("%s: database %s, home %s\n", file.string().c_str(), db->deed().id.c_str(),
                db->parts()[0].where.string().c_str());
    return 0;
}

int cmd_deed(const fs::path& store_file, fs::path file) {
    std::error_code ec;
    if (!fs::exists(store_file, ec)) {
        std::fprintf(stderr, "rancho: no store at %s\n", store_file.string().c_str());
        return 2;
    }
    if (file.empty()) file = store_file.parent_path() / (store_file.stem().string() + ".rancho");
    if (fs::exists(file, ec)) {
        std::fprintf(stderr, "rancho: %s already exists\n", file.string().c_str());
        return 2;
    }
    Store s(store_file);
    if (!s.ok()) {
        std::fprintf(stderr, "rancho: %s\n", s.error().c_str());
        return 2;
    }
    const Deed d = deed_for_store(s, file.parent_path(), store_file.stem().string());
    std::string err;
    if (!write_deed(d, file, &err)) {
        std::fprintf(stderr, "rancho: %s\n", err.c_str());
        return 2;
    }
    std::printf("%s: database %s, one part (%s)\n", file.string().c_str(), d.id.c_str(), d.parts[0].path.c_str());
    return 0;
}

int cmd_check(const fs::path& file) {
    std::string err;
    auto db = Database::open(file, {}, &err);
    if (!db) {
        std::fprintf(stderr, "rancho: %s is not a deed: %s\n", file.string().c_str(), err.c_str());
        return 2;
    }
    const Deed& d = db->deed();
    std::printf("database %s%s%s\n", d.id.c_str(), d.name.empty() ? "" : "  ", d.name.c_str());
    const Part* home = d.home_part();
    for (std::size_t i = 0; i < d.parts.size(); ++i) {
        const Part& p = d.parts[i];
        const PartState& st = db->parts()[i];
        std::printf("  part %-12s %-9s %-13s %s%s\n", p.id.c_str(), p.form.c_str(), part_status_name(st.status),
                    st.where.string().c_str(), &p == home ? "  (home)" : "");
        if (!st.reason.empty()) std::printf("      %s\n", st.reason.c_str());
    }
    std::printf("mantles\n");
    for (const auto& m : db->placement())
        std::printf("  %-24s in %-12s %-9s %s\n", m.mantle.c_str(), m.part.c_str(), m.present ? "" : "(absent)",
                    m.key == m.mantle ? "" : m.key.c_str());
    for (const auto& n : db->notes()) std::printf("note: %s\n", n.c_str());
    std::printf("%s\n", db->complete() ? "complete" : "incomplete: what is absent is unknown, not deleted");
    return db->complete() ? 0 : 1;
}

int cmd_ids(const fs::path& file) {
    std::string err;
    auto db = Database::open(file, {}, &err);
    if (!db) {
        std::fprintf(stderr, "rancho: %s is not a deed: %s\n", file.string().c_str(), err.c_str());
        return 2;
    }
    const Deed d = db->deed_by_id();
    int changed = 0;
    for (std::size_t i = 0; i < d.parts.size(); ++i)
        for (std::size_t k = 0; k < d.parts[i].mantles.size(); ++k)
            changed += d.parts[i].mantles[k] != db->deed().parts[i].mantles[k];
    db.reset();
    if (changed && !write_deed(d, file, &err)) {
        std::fprintf(stderr, "rancho: %s\n", err.c_str());
        return 2;
    }
    std::printf("%s: %d mantle%s now named by id\n", file.string().c_str(), changed, changed == 1 ? "" : "s");
    return 0;
}

int cmd_embed(const fs::path& file) {
    std::string err;
    auto deed = read_deed(file, &err);
    if (!deed) {
        std::fprintf(stderr, "rancho: %s is not a deed: %s\n", file.string().c_str(), err.c_str());
        return 2;
    }
    const Part* home = deed->home_part();
    if (!home || home->form != "store") {
        std::fprintf(stderr, "rancho: the deed's home is not a store: nothing to carry it\n");
        return 2;
    }
    const fs::path store_file = file.parent_path() / home->path;
    Store s(store_file);
    if (!s.ok()) {
        std::fprintf(stderr, "rancho: %s\n", s.error().c_str());
        return 2;
    }
    if (!embed_deed(s, *deed, &err)) {
        std::fprintf(stderr, "rancho: %s\n", err.c_str());
        return 2;
    }
    std::printf("%s now carries the deed of database %s: it opens alone\n", store_file.string().c_str(),
                deed->id.c_str());
    return 0;
}

} // namespace

/* The arguments as paths and as UTF-8 text. On Windows `argv` arrives in the
 * ANSI code page, which cannot hold every name a person gives a folder, so the
 * wide command line is read instead (Void Squid's note, 2026-11-09). */
struct Args {
    std::vector<fs::path> paths;
    std::vector<std::string> text;
};

Args arguments(int argc, char** argv) {
    Args a;
#ifdef _WIN32
    int n = 0;
    if (LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n)) {
        for (int i = 0; i < n; ++i) {
            a.paths.emplace_back(std::wstring(w[i]));
            const int len = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
            std::string u(len > 0 ? (std::size_t)len - 1 : 0, '\0');
            if (len > 1) WideCharToMultiByte(CP_UTF8, 0, w[i], -1, u.data(), len, nullptr, nullptr);
            a.text.push_back(std::move(u));
        }
        LocalFree(w);
        return a;
    }
#endif
    for (int i = 0; i < argc; ++i) {
        a.paths.emplace_back(argv[i]);
        a.text.emplace_back(argv[i]);
    }
    return a;
}

int main(int argc, char** argv) {
    const Args a = arguments(argc, argv);
    const int n = (int)a.text.size();
    if (n < 3) return usage();
    const std::string& cmd = a.text[1];
    if (cmd == "new") return cmd_new(a.paths[2], n > 3 ? a.text[3] : "");
    if (cmd == "deed") return cmd_deed(a.paths[2], n > 3 ? a.paths[3] : fs::path());
    if (cmd == "check") return cmd_check(a.paths[2]);
    if (cmd == "ids") return cmd_ids(a.paths[2]);
    if (cmd == "embed") return cmd_embed(a.paths[2]);
    return usage();
}
