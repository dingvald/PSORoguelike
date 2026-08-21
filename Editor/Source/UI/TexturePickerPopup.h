#pragma once

#include "UI/FieldWidgets.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
class Element;
} // namespace Rml

namespace psr {

// A list picker over every .png under a textures directory (see
// EditorFilepaths::TexturesPath), presented by filename stem. Stems are
// hashed via entt::hashed_string, the same name-as-id convention used
// elsewhere in this project's content pipeline (JsonEntityLoader,
// NameIdRegistry) -- no texture-loading/atlas system exists in this project
// yet to cross-check the hash against, so this rescans the directory itself
// rather than querying anything.
//
// Bound against an already-loaded document, same ownership split as
// ColorPickerPopup -- the owning editor layer loads this popup's document and
// calls Bind() once.
class TexturePickerPopup
{
public:
    void Bind(Rml::ElementDocument& document);
    void Unbind();

    // Rescans textures_directory, shows the popup with current preselected
    // (highlighted, not otherwise special), and calls on_pick once when a row
    // is clicked (then closes -- a texture choice is discrete, unlike the
    // color picker's continuous live drag). on_pick receives both the picked
    // stem's hashed id and the stem name itself, so callers can retain the
    // name for round-tripping (hashing is one-way).
    void Open(const std::filesystem::path& textures_directory, std::uint32_t current,
              std::function<void(std::uint32_t, std::string)> on_pick);
    void Close();
    bool IsOpen() const { return m_open; }

private:
    void Refresh();

    Rml::ElementDocument* m_document = nullptr;
    Rml::Element* m_list = nullptr;

    std::vector<std::string> m_stems;
    std::uint32_t m_current = 0;
    bool m_open = false;
    std::function<void(std::uint32_t, std::string)> m_on_pick;

    fieldwidgets::Listeners m_listeners;
};

} // namespace psr
