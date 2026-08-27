#include "Layers/AffixEditorLayer.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Items/AffixLibraryFile.h"
#include "Layers/EditorMenuLayer.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <EditorFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <entt/core/hashed_string.hpp>

#include <array>
#include <cstddef>
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "affix_editor.rml";

    // Turns an entered id ("power") into its file path ("Affixes/power.json"),
    // mirroring LoadJsonDirectory's reverse rule, same as PieceEditorLayer.cpp/
    // DungeonEditorLayer.cpp's IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::AffixesPath;
        std::string segment;
        for (char c : id)
        {
            if (c == '.')
            {
                path /= segment;
                segment.clear();
            }
            else
            {
                segment += c;
            }
        }
        path /= segment;
        path += ".json";
        return path;
    }

    // Generic BuildEnumField backing, same shape as PrefabEditorLayer.cpp's --
    // written once against EnumNames<E> rather than a per-enum Options/
    // ToString/FromString trio.
    template <typename E> std::vector<std::string> EnumOptions()
    {
        std::vector<std::string> options;
        for (const auto& [text, value] : EnumNames<E>::kValues)
        {
            (void)value;
            options.push_back(std::string{text});
        }
        return options;
    }

    template <typename E> std::string EnumToString(E value)
    {
        for (const auto& [text, candidate] : EnumNames<E>::kValues)
            if (candidate == value)
                return std::string{text};
        return std::string{EnumNames<E>::kValues.front().first}; // unreachable for a valid enum value
    }

    template <typename E> E EnumFromString(const std::string& text, E fallback)
    {
        for (const auto& [name, value] : EnumNames<E>::kValues)
            if (name == text)
                return value;
        return fallback;
    }
} // namespace

AffixEditorLayer::AffixEditorLayer() : Layer("AffixEditorLayer") {}
AffixEditorLayer::~AffixEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void AffixEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: AffixEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: AffixEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    LoadDocuments();
    ReloadAffixLibrary();
    RefreshAffixList();
    ShowScreen(Mode::List);
}

void AffixEditorLayer::OnDetach()
{
    m_form_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void AffixEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: AffixEditorLayer has no editor document");
        return;
    }

    WireButtonClick("new-affix", [this] { BeginNewAffix(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-affix", [this] { SaveDraft(); });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshAffixList();
                    });

    m_editor->Show();
}

void AffixEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
{
    if (!m_editor)
        return;
    Rml::Element* element = m_editor->GetElementById(element_id);
    if (!element)
        return;
    auto listener = std::make_unique<RmlClickListener>(std::move(on_click));
    listener->Attach(*element);
    m_listeners.push_back(std::move(listener));
}

void AffixEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void AffixEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void AffixEditorLayer::ReloadAffixLibrary()
{
    try
    {
        m_affixes = LoadAffixLibrary(EditorFilepaths::AffixesPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_affixes = AffixLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void AffixEditorLayer::RefreshAffixList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("affix-list");
    if (!list)
        return;

    const std::vector<Affix>& affixes = m_affixes.All();
    if (affixes.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No affixes yet -- click New Affix to create one.</div>");
        return;
    }

    std::string markup;
    for (const Affix& affix : affixes)
    {
        const bool confirming = affix.id_string == m_pending_delete_id;
        const std::string label = affix.name.empty() ? affix.id_string : affix.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < affixes.size(); ++i)
    {
        const std::string id = affixes[i].id_string;
        if (Rml::Element* edit_button = rows[i]->QuerySelector(".edit"))
        {
            auto listener = std::make_unique<RmlClickListener>([this, id] { OpenForEdit(id); });
            listener->Attach(*edit_button);
            m_list_listeners.push_back(std::move(listener));
        }
        if (Rml::Element* delete_button = rows[i]->QuerySelector(".delete"))
        {
            auto listener = std::make_unique<RmlClickListener>([this, id] { RequestDelete(id); });
            listener->Attach(*delete_button);
            m_list_listeners.push_back(std::move(listener));
        }
    }
}

void AffixEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshAffixList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    ReloadAffixLibrary();
    RefreshAffixList();
}

void AffixEditorLayer::OpenForEdit(const std::string& id)
{
    const Affix* found = m_affixes.Find(entt::hashed_string::value(id.c_str()));
    if (!found)
        return;
    m_draft = *found;

    m_draft_id = id;
    m_original_id = id;
    m_is_new = false;
    m_dirty = false;
    m_pending_delete_id.clear();
    m_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

void AffixEditorLayer::BeginNewAffix()
{
    m_draft = Affix{};

    m_draft_id.clear();
    m_original_id.clear();
    m_is_new = true;
    m_dirty = true;
    m_pending_delete_id.clear();
    m_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

// -- Edit mode ----------------------------------------------------------------

void AffixEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void AffixEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void AffixEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new affix)"} : m_draft_id;
    if (Rml::Element* title = m_editor->GetElementById("edit-title"))
        title->SetInnerRML(EscapeRml(display_id));

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_form_listeners.push_back(std::move(listener));
    };

    if (Rml::Element* row = m_editor->GetElementById("field-id"))
        keep(fieldwidgets::BuildStringField(*row, "id", m_draft_id,
                                            [this](std::string v)
                                            {
                                                m_draft_id = std::move(v);
                                                MarkDirty();
                                                if (Rml::Element* title = m_editor->GetElementById("edit-title"))
                                                    title->SetInnerRML(
                                                        EscapeRml(m_draft_id.empty() ? std::string{"(new affix)"}
                                                                                     : m_draft_id));
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-name"))
        keep(fieldwidgets::BuildStringField(*row, "name", m_draft.name,
                                            [this](std::string v)
                                            {
                                                m_draft.name = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-kind"))
        keep(fieldwidgets::BuildEnumField(*row, "kind", EnumOptions<AffixKind>(), EnumToString(m_draft.kind),
                                          [this](std::string v)
                                          {
                                              m_draft.kind = EnumFromString(v, AffixKind::Prefix);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-stat"))
        keep(fieldwidgets::BuildEnumField(*row, "stat", EnumOptions<AffixStat>(), EnumToString(m_draft.stat),
                                          [this](std::string v)
                                          {
                                              m_draft.stat = EnumFromString(v, AffixStat::Atp);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-amount"))
        keep(fieldwidgets::BuildIntField(*row, "amount", m_draft.amount,
                                         [this](int v)
                                         {
                                             m_draft.amount = v;
                                             MarkDirty();
                                         }));

    RefreshDirtyDisplay();
}

void AffixEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Affix id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "An affix already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SaveAffix(target, m_draft);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        ReloadAffixLibrary();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("AffixEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

// -- Events -------------------------------------------------------------------

void AffixEditorLayer::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& e)
        {
            if (e.GetKeyCode() != SDLK_ESCAPE)
                return false;
            switch (m_mode)
            {
            case Mode::Edit:
                m_mode = Mode::List;
                ShowScreen(Mode::List);
                RefreshAffixList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
