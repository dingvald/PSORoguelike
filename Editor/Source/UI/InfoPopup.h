#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Rml {
class ElementDocument;
class Element;
} // namespace Rml

namespace psr {

class RmlClickListener;

// A single-message modal with one OK button -- used to report the result of
// an action the user didn't step through interactively (e.g. how many other
// assets a rename touched). Bound against an already-loaded document, same
// ownership split as ColorPickerPopup/TexturePickerPopup: the owning editor
// layer loads this popup's document and calls Bind() once.
class InfoPopup
{
public:
    void Bind(Rml::ElementDocument& document);
    void Unbind();

    void Open(const std::string& message);
    void Close();
    bool IsOpen() const { return m_open; }

private:
    Rml::ElementDocument* m_document = nullptr;
    Rml::Element* m_message = nullptr;

    bool m_open = false;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;
};

} // namespace psr
