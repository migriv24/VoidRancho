/* authority_test.cpp — which file is in charge (rancho/authority.hpp,
 * okf/concepts/authority.md). A cache is found again for its authority and for
 * no other file; two copies of one database get two caches; the note says
 * whether the cache can be used, must be rebuilt, or holds unsaved work, from
 * the authority's stamp alone; Save As moves the cache to the new file without
 * moving it; forgetting a cache loses nothing the authority holds. */
#include "check.hpp"

#include "rancho/authority.hpp"

#include <chrono>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace rancho;

namespace {

void write(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary | std::ios::trunc) << text;
}

/* A file clock can be coarse: change the file until its stamp moves. */
void change(const fs::path& p, const std::string& text) {
    const Stamp before = stamp_of(p);
    for (int i = 0; i < 50; ++i) {
        write(p, text + std::string((size_t)i, ' '));
        if (!(stamp_of(p) == before)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void a_cache_is_found_again_for_its_authority_only() {
    const fs::path root = test::scratch("authority", "root1");
    const fs::path a = test::scratch("authority", "a.miga"), b = test::scratch("authority", "b.miga");
    write(a, "A");
    write(b, "B");
    const fs::path ca = working_copy_for(a, root);
    CHECK(fs::is_directory(ca));
    CHECK(cache_state(ca) == CacheState::New);
    CHECK(working_copy_for(a, root) == ca);                        // the same file: the same cache
    CHECK(working_copy_for(a.parent_path() / "." / a.filename(), root) == ca); // spelled differently, still it
    const fs::path cb = working_copy_for(b, root);
    CHECK(cb != ca);
    CHECK(caches(root).size() == 2);
}

void two_copies_of_one_database_get_two_caches() {
    const fs::path root = test::scratch("authority", "root2");
    const fs::path a = test::scratch("authority", "here/db.miga"), b = test::scratch("authority", "there/db.miga");
    write(a, "same bytes");
    write(b, "same bytes");
    const fs::path ca = working_copy_for(a, root), cb = working_copy_for(b, root);
    CHECK(ca != cb); // the same database, two files: a working copy each
    CacheNote n = *read_note(ca);
    n.database = "db-1";
    CHECK(write_note(ca, n));
    CHECK(read_note(ca)->database == "db-1");
}

void the_note_says_how_the_cache_stands() {
    const fs::path root = test::scratch("authority", "root3");
    const fs::path a = test::scratch("authority", "c.miga");
    write(a, "version one");
    const fs::path c = working_copy_for(a, root);
    CHECK(cache_state(c) == CacheState::New);
    CHECK(mark_matched(c)); // filled from the authority
    CHECK(cache_state(c) == CacheState::Current);
    CHECK(mark_ahead(c)); // an edit, not saved yet
    CHECK(cache_state(c) == CacheState::Ahead);
    CHECK(mark_matched(c)); // saved into the authority
    CHECK(cache_state(c) == CacheState::Current);
    change(a, "version two, saved by another copy"); // the authority moved on without us
    CHECK(cache_state(c) == CacheState::Stale);
    CHECK(mark_matched(c));
    CHECK(mark_ahead(c));
    change(a, "version three");
    CHECK(cache_state(c) == CacheState::Diverged); // both changed: keep the edits aside first
    fs::remove(a);
    CHECK(cache_state(c) == CacheState::Orphan); // the authority is gone; the cache may be all there is
}

void save_as_moves_the_cache_to_the_new_file() {
    const fs::path root = test::scratch("authority", "root4");
    const fs::path a = test::scratch("authority", "old.miga"), b = test::scratch("authority", "new.miga");
    write(a, "A");
    const fs::path c = working_copy_for(a, root);
    write(b, "A, saved as");
    CHECK(reassign(c, b, root));
    CHECK(working_copy_for(b, root) == c);      // the new file finds this cache
    CHECK(working_copy_for(a, root) != c);      // and the old file has a new, empty one
    CHECK(read_note(c)->authority.filename() == "new.miga");
}

void forgetting_a_cache_loses_nothing_the_authority_holds() {
    const fs::path root = test::scratch("authority", "root5");
    const fs::path a = test::scratch("authority", "d.miga");
    write(a, "everything");
    const fs::path c = working_copy_for(a, root);
    write(c / "state.json", "a copy");
    CHECK(forget(c, root));
    CHECK(!fs::exists(c));
    CHECK(fs::exists(a)); // the authority is untouched
    const fs::path again = working_copy_for(a, root);
    CHECK(cache_state(again) == CacheState::New); // rebuilt from it, as on a first open
}

void the_roots_are_per_user_and_overridable() {
    CHECK(!cache_root("host").empty());
    CHECK(!keep_root("host").empty());
    CHECK(cache_root("host") != keep_root("host")); // a keepsake is never in the cache
    CHECK(cache_root("host").filename() == "host" || cache_root("host").filename() == "cache");
}

} // namespace

int main() {
    a_cache_is_found_again_for_its_authority_only();
    two_copies_of_one_database_get_two_caches();
    the_note_says_how_the_cache_stands();
    save_as_moves_the_cache_to_the_new_file();
    forgetting_a_cache_loses_nothing_the_authority_holds();
    the_roots_are_per_user_and_overridable();
    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "authority_test: ok\n";
    return failures ? 1 : 0;
}
