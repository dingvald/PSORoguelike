#pragma once

#include <string>

namespace psr {

// Escapes text for safe interpolation into an RmlUi document's InnerRML --
// shared by every content editor layer that builds list/row markup from
// user-authored strings (id/name/etc.), rather than each keeping its own
// private copy (the pattern UnnamedRoguelike's editor layers each do).
inline std::string EscapeRml(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}

} // namespace psr
