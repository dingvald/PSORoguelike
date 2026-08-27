#pragma once

#include <string>

namespace psr {

// Escapes text for safe interpolation into an RmlUi document's InnerRML.
// Mirrors Editor/Source/UI/RmlText.h -- App can't include Editor sources
// (wrong dependency direction), so this small, stable, pure function is
// duplicated here rather than relocated (moving it to Core would mean
// touching every one of Editor's existing include sites blind).
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
