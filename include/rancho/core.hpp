/* rancho/core.hpp — the one seam between Void Rancho and a Void Core manager
 * (okf/design/boundaries.md §5).
 *
 * Rancho never changes data; it keeps a store in step with what a Core says
 * after the host's commands land (okf/concepts/write-path.md). So what it needs
 * of a Core is small and is all reads, plus the reload a working set is:
 *
 *   dispatch      — the one door. Rancho itself only sends READS through it
 *                   (`cat`, `links`, `config`) to learn what a command did; the
 *                   host's own commands go through WorkingSet::dispatch, which
 *                   forwards here unchanged.
 *   export_state  — the whole document, for the full comparison.
 *   replace_state — a new working set: a reload, never an edit.
 *   revision      — changes whenever the state MAY have changed.
 *
 * CoreLink is an interface ON PURPOSE. A host already owns its Core, with its
 * glyph registrations, log sink, effect handler and undo depth; Void Hormiga's
 * is a `maiz::Core`, and Void Maiz ships the adapter that makes it a CoreLink:
 * `maiz::RanchoLink` in voidmaiz/rancho.hpp (header-only; Maiz never links
 * Rancho, Rancho never includes Maiz's Core). Its revision does not move on the
 * reads a sync sends, so a sync costs the host none of its cached exports.
 * VcCore is the default, straight over Void
 * Core's C ABI, for hosts with nothing of their own and for Rancho's tests.
 *
 * Not thread-safe, as Void Core is not: one CoreLink per thread. */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct VC_Manager;

namespace rancho {

/* One dispatch's envelope (Void Core SPEC §6): {"ok":bool,"lines":[...],"data":any}. */
struct Reply {
    bool ok = false;
    std::string data = "null"; // the machine value, as compact JSON
    std::string text;          // the lines, joined with '\n'
    explicit operator bool() const { return ok; }
};

class CoreLink {
public:
    virtual ~CoreLink() = default;
    virtual Reply dispatch(std::string_view command) = 0;
    virtual std::string export_state() = 0;
    /* False, changing nothing, if the document could not be taken. */
    virtual bool replace_state(std::string_view state_json) = 0;
    virtual std::uint64_t revision() const = 0;
};

/* ── the SPEC §6.1 command codec, forwarded to Void Core ─────────────────────
 * Not implemented here, for the reason Void Maiz gives at length in its
 * embed.hpp: five codebases implemented §6.1 by hand and all five got it
 * wrong. These forward to vc_arg_quote and vc_argv_split_json. */
std::string arg(std::string_view value);
/* `batch '<json array of command lines>'`: several commands as one atomic one. */
std::string batch(const std::vector<std::string>& commands);
struct Argv {
    bool ok = false;
    std::vector<std::string> argv;
};
Argv split_argv(std::string_view line);

/* SPEC §5's one evaluator: does a rune with these tags match `expr`? The caller
 * puts the rune's name and "glyph:<g>" in `tags`, as `ls --tag` sees them. */
bool tag_match(std::string_view expr, const std::vector<std::string>& tags);

std::string_view core_version();

/* The default CoreLink: one VC_Manager, owned. Remembers the glyphs registered
 * through it and the undo settings, so replace_state keeps them (registered
 * glyphs are host config and do not travel in an exported document). */
class VcCore final : public CoreLink {
public:
    VcCore();                                  // the empty state
    explicit VcCore(std::string_view state_json);
    ~VcCore() override;
    VcCore(const VcCore&) = delete;
    VcCore& operator=(const VcCore&) = delete;

    Reply dispatch(std::string_view command) override;
    std::string export_state() override;
    bool replace_state(std::string_view state_json) override;
    std::uint64_t revision() const override { return rev_; }

    bool register_glyph(std::string_view glyph_json);
    void set_undo(bool on);
    void set_undo_depth(int depth);
    void set_journal(bool on);
    std::string export_journal(); // "[]" when the journal is off

    VC_Manager* raw() { ++rev_; return m_; } // the handle hands out the means to change anything

private:
    VC_Manager* m_ = nullptr;
    std::uint64_t rev_ = 1;
    std::vector<std::string> glyphs_;
    int undo_ = -1, undo_depth_ = -1, journal_ = -1; // -1: never set
};

} // namespace rancho
