#include "Layers/AreaEditorLayer.h"

#include "Areas/AreaLibraryFile.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Layers/EditorMenuLayer.h"
#include "UI/AssetRenameCascade.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <EditorFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <entt/core/hashed_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "area_editor.rml";
    const std::filesystem::path kInfoPopupDocument = EditorFilepaths::RmlDocumentsPath / "info_popup.rml";
    const std::filesystem::path kTexturePickerDocument = EditorFilepaths::RmlDocumentsPath / "texture_picker.rml";

    // Turns an entered id ("forest") into its file path ("Areas/forest.json"),
    // mirroring LoadJsonDirectory's reverse rule, same as
    // AffixEditorLayer.cpp/PieceEditorLayer.cpp's IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::AreasPath;
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

    // The authored label for a NameId field, recovered via NameIdRegistry
    // (empty if never seen this process), same as PrefabEditorLayer.cpp's/
    // DungeonEditorLayer.cpp's LabelFor.
    std::string LabelFor(std::uint32_t id)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return *label;
        return {};
    }

    // Generic BuildEnumField backing, same shape as AffixEditorLayer.cpp's --
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

AreaEditorLayer::AreaEditorLayer() : Layer("AreaEditorLayer") {}
AreaEditorLayer::~AreaEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void AreaEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: AreaEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: AreaEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    try
    {
        m_dungeons = LoadDungeonLibrary(EditorFilepaths::DungeonsPath);
    }
    catch (const std::exception& error)
    {
        m_dungeons = DungeonLibrary{};
        m_error = error.what();
    }

    LoadDocuments();
    ReloadAreaLibrary();
    RefreshAreaList();
    ShowScreen(Mode::List);
}

void AreaEditorLayer::OnDetach()
{
    m_dungeon_row_listeners.clear();
    m_card_listeners.clear();
    m_form_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    m_info_popup.Unbind();
    if (m_info_popup_document)
    {
        m_info_popup_document->Close();
        m_info_popup_document = nullptr;
    }
    m_texture_picker.Unbind();
    if (m_texture_picker_document)
    {
        m_texture_picker_document->Close();
        m_texture_picker_document = nullptr;
    }
    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void AreaEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
        m_info_popup_document = gui_context->LoadDocument(kInfoPopupDocument.string().c_str());
        m_texture_picker_document = gui_context->LoadDocument(kTexturePickerDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: AreaEditorLayer has no editor document");
        return;
    }
    if (m_info_popup_document)
        m_info_popup.Bind(*m_info_popup_document);
    if (m_texture_picker_document)
        m_texture_picker.Bind(*m_texture_picker_document);
    m_pickers.open_texture_picker = [this](std::uint32_t id, std::function<void(std::uint32_t, std::string)> on_pick)
    { m_texture_picker.Open(EditorFilepaths::TexturesPath, id, std::move(on_pick)); };

    WireButtonClick("new-area", [this] { BeginNewArea(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-area", [this] { SaveDraft(); });
    WireButtonClick("add-dungeon-sequence-entry",
                    [this]
                    {
                        m_draft.dungeon_id_strings.push_back({});
                        MarkDirty();
                        RefreshDungeonSequenceRows();
                    });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshAreaList();
                    });

    if (Rml::Element* card = m_editor->GetElementById("dungeon-sequence-card"))
        for (auto& listener : fieldwidgets::WireCollapseToggle(*card, /*use_chevron=*/true))
            m_card_listeners.push_back(std::move(listener));

    m_editor->Show();
}

void AreaEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void AreaEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void AreaEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void AreaEditorLayer::ReloadAreaLibrary()
{
    try
    {
        m_areas = LoadAreaLibrary(EditorFilepaths::AreasPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_areas = AreaLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void AreaEditorLayer::RefreshAreaList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("area-list");
    if (!list)
        return;

    const std::vector<Area>& areas = m_areas.All();
    if (areas.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No areas yet -- click New Area to create one.</div>");
        return;
    }

    std::string markup;
    for (const Area& area : areas)
    {
        const bool confirming = area.id_string == m_pending_delete_id;
        const std::string label = area.name.empty() ? area.id_string : area.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < areas.size(); ++i)
    {
        const std::string id = areas[i].id_string;
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

void AreaEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshAreaList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    ReloadAreaLibrary();
    RefreshAreaList();
}

void AreaEditorLayer::OpenForEdit(const std::string& id)
{
    const Area* found = m_areas.Find(entt::hashed_string::value(id.c_str()));
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

void AreaEditorLayer::BeginNewArea()
{
    m_draft = Area{};

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

void AreaEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void AreaEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void AreaEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new area)"} : m_draft_id;
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
                                                    title->SetInnerRML(EscapeRml(
                                                        m_draft_id.empty() ? std::string{"(new area)"} : m_draft_id));
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-name"))
        keep(fieldwidgets::BuildStringField(*row, "name", m_draft.name,
                                            [this](std::string v)
                                            {
                                                m_draft.name = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-tag"))
        keep(fieldwidgets::BuildStringField(*row, "tag", m_draft.tag,
                                            [this](std::string v)
                                            {
                                                m_draft.tag = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-race-id"))
        keep(fieldwidgets::BuildNameIdField(*row, "race_id", m_draft.race_id, LabelFor(m_draft.race_id),
                                            [this](std::uint32_t id, std::string name)
                                            {
                                                m_draft.race_id = id;
                                                if (!name.empty())
                                                    NameIdRegistry::Register(id, name);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-hazard"))
        keep(fieldwidgets::BuildEnumField(*row, "hazard", EnumOptions<HazardType>(), EnumToString(m_draft.hazard),
                                          [this](std::string v)
                                          {
                                              m_draft.hazard = EnumFromString(v, HazardType::None);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-floor-texture-id"))
        keep(fieldwidgets::BuildTextureField(
            *row, "floor_texture_id", m_draft.floor_texture_id, LabelFor(m_draft.floor_texture_id),
            [this](std::uint32_t id, std::string name)
            {
                m_draft.floor_texture_id = id;
                if (!name.empty())
                    NameIdRegistry::Register(id, name);
                MarkDirty();
            },
            m_pickers.open_texture_picker));

    if (Rml::Element* row = m_editor->GetElementById("field-wall-texture-id"))
        keep(fieldwidgets::BuildTextureField(
            *row, "wall_texture_id", m_draft.wall_texture_id, LabelFor(m_draft.wall_texture_id),
            [this](std::uint32_t id, std::string name)
            {
                m_draft.wall_texture_id = id;
                if (!name.empty())
                    NameIdRegistry::Register(id, name);
                MarkDirty();
            },
            m_pickers.open_texture_picker));

    if (Rml::Element* row = m_editor->GetElementById("field-accent-texture-id"))
        keep(fieldwidgets::BuildTextureField(
            *row, "accent_texture_id", m_draft.accent_texture_id, LabelFor(m_draft.accent_texture_id),
            [this](std::uint32_t id, std::string name)
            {
                m_draft.accent_texture_id = id;
                if (!name.empty())
                    NameIdRegistry::Register(id, name);
                MarkDirty();
            },
            m_pickers.open_texture_picker));

    if (Rml::Element* row = m_editor->GetElementById("field-unlock-predecessor-tag"))
        keep(fieldwidgets::BuildStringField(*row, "unlock_predecessor_tag", m_draft.unlock_predecessor_tag,
                                            [this](std::string v)
                                            {
                                                m_draft.unlock_predecessor_tag = std::move(v);
                                                MarkDirty();
                                            }));

    RefreshDungeonSequenceRows();
    RefreshDirtyDisplay();
}

void AreaEditorLayer::RefreshDungeonSequenceRows()
{
    if (!m_editor)
        return;
    m_dungeon_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("dungeon-sequence-list");
    if (!list)
        return;

    std::vector<std::pair<std::uint32_t, std::string>> dungeon_options = {{0, "-- Select Dungeon --"}};
    for (const Dungeon& dungeon : m_dungeons.All())
        dungeon_options.emplace_back(dungeon.id, dungeon.name.empty() ? dungeon.id_string : dungeon.name);

    const auto ResolveDungeonIdString = [this](std::uint32_t dungeon_id) -> std::string
    {
        for (const Dungeon& dungeon : m_dungeons.All())
            if (dungeon.id == dungeon_id)
                return dungeon.id_string;
        return {};
    };

    const auto CurrentIdFor = [this](const std::string& id_string) -> std::uint32_t
    {
        for (const Dungeon& dungeon : m_dungeons.All())
            if (dungeon.id_string == id_string)
                return dungeon.id;
        return 0;
    };

    const std::vector<std::string> content(m_draft.dungeon_id_strings.size(),
                                           "<div class=\"sequence-dungeon field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No dungeons in this area's sequence yet.</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.dungeon_id_strings.size())
                m_draft.dungeon_id_strings.erase(m_draft.dungeon_id_strings.begin() +
                                                 static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshDungeonSequenceRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.dungeon_id_strings, from, to);
                MarkDirty();
                RefreshDungeonSequenceRows();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.dungeon_id_strings.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".sequence-dungeon"))
            for (auto& listener : fieldwidgets::BuildIdEnumField(
                     *row, "dungeon_id", dungeon_options, CurrentIdFor(m_draft.dungeon_id_strings[i]),
                     [this, index, ResolveDungeonIdString](std::uint32_t id)
                     {
                         if (index < m_draft.dungeon_id_strings.size())
                             m_draft.dungeon_id_strings[index] = ResolveDungeonIdString(id);
                         MarkDirty();
                     }))
                m_dungeon_row_listeners.push_back(std::move(listener));
    }

    for (auto& listener : result.listeners)
        m_dungeon_row_listeners.push_back(std::move(listener));
}

void AreaEditorLayer::OnRender(SDL_Renderer* renderer)
{
    (void)renderer;
    // See fieldwidgets::WireDragReorder's doc comment -- deferred a frame
    // past the drag gesture that requested it.
    if (m_pending_action)
    {
        const std::function<void()> action = std::exchange(m_pending_action, nullptr);
        action();
    }
}

void AreaEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Area id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "An area already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SaveArea(target, m_draft);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);

            const int updated = UpdateReferencesOnRename(AssetKind::Area, m_original_id, m_draft_id);
            if (updated > 0)
                m_info_popup.Open("Updated " + std::to_string(updated) + " other asset(s) that referenced this asset.");
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        ReloadAreaLibrary();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("AreaEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

// -- Events -------------------------------------------------------------------

void AreaEditorLayer::OnEvent(Event& event)
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
                RefreshAreaList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
