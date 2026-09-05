#pragma once

#include <string>

namespace psr {

// Converts a small inline markup syntax used by CombatLogEntryMessage/
// LootDropMessage text into escaped RML for HudLayer's event log:
//   [c=#RRGGBB]...[/c]  -- inline color
//   [b]...[/b]          -- bold
//   [i]...[/i]          -- italic
// Tags nest freely. Never throws: malformed hex colors and unrecognized
// [...] sequences are treated as literal (escaped) text, and unclosed tags
// are auto-closed at the end of the string, so one bad line can never break
// the whole log.
std::string ConvertLogMarkupToRml(const std::string& markup_text);

} // namespace psr
