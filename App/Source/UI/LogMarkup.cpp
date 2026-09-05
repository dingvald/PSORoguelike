#include "UI/LogMarkup.h"

#include "UI/RmlText.h"

#include <cstddef>
#include <vector>

namespace psr {

namespace {

    enum class LogMarkupTagKind
    {
        Color,
        Bold,
        Italic
    };

    bool IsHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

    bool IsValidHexColor(const std::string& value)
    {
        if (value.size() != 7 || value[0] != '#')
            return false;
        for (std::size_t i = 1; i < value.size(); ++i)
            if (!IsHexDigit(value[i]))
                return false;
        return true;
    }

} // namespace

std::string ConvertLogMarkupToRml(const std::string& markup_text)
{
    std::string output;
    std::string pending_literal;
    std::vector<LogMarkupTagKind> open_tags;

    const auto flush_pending_literal = [&]
    {
        if (!pending_literal.empty())
        {
            output += EscapeRml(pending_literal);
            pending_literal.clear();
        }
    };

    // Closes the innermost open tag of `kind`; if it isn't the topmost open
    // tag, forces every tag opened after it closed too, so the emitted RML
    // stays balanced even when a message's tags aren't properly nested.
    const auto close_tag = [&](LogMarkupTagKind kind)
    {
        for (std::size_t idx = open_tags.size(); idx > 0; --idx)
        {
            if (open_tags[idx - 1] != kind)
                continue;

            flush_pending_literal();
            while (open_tags.size() >= idx)
            {
                output += "</span>";
                open_tags.pop_back();
            }
            return;
        }
    };

    std::size_t i = 0;
    while (i < markup_text.size())
    {
        if (markup_text[i] != '[')
        {
            pending_literal += markup_text[i];
            ++i;
            continue;
        }

        const std::size_t close_bracket = markup_text.find(']', i + 1);
        if (close_bracket == std::string::npos)
        {
            pending_literal += markup_text[i];
            ++i;
            continue;
        }

        const std::string tag = markup_text.substr(i + 1, close_bracket - i - 1);
        i = close_bracket + 1;

        if (tag == "b")
        {
            flush_pending_literal();
            open_tags.push_back(LogMarkupTagKind::Bold);
            output += "<span style=\"font-weight:bold;\">";
        }
        else if (tag == "i")
        {
            flush_pending_literal();
            open_tags.push_back(LogMarkupTagKind::Italic);
            output += "<span style=\"font-style:italic;\">";
        }
        else if (tag.size() > 2 && tag[0] == 'c' && tag[1] == '=' && IsValidHexColor(tag.substr(2)))
        {
            flush_pending_literal();
            open_tags.push_back(LogMarkupTagKind::Color);
            output += "<span style=\"color:" + tag.substr(2) + ";\">";
        }
        else if (tag == "/b")
        {
            close_tag(LogMarkupTagKind::Bold);
        }
        else if (tag == "/i")
        {
            close_tag(LogMarkupTagKind::Italic);
        }
        else if (tag == "/c")
        {
            close_tag(LogMarkupTagKind::Color);
        }
        else
        {
            pending_literal += "[" + tag + "]";
        }
    }

    flush_pending_literal();
    while (!open_tags.empty())
    {
        output += "</span>";
        open_tags.pop_back();
    }

    return output;
}

} // namespace psr
