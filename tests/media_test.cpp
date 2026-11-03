/* media_test.cpp — one host at a time where the medium cannot share
 * (rancho/media.hpp, okf/design/storage-research.md §1).
 *
 * A real network share is not something a test can count on, so the network
 * branch is reached two ways: the path-spelling half (UNC) directly, and the
 * locking half through Sharing::SingleHost, which opens a store exactly as a
 * network path does. */
#include "check.hpp"

#include "rancho/media.hpp"
#include "rancho/store.hpp"

#include "sqlite3.h"

namespace fs = std::filesystem;
using namespace rancho;

namespace {

std::string journal_mode(Store& st) {
    sqlite3_stmt* q = nullptr;
    sqlite3_prepare_v2(st.handle(), "PRAGMA journal_mode", -1, &q, nullptr);
    std::string m = sqlite3_step(q) == SQLITE_ROW ? (const char*)sqlite3_column_text(q, 0) : "";
    sqlite3_finalize(q);
    return m;
}

void a_unc_path_is_a_network_path_however_it_is_spelled() {
    CHECK(is_unc("\\\\nas\\share\\org.store"));
    CHECK(is_unc("//nas/share/org.store"));
    CHECK(is_unc("\\\\?\\UNC\\nas\\share\\org.store"));
    CHECK(!is_unc("\\\\?\\C:\\data\\org.store")); // a local path, spelled long
    CHECK(!is_unc("C:\\data\\org.store"));
    CHECK(!is_unc("/home/me/org.store"));
    CHECK(!is_unc("\\\\"));
    CHECK(detect_medium("\\\\nas\\share\\org.store") == Medium::Network);
}

void the_temp_folder_is_local_and_a_local_store_uses_wal() {
    const fs::path p = test::scratch("media-test", "local.store");
    CHECK(detect_medium(p) == Medium::Local);
    Store st(p);
    CHECK(st.ok());
    CHECK(st.medium() == Medium::Local);
    CHECK(!st.exclusive());
    CHECK(journal_mode(st) == "wal");
}

void a_single_host_store_refuses_a_second_opener() {
    const fs::path p = test::scratch("media-test", "single.store");
    OpenOptions one;
    one.sharing = Sharing::SingleHost;
    {
        Store first(p, one);
        CHECK(first.ok());
        CHECK(first.exclusive());
        CHECK(journal_mode(first) == "delete"); // never WAL: it needs shared memory on one machine
        JsonStream in = JsonStream::from_text(R"({"version":1,"mantles":[{"id":"m","name":"a","runes":[]}]})");
        CHECK(first.import(in).ok);
        Store second(p); // another process, another machine: the same answer
        CHECK(!second.ok());
        CHECK(second.error().find("in use by another process or machine") != std::string::npos);
        CHECK(first.ok() && first.has_document()); // and the first is untouched
    }
    Store again(p, one); // once the first is closed, the next may open it
    CHECK(again.ok());
    CHECK(again.has_document());
}

} // namespace

int main() {
    const test::Case cases[] = {
        {"a UNC path is a network path however it is spelled", a_unc_path_is_a_network_path_however_it_is_spelled},
        {"the temp folder is local, and a local store uses WAL", the_temp_folder_is_local_and_a_local_store_uses_wal},
        {"a single-host store refuses a second opener", a_single_host_store_refuses_a_second_opener},
    };
    return test::run(cases);
}
