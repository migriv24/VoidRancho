/* capi_test.cpp — the C ABI does what the C++ does (rancho/rancho.h). Written
 * against the header only, as a C host would use it. */
#include "check.hpp"

#include "rancho/rancho.h"

#include <fstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

void a_c_host_imports_queries_snapshots_and_exports() {
    rancho::VcCore c;
    c.dispatch("mantle new m");
    c.dispatch(rancho::batch({"rune new text a", "rune new text b", "tag a +x", "link a b --weight 2"}));
    const fs::path doc = test::scratch("capi-test", "doc.json");
    std::ofstream(doc, std::ios::binary) << c.export_state();

    CHECK(rancho_open(test::scratch("capi-test", "bad/dir/that/is/not/there/x.store").string().c_str()) == nullptr);
    CHECK(std::string(rancho_last_open_error()).size() > 0);

    RanchoStore* s = rancho_open(test::scratch("capi-test", "c.store").string().c_str());
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(rancho_import_file(s, doc.string().c_str()) == 1);
    CHECK(std::string(rancho_error(s)).empty());
    char* names = rancho_select(s, "m", "x OR b");
    CHECK(json::parse(names) == json::array({"a", "b"}));
    rancho_free_str(names);
    char* ms = rancho_mantles(s);
    CHECK(json::parse(ms)[0]["edges"] == 1);
    rancho_free_str(ms);
    char* st = rancho_export_state(s);
    CHECK(test::plain(st) == test::plain(c.export_state()));
    rancho_free_str(st);
    const fs::path dir = test::scratch("capi-test", "snap");
    CHECK(rancho_snapshot(s, dir.string().c_str()) == 1);
    CHECK(rancho_restore(s, dir.string().c_str(), 0) == 1);
    CHECK(rancho_export_npy(s, "m", test::scratch("capi-test", "npy").string().c_str()) == 1);
    CHECK(rancho_export_npy(s, "nope", test::scratch("capi-test", "npy2").string().c_str()) == 0);
    CHECK(std::string(rancho_error(s)).find("no such mantle") != std::string::npos);
    CHECK(std::string(rancho_version()) == "0.1.0");
    rancho_close(s);
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"a C host imports, queries, snapshots and exports", a_c_host_imports_queries_snapshots_and_exports},
    };
    return test::run(cases);
}
