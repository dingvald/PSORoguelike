#include "Layers/DropTableEditorLayer.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Items/DropTableLibraryFile.h"
#include "Items/SectionId.h"
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
    constexpr int kEntitySchemaVersion = 1;

    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "drop_table_editor.rml";

    // Mirrors PieceEditorLayer.cpp/AffixEditorLayer.cpp/PhotonArtEditorLayer.cpp's
    // own IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::DropTablesPath;
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

    // No dedicated "item library" exists -- weapons/armor/mods are just
    // entity prefabs (see M8.1) -- so this offers every authored entity
    // prefab as a candidate drop, the same universe PrefabEditorLayer's own
    // list screen browses. Mirrors PrefabEditorLayer.cpp's RefreshPrefabList
    // scan.
    std::vector<std::pair<std::uint32_t, std::string>> LoadPrefabIdOptions()
    {
        std::vector<std::pair<std::uint32_t, std::string>> options = {{0, "-- Select Item --"}};
        for (const JsonDirectoryEntry& entry : LoadJsonDirectory(EditorFilepaths::EntitiesPath, kEntitySchemaVersion))
            options.push_back({entt::hashed_string::value(entry.id.c_str()), entry.id});
        return options;
    }
} // namespace

DropTableEditorLayer::DropTableEditorLayer() : Layer("DropTableEditorLayer") {}
DropTableEditorLayer::~DropTableEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void DropTableEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: DropTableEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: DropTableEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    LoadDocuments();
    ReloadLibrary();
    RefreshList();
    ShowScreen(Mode::List);
}

void DropTableEditorLayer::OnDetach()
{
    m_rare_row_listeners.clear();
    m_common_row_listeners.clear();
    m_guaranteed_row_listeners.clear();
    m_form_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void DropTableEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: DropTableEditorLayer has no editor document");
        return;
    }

    WireButtonClick("new-drop-table", [this] { BeginNew(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-drop-table", [this] { SaveDraft(); });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshList();
                    });

    m_editor->Show();
}

void DropTableEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void DropTableEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void DropTableEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void DropTableEditorLayer::ReloadLibrary()
{
    try
    {
        m_drop_tables = LoadDropTableLibrary(EditorFilepaths::DropTablesPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_drop_tables = DropTableLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void DropTableEditorLayer::RefreshList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("drop-table-list");
    if (!list)
        return;

    const std::vector<DropTable>& drop_tables = m_drop_tables.All();
    if (drop_tables.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No drop tables yet -- click New Drop Table to create one.</div>");
        return;
    }

    std::string markup;
    for (const DropTable& table : drop_tables)
    {
        const bool confirming = table.id_string == m_pending_delete_id;
        const std::string label = table.name.empty() ? table.id_string : table.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < drop_tables.size(); ++i)
    {
        const std::string id = drop_tables[i].id_string;
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

void DropTableEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    ReloadLibrary();
    RefreshList();
}

void DropTableEditorLayer::OpenForEdit(const std::string& id)
{
    const DropTable* found = m_drop_tables.Find(entt::hashed_string::value(id.c_str()));
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

void DropTableEditorLayer::BeginNew()
{
    m_draft = DropTable{};

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

void DropTableEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void DropTableEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void DropTableEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new drop table)"} : m_draft_id;
    if (Rml::Element* title = m_editor->GetElementById("edit-title"))
        title->SetInnerRML(EscapeRml(display_id));

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_form_listeners.push_back(std::move(listener));
    };

    if (Rml::Element* row = m_editor->GetElementById("field-id"))
        keep(fieldwidgets::BuildStringField(
            *row, "id", m_draft_id,
            [this](std::string v)
            {
                m_draft_id = std::move(v);
                MarkDirty();
                if (Rml::Element* title = m_editor->GetElementById("edit-title"))
                    title->SetInnerRML(EscapeRml(m_draft_id.empty() ? std::string{"(new drop table)"} : m_draft_id));
            }));

    if (Rml::Element* row = m_editor->GetElementById("field-name"))
        keep(fieldwidgets::BuildStringField(*row, "name", m_draft.name,
                                            [this](std::string v)
                                            {
                                                m_draft.name = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-rare-chance"))
        keep(fieldwidgets::BuildFloatField(*row, "rare_roll_chance_percent", m_draft.rare_roll_chance_percent,
                                           [this](float v)
                                           {
                                               m_draft.rare_roll_chance_percent = v;
                                               MarkDirty();
                                           }));

    if (Rml::Element* row = m_editor->GetElementById("field-meseta-min"))
        keep(fieldwidgets::BuildIntField(*row, "meseta_min", m_draft.meseta_min,
                                         [this](int v)
                                         {
                                             m_draft.meseta_min = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* row = m_editor->GetElementById("field-meseta-max"))
        keep(fieldwidgets::BuildIntField(*row, "meseta_max", m_draft.meseta_max,
                                         [this](int v)
                                         {
                                             m_draft.meseta_max = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* add_guaranteed = m_editor->GetElementById("add-guaranteed"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_draft.guaranteed_item_ids.push_back(0);
                MarkDirty();
                RefreshGuaranteedRows();
            });
        listener->Attach(*add_guaranteed);
        m_form_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* add_common = m_editor->GetElementById("add-common-entry"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_draft.common_entries.emplace_back();
                MarkDirty();
                if (Rml::Element* list = m_editor->GetElementById("common-entry-list"))
                    RefreshEntryRows(*list, m_draft.common_entries, m_common_row_listeners);
            });
        listener->Attach(*add_common);
        m_form_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* add_rare = m_editor->GetElementById("add-rare-entry"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_draft.rare_entries.emplace_back();
                MarkDirty();
                if (Rml::Element* list = m_editor->GetElementById("rare-entry-list"))
                    RefreshEntryRows(*list, m_draft.rare_entries, m_rare_row_listeners);
            });
        listener->Attach(*add_rare);
        m_form_listeners.push_back(std::move(listener));
    }

    RefreshGuaranteedRows();
    if (Rml::Element* list = m_editor->GetElementById("common-entry-list"))
        RefreshEntryRows(*list, m_draft.common_entries, m_common_row_listeners);
    if (Rml::Element* list = m_editor->GetElementById("rare-entry-list"))
        RefreshEntryRows(*list, m_draft.rare_entries, m_rare_row_listeners);
    RefreshDirtyDisplay();
}

void DropTableEditorLayer::RefreshGuaranteedRows()
{
    if (!m_editor)
        return;
    m_guaranteed_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("guaranteed-list");
    if (!list)
        return;

    const std::vector<std::string> content(m_draft.guaranteed_item_ids.size(),
                                           "<div class=\"guaranteed-item field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No guaranteed drops (boss-only).</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.guaranteed_item_ids.size())
                m_draft.guaranteed_item_ids.erase(m_draft.guaranteed_item_ids.begin() +
                                                  static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshGuaranteedRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.guaranteed_item_ids, from, to);
                MarkDirty();
                RefreshGuaranteedRows();
            };
        });

    const std::vector<std::pair<std::uint32_t, std::string>> options = LoadPrefabIdOptions();
    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.guaranteed_item_ids.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".guaranteed-item"))
            for (auto& listener :
                 fieldwidgets::BuildIdEnumField(*row, "item_prefab_id", options, m_draft.guaranteed_item_ids[i],
                                                [this, index](std::uint32_t id)
                                                {
                                                    if (index < m_draft.guaranteed_item_ids.size())
                                                        m_draft.guaranteed_item_ids[index] = id;
                                                    MarkDirty();
                                                }))
                m_guaranteed_row_listeners.push_back(std::move(listener));
    }
    for (auto& listener : result.listeners)
        m_guaranteed_row_listeners.push_back(std::move(listener));
}

void DropTableEditorLayer::RefreshEntryRows(Rml::Element& list, std::vector<DropTableEntry>& entries,
                                            fieldwidgets::Listeners& row_listeners)
{
    row_listeners.clear();

    std::string section_fields;
    for (const auto& [name, section_id] : EnumNames<SectionId>::kValues)
    {
        (void)section_id;
        section_fields += "<div class=\"section-weight-" + std::string(name) + " field-row\"></div>";
    }

    const std::string entry_html = "<div class=\"drop-entry-id field-row\"></div>"
                                   "<div class=\"drop-entry-weight field-row\"></div>"
                                   "<div class=\"list-item collapsed drop-entry-sections\">"
                                   "<span class=\"collapse-toggle\">&gt;</span>"
                                   "<span class=\"text-accent\">Section ID Overrides</span>"
                                   "<div class=\"list-item-body\">" +
                                   section_fields + "</div></div>";

    const std::vector<std::string> content(entries.size(), entry_html);

    std::vector<DropTableEntry>* entries_ptr = &entries;
    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        list, content, "<div class=\"list-empty\">No entries yet.</div>",
        [this, entries_ptr, &list, &row_listeners](std::size_t index)
        {
            if (index < entries_ptr->size())
                entries_ptr->erase(entries_ptr->begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshEntryRows(list, *entries_ptr, row_listeners);
        },
        [this, entries_ptr, &list, &row_listeners](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, entries_ptr, &list, &row_listeners, from, to]
            {
                fieldwidgets::MoveElement(*entries_ptr, from, to);
                MarkDirty();
                RefreshEntryRows(list, *entries_ptr, row_listeners);
            };
        });

    const std::vector<std::pair<std::uint32_t, std::string>> options = LoadPrefabIdOptions();
    for (std::size_t i = 0; i < result.rows.size() && i < entries_ptr->size(); ++i)
    {
        const std::size_t index = i;

        if (Rml::Element* row = result.rows[i]->QuerySelector(".drop-entry-id"))
            for (auto& listener :
                 fieldwidgets::BuildIdEnumField(*row, "item_prefab_id", options, (*entries_ptr)[i].item_prefab_id,
                                                [entries_ptr, index, this](std::uint32_t id)
                                                {
                                                    if (index < entries_ptr->size())
                                                        (*entries_ptr)[index].item_prefab_id = id;
                                                    MarkDirty();
                                                }))
                row_listeners.push_back(std::move(listener));

        if (Rml::Element* row = result.rows[i]->QuerySelector(".drop-entry-weight"))
            for (auto& listener : fieldwidgets::BuildFloatField(*row, "weight", (*entries_ptr)[i].weight,
                                                                [entries_ptr, index, this](float v)
                                                                {
                                                                    if (index < entries_ptr->size())
                                                                        (*entries_ptr)[index].weight = v;
                                                                    MarkDirty();
                                                                }))
                row_listeners.push_back(std::move(listener));

        if (Rml::Element* sections = result.rows[i]->QuerySelector(".drop-entry-sections"))
        {
            for (auto& listener : fieldwidgets::WireCollapseToggle(*sections))
                row_listeners.push_back(std::move(listener));

            std::size_t section_index = 0;
            for (const auto& [name, section_id] : EnumNames<SectionId>::kValues)
            {
                (void)section_id;
                if (Rml::Element* field = sections->QuerySelector("." + std::string("section-weight-") +
                                                                  std::string(name)))
                {
                    const std::size_t weight_index = section_index;
                    for (auto& listener :
                         fieldwidgets::BuildFloatField(*field, std::string(name),
                                                       (*entries_ptr)[i].section_id_weights[weight_index],
                                                       [entries_ptr, index, weight_index, this](float v)
                                                       {
                                                           if (index < entries_ptr->size())
                                                               (*entries_ptr)[index].section_id_weights[weight_index] =
                                                                   v;
                                                           MarkDirty();
                                                       }))
                        row_listeners.push_back(std::move(listener));
                }
                ++section_index;
            }
        }
    }
    for (auto& listener : result.listeners)
        row_listeners.push_back(std::move(listener));
}

void DropTableEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Drop table id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "A drop table already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SaveDropTable(target, m_draft);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        ReloadLibrary();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("DropTableEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

void DropTableEditorLayer::OnRender(SDL_Renderer* renderer)
{
    (void)renderer;
    // See fieldwidgets::WireDragReorder's doc comment / PhotonArtEditorLayer's
    // own m_pending_action precedent -- deferred a frame past the drag
    // gesture that requested it.
    if (m_pending_action)
    {
        const std::function<void()> action = std::exchange(m_pending_action, nullptr);
        action();
    }
}

// -- Events -------------------------------------------------------------------

void DropTableEditorLayer::OnEvent(Event& event)
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
                RefreshList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
