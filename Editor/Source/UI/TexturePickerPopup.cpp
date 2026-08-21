#include "UI/TexturePickerPopup.h"

#include "UI/RmlClickListener.h"

#include <RmlUi/Core.h>

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <cctype>

namespace psr {

namespace {

    bool IsPngFile(const std::filesystem::directory_entry& entry)
    {
        if (!entry.is_regular_file())
            return false;
        std::string extension = entry.path().extension().string();
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return extension == ".png";
    }

} // namespace

void TexturePickerPopup::Bind(Rml::ElementDocument& document)
{
    m_document = &document;
    m_list = document.GetElementById("texture-list");

    if (Rml::Element* close = document.GetElementById("texture-picker-close"))
    {
        auto listener = std::make_unique<RmlClickListener>([this] { Close(); });
        listener->Attach(*close);
        m_listeners.push_back(std::move(listener));
    }
}

void TexturePickerPopup::Unbind()
{
    m_listeners.clear();
    m_document = nullptr;
    m_list = nullptr;
}

void TexturePickerPopup::Open(const std::filesystem::path& textures_directory, std::uint32_t current,
                              std::function<void(std::uint32_t, std::string)> on_pick)
{
    if (!m_document)
        return;
    m_current = current;
    m_on_pick = std::move(on_pick);

    m_stems.clear();
    std::error_code error_code;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(textures_directory, error_code))
    {
        if (IsPngFile(entry))
            m_stems.push_back(entry.path().stem().string());
    }
    std::sort(m_stems.begin(), m_stems.end());

    Refresh();
    m_open = true;
    m_document->Show();
}

void TexturePickerPopup::Close()
{
    m_open = false;
    m_on_pick = nullptr;
    if (m_document)
        m_document->Hide();
}

void TexturePickerPopup::Refresh()
{
    if (!m_list)
        return;

    // Rebuilding the row markup destroys the old row elements, so drop every
    // listener (including the close button's, re-wired below) before
    // SetInnerRML -- safe to call from Open() since Refresh() is never
    // invoked from inside one of these listeners' own callbacks (Close()
    // doesn't call it back in). Do not call Refresh() from a per-row click
    // callback without re-checking this invariant.
    m_listeners.clear();
    if (Rml::Element* close = m_document ? m_document->GetElementById("texture-picker-close") : nullptr)
    {
        auto listener = std::make_unique<RmlClickListener>([this] { Close(); });
        listener->Attach(*close);
        m_listeners.push_back(std::move(listener));
    }

    if (m_stems.empty())
    {
        m_list->SetInnerRML("<div class=\"texture-empty\">No textures found</div>");
        return;
    }

    std::string markup;
    for (const std::string& stem : m_stems)
    {
        const bool selected = entt::hashed_string::value(stem.c_str()) == m_current;
        markup += "<div class=\"texture-row";
        if (selected)
            markup += " selected";
        markup += "\">" + stem + "</div>";
    }
    m_list->SetInnerRML(markup);

    Rml::ElementList rows;
    m_list->QuerySelectorAll(rows, ".texture-row");
    for (std::size_t i = 0; i < rows.size() && i < m_stems.size(); ++i)
    {
        const std::string stem = m_stems[i];
        auto listener = std::make_unique<RmlClickListener>(
            [this, stem]
            {
                const std::uint32_t id = entt::hashed_string::value(stem.c_str());
                if (m_on_pick)
                    m_on_pick(id, stem);
                Close();
            });
        listener->Attach(*rows[i]);
        m_listeners.push_back(std::move(listener));
    }
}

} // namespace psr
