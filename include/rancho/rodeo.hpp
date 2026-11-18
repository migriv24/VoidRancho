/* rancho/rodeo.hpp — el rodeo and el embarque: reading the whole potrero in
 * bounded memory, and writing what it makes as one thing in many files
 * (okf/concepts/rodeo.md; Void Hormiga's "el lindero", 2026-11-03).
 *
 * THE AUTHOR (2026-11-03, relayed by Hormiga): an export that turns a database
 * into another modality (a newsletter, a site, a compiled game) "has to be
 * able to completely traverse the database and encapsulate it fully", and a
 * petabyte output should not be a petabyte file.
 *
 * EL RODEO is a pass over every rune and every edge of a store, a herd at a
 * time, in the store's own order: per mantle, its always-loaded runes, then
 * each cluster leaf in path order, then its unclustered runes in pages, then
 * the edges whose source is no rune. Each edge comes once, with the herd that
 * holds its source. Memory is one herd's. A `cursor()` taken after any herd
 * resumes the pass from there, in this process or another, so a pass that is
 * stopped is not lost. What a rodeo has made is SUELTO (loose) until `done()`;
 * then it is CABAL: nothing was left unread. A rodeo reads; it never writes.
 * For an answer as of one moment, run it on a copy (`Store::vacuum_into`)
 * while the original takes edits.
 *
 * EL EMBARQUE is the output: one thing in identity, many in storage. Each
 * PARTIDA (a page, a level, an asset pack: the OUTPUT's structure cuts them,
 * not the database's) is an immutable file named by the SHA-256 of its bytes,
 * as segments are; a manifest names them in order, and the manifest's own hash
 * names the embarque. So a petabyte embarque is one name, one version, one
 * signature, and never one file: it is written, resumed, copied, verified and
 * served a partida at a time, and a partida two embarques share is stored
 * once. The writer does not care what a partida holds.
 *
 *   <dir>/partidas/<sha256>         the bytes, never changed once written
 *   <dir>/embarques/<sha256>.json   a manifest: format, meta, partidas in order
 *
 * EL MOLINO, an output made on demand from the database (a page rendered when
 * asked, a world streamed as a player moves), needs no new mechanism: it is a
 * host reading the store, and stays possible beside these. */
#pragma once

#include "rancho/store.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rancho {

/* One herd of a rodeo: runes and the edges from them, as Core wrote them. */
struct Herd {
    std::string mantle;              // its name
    std::string path;                // a leaf, "!" (always-loaded), "page", or "edges" (sources that are no rune)
    std::vector<std::string> runes;  // each a rune object
    std::vector<std::string> edges;  // each an edge object
};

struct RodeoOptions {
    std::uint64_t page_bytes = 4ull << 20; // unclustered runes, and edges with no source rune, in pages of about this
};

class Rodeo {
public:
    /* From the start, or from a cursor an earlier rodeo gave. */
    explicit Rodeo(Store& store, const std::string& cursor = {}, const RodeoOptions& options = {});
    /* The next herd; false when the pass is over (then `done()`), or on error. */
    bool next(Herd& out);
    bool done() const { return done_; }
    /* Where the pass stands: give it to a new Rodeo to resume after the last
     * herd `next` returned. */
    std::string cursor() const;
    const std::string& error() const { return error_; }

private:
    bool advance(Herd& out);
    Store& store_;
    RodeoOptions options_;
    std::vector<std::pair<long long, std::string>> mantles_; // id, name in order
    std::size_t m_ = 0;
    int stage_ = 0;        // 0 always-loaded, 1 leaves, 2 unclustered pages, 3 edges with no source rune
    std::string key_;      // the last leaf, or the last position, of the stage
    bool done_ = false;
    std::string error_;
};

struct PartidaRef {
    std::string name;    // the output's own name for it ("pages/index.html")
    std::string sha256;
    std::uint64_t bytes = 0;
};

class EmbarqueWriter {
public:
    explicit EmbarqueWriter(const std::filesystem::path& dir);
    /* Write one partida. Content-named: if its bytes are already there (a
     * resumed export, or another embarque's), nothing is written. A name may
     * appear once. */
    bool add(const std::string& name, std::string_view bytes);
    /* Write the manifest; returns its SHA-256, the embarque's name ("" on
     * failure). `meta` is a JSON object the host chooses. */
    std::string finish(const std::string& meta_json = "{}");
    const std::vector<PartidaRef>& partidas() const { return partidas_; }
    std::uint64_t written() const { return written_; }
    std::uint64_t reused() const { return reused_; }
    const std::string& error() const { return error_; }

private:
    std::filesystem::path dir_;
    std::vector<PartidaRef> partidas_;
    std::vector<std::string> names_; // sorted, to refuse a repeated name
    std::uint64_t written_ = 0, reused_ = 0;
    std::string error_;
};

struct Embarque {
    std::string id, meta;              // meta: the JSON object given to `finish`
    std::vector<PartidaRef> partidas;
    std::uint64_t bytes = 0;
};

/* A manifest, checked against its name. */
std::optional<Embarque> read_embarque(const std::filesystem::path& dir, const std::string& id,
                                      std::string* error = nullptr);
/* One partida's bytes, checked against its name: a corrupted file is
 * reported, never returned. */
std::optional<std::string> read_partida(const std::filesystem::path& dir, const std::string& sha256,
                                        std::string* error = nullptr);

} // namespace rancho
