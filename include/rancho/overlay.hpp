/* rancho/overlay.hpp — a person's edits over a part they do not own
 * (okf/concepts/deed.md, "Overlays"; Void Squid's specification, 2026-11-09).
 *
 * A game library is rebuilt by a toolbox; a project reads it as a read-only
 * part and a person edits its runes: a tag added, a tag removed, a kind
 * chosen, a sound set, a rune of their own made inside a library mantle. Each
 * edit must survive the library being rebuilt. An OVERLAY keeps, per base rune
 * (keyed by its mantle's key and its spirit id, never by its name), the
 * DIFFERENCE between what the base says and what the person's Core says, and
 * reading the part gives base ⊕ overlay. Void Squid's splice is the
 * specification, rule by rule:
 *
 *  1. a tag the person added stays, with its source;
 *  2. a tag the person removed stays removed, even when a rebuilt base has it
 *     again (a removal is recorded as a removal, not as an absence);
 *  3. a member the person set (a content field, a facet, the glyph, the
 *     placement) wins over the base's value;
 *  4. a rune the person made inside a base mantle stays, whole;
 *  5. identity is kept: the overlay is keyed by spirit id;
 *  6. placement (where a rune sits when drawn) is the person's: rule 3;
 *  7. an edit to a base rune that a rebuild removed is never dropped silently:
 *     it is kept, and named as a RESIDUAL (the frontier's rule, "never read
 *     the absence as deletions", applied to a base that moved).
 *
 * A difference is a JSON object:
 *   { "tags_added": {"<tag>": "<source>"}, "tags_removed": ["<tag>"],
 *     "set": {"<member>" or "<member>/<key>": <value>}, "unset": ["<member>/<key>"],
 *     "made": <the whole rune>, "source": "<who made it>" }
 * Members absent when empty. These functions are pure: rows are the store's,
 * and only `Database::record_overlay` writes them, from what Core says. */
#pragma once

#include <map>
#include <string>

namespace rancho::overlay {

/* What `mine` (the rune as the person's Core has it) changes of `base`; "" if
 * nothing. `sources`: the source of each tag already recorded as added, kept;
 * a tag newly added is `source`'s. */
std::string diff(const std::string& base, const std::string& mine, const std::string& source,
                 const std::map<std::string, std::string>& sources = {});
/* A rune the person made, which no base holds. */
std::string made(const std::string& rune, const std::string& source);
bool is_made(const std::string& diff);
/* base ⊕ difference: the rune the person sees. For a made rune, `base` is
 * ignored and the rune is returned as made. */
std::string apply(const std::string& base, const std::string& diff);
/* The sources of the tags a difference added. */
std::map<std::string, std::string> added_tags(const std::string& diff);

} // namespace rancho::overlay
