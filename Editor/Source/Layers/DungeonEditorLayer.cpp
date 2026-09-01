#include "Layers/DungeonEditorLayer.h"

#include "Components/RegisterComponents.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Layers/EditorMenuLayer.h"
#include "Render/RegistryRenderableLookup.h"
#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"
#include "UI/SpriteQuad.h"

#include <EditorFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <entt/core/hashed_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "dungeon_editor.rml";
    const std::filesystem::path kVertexShaderPath = "TileSprite.vert.spv";
    const std::filesystem::path kFragmentShaderPath = "TileSprite.frag.spv";

    // Turns an entered id ("forest.main") into its file path
    // ("Dungeons/forest/main.json"), mirroring LoadJsonDirectory's reverse
    // rule, same as PieceEditorLayer.cpp's IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::DungeonsPath;
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
} // namespace

DungeonEditorLayer::DungeonEditorLayer() : Layer("DungeonEditorLayer") {}
DungeonEditorLayer::~DungeonEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void DungeonEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: DungeonEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: DungeonEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    BuildPrefabCaches();
    try
    {
        m_pieces = LoadPieceLibrary(EditorFilepaths::PiecesPath);
    }
    catch (const std::exception& error)
    {
        m_pieces = PieceLibrary{};
        m_error = error.what();
    }

    LoadDocuments();
    ReloadDungeonLibrary();
    RefreshDungeonList();
    ShowScreen(Mode::List);
}

void DungeonEditorLayer::OnDetach()
{
    m_lock_row_listeners.clear();
    m_piece_row_listeners.clear();
    m_form_listeners.clear();
    m_preview_chrome_listeners.clear();
    m_preview_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void DungeonEditorLayer::BuildPrefabCaches()
{
    m_renderables.clear();
    try
    {
        Registry registry;
        const EntitySchemaModel schema = RegisterComponents(registry);
        JsonEntityLoader loader{registry.GetMetaContext(), &schema};
        loader.Load(EditorFilepaths::EntitiesPath);
        registry.RegisterPrefabs(loader);

        RegistryRenderableLookup lookup{registry};
        for (const JsonDirectoryEntry& entry : LoadJsonDirectory(EditorFilepaths::EntitiesPath, 1))
        {
            const std::uint32_t prefab_id = entt::hashed_string::value(entry.id.c_str());
            const entt::entity instance = registry.CreateEntity(prefab_id);
            if (instance == entt::null)
                continue;

            PrefabVisual visual;
            if (std::optional<RenderableTile> tile = lookup.GetRenderableTile(instance))
            {
                visual.renderable = *tile;
                visual.has_renderable = true;
            }
            m_renderables.emplace(prefab_id, visual);
        }
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
    }
}

void DungeonEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: DungeonEditorLayer has no editor document");
        return;
    }

    WireButtonClick("new-dungeon", [this] { BeginNewDungeon(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-dungeon", [this] { SaveDraft(); });
    WireButtonClick("generate-preview", [this] { RegeneratePreview(); });
    WireButtonClick("reroll-preview", [this] { RerollPreview(); });
    WireButtonClick("add-piece-ref",
                    [this]
                    {
                        m_draft.pieces.push_back(DungeonPieceRef{});
                        MarkDirty();
                        RefreshPieceRefRows();
                    });
    WireButtonClick("add-lock",
                    [this]
                    {
                        m_draft.locks.push_back(DungeonLockConfig{});
                        MarkDirty();
                        RefreshLockRows();
                    });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshDungeonList();
                    });
    if (Rml::Element* preview_window = m_editor->GetElementById("preview-window"))
        for (auto& listener : previewwindow::Build(*preview_window, m_preview_canvas))
            m_preview_chrome_listeners.push_back(std::move(listener));

    m_editor->Show();
    WirePreviewInteraction();
}

void DungeonEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void DungeonEditorLayer::WirePreviewInteraction()
{
    if (!m_editor)
        return;
    Rml::Element* target = m_editor->GetElementById("edit-body");
    if (!target)
        return;

    auto down = std::make_unique<RmlEventListener>(
        "mousedown", [this](Rml::Event& event) { HandlePreviewMouseDown(event); });
    down->Attach(*target);
    m_preview_listeners.push_back(std::move(down));

    auto move = std::make_unique<RmlEventListener>(
        "mousemove", [this](Rml::Event& event) { HandlePreviewMouseMove(event); });
    move->Attach(*target);
    m_preview_listeners.push_back(std::move(move));

    auto up =
        std::make_unique<RmlEventListener>("mouseup", [this](Rml::Event& event) { HandlePreviewMouseUp(event); });
    up->Attach(*target);
    m_preview_listeners.push_back(std::move(up));

    auto scroll = std::make_unique<RmlEventListener>(
        "mousescroll", [this](Rml::Event& event) { HandlePreviewMouseScroll(event); });
    scroll->Attach(*target);
    m_preview_listeners.push_back(std::move(scroll));
}

void DungeonEditorLayer::HandlePreviewMouseDown(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseDown(mouse_x, mouse_y, button);
}

void DungeonEditorLayer::HandlePreviewMouseMove(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseMove(mouse_x, mouse_y);
}

void DungeonEditorLayer::HandlePreviewMouseUp(Rml::Event& event)
{
    m_preview_canvas.OnMouseUp(event.GetParameter<int>("button", -1));
}

void DungeonEditorLayer::HandlePreviewMouseScroll(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    const float wheel_delta = event.GetParameter<float>("wheel_delta_y", 0.0f);
    m_preview_canvas.OnMouseScroll(mouse_x, mouse_y, wheel_delta);
    event.StopPropagation();
}

void DungeonEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void DungeonEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_preview_error.empty() ? m_error : m_preview_error));
}

// -- List mode ----------------------------------------------------------------

void DungeonEditorLayer::ReloadDungeonLibrary()
{
    try
    {
        m_dungeons = LoadDungeonLibrary(EditorFilepaths::DungeonsPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_dungeons = DungeonLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void DungeonEditorLayer::RefreshDungeonList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("dungeon-list");
    if (!list)
        return;

    const std::vector<Dungeon>& dungeons = m_dungeons.All();
    if (dungeons.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No dungeons yet -- click New Dungeon to create one.</div>");
        return;
    }

    std::string markup;
    for (const Dungeon& dungeon : dungeons)
    {
        const bool confirming = dungeon.id_string == m_pending_delete_id;
        const std::string label = dungeon.name.empty() ? dungeon.id_string : dungeon.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < dungeons.size(); ++i)
    {
        const std::string id = dungeons[i].id_string;
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

void DungeonEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshDungeonList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    ReloadDungeonLibrary();
    RefreshDungeonList();
}

void DungeonEditorLayer::OpenForEdit(const std::string& id)
{
    const Dungeon* found = m_dungeons.Find(entt::hashed_string::value(id.c_str()));
    if (!found)
        return;
    m_draft = *found;

    m_draft_id = id;
    m_original_id = id;
    m_is_new = false;
    m_dirty = false;
    m_pending_delete_id.clear();
    m_error.clear();
    m_preview.reset();
    m_preview_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

void DungeonEditorLayer::BeginNewDungeon()
{
    m_draft = Dungeon{};

    m_draft_id.clear();
    m_original_id.clear();
    m_is_new = true;
    m_dirty = true;
    m_pending_delete_id.clear();
    m_error.clear();
    m_preview.reset();
    m_preview_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

// -- Edit mode ----------------------------------------------------------------

void DungeonEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void DungeonEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void DungeonEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new dungeon)"} : m_draft_id;
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
                                                        EscapeRml(m_draft_id.empty() ? std::string{"(new dungeon)"}
                                                                                     : m_draft_id));
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-name"))
        keep(fieldwidgets::BuildStringField(*row, "name", m_draft.name,
                                            [this](std::string v)
                                            {
                                                m_draft.name = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-area-tag"))
        keep(fieldwidgets::BuildStringField(*row, "area_tag", m_draft.area_tag,
                                            [this](std::string v)
                                            {
                                                m_draft.area_tag = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-room-min"))
        keep(fieldwidgets::BuildIntField(*row, "room_count_min", m_draft.room_count_min,
                                         [this](int v)
                                         {
                                             m_draft.room_count_min = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-room-max"))
        keep(fieldwidgets::BuildIntField(*row, "room_count_max", m_draft.room_count_max,
                                         [this](int v)
                                         {
                                             m_draft.room_count_max = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-loopback-min"))
        keep(fieldwidgets::BuildIntField(*row, "loopback_count_min", m_draft.loopback_count_min,
                                         [this](int v)
                                         {
                                             m_draft.loopback_count_min = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-loopback-max"))
        keep(fieldwidgets::BuildIntField(*row, "loopback_count_max", m_draft.loopback_count_max,
                                         [this](int v)
                                         {
                                             m_draft.loopback_count_max = v;
                                             MarkDirty();
                                         }));

    RefreshPieceRefRows();
    RefreshLockRows();
    RefreshDirtyDisplay();
}

void DungeonEditorLayer::RefreshPieceRefRows()
{
    if (!m_editor)
        return;
    m_piece_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("piece-ref-list");
    if (!list)
        return;

    const std::vector<std::string> content(
        m_draft.pieces.size(),
        "<div class=\"ref-id field-row\"></div><div class=\"ref-weight field-row\"></div><div class=\"ref-max "
        "field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No pieces referenced yet.</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.pieces.size())
                m_draft.pieces.erase(m_draft.pieces.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshPieceRefRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.pieces, from, to);
                MarkDirty();
                RefreshPieceRefRows();
            };
        });

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_piece_row_listeners.push_back(std::move(listener));
    };

    std::vector<std::pair<std::uint32_t, std::string>> piece_options = {{0, "-- Select Piece --"}};
    for (const DungeonPiece& piece : m_pieces.All())
        piece_options.emplace_back(piece.id, piece.name.empty() ? piece.id_string : piece.name);

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.pieces.size(); ++i)
    {
        const std::size_t index = i;
        const DungeonPieceRef& ref = m_draft.pieces[i];

        if (Rml::Element* row = result.rows[i]->QuerySelector(".ref-id"))
            keep(fieldwidgets::BuildIdEnumField(*row, "piece_id", piece_options, ref.piece_id,
                                                [this, index](std::uint32_t id)
                                                {
                                                    if (index < m_draft.pieces.size())
                                                        m_draft.pieces[index].piece_id = id;
                                                    MarkDirty();
                                                }));
        if (Rml::Element* row = result.rows[i]->QuerySelector(".ref-weight"))
            keep(fieldwidgets::BuildFloatField(*row, "weight", ref.weight,
                                               [this, index](float v)
                                               {
                                                   if (index < m_draft.pieces.size())
                                                       m_draft.pieces[index].weight = v;
                                                   MarkDirty();
                                               }));
        if (Rml::Element* row = result.rows[i]->QuerySelector(".ref-max"))
            keep(fieldwidgets::BuildIntField(*row, "max_occurrences", ref.max_occurrences,
                                             [this, index](int v)
                                             {
                                                 if (index < m_draft.pieces.size())
                                                     m_draft.pieces[index].max_occurrences = v;
                                                 MarkDirty();
                                             }));
    }

    for (auto& listener : result.listeners)
        m_piece_row_listeners.push_back(std::move(listener));
}

void DungeonEditorLayer::RefreshLockRows()
{
    if (!m_editor)
        return;
    m_lock_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("lock-list");
    if (!list)
        return;

    const std::vector<std::string> content(
        m_draft.locks.size(), "<div class=\"lock-type field-row\"></div><div class=\"lock-count field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No locks configured.</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.locks.size())
                m_draft.locks.erase(m_draft.locks.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshLockRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.locks, from, to);
                MarkDirty();
                RefreshLockRows();
            };
        });

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_lock_row_listeners.push_back(std::move(listener));
    };

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.locks.size(); ++i)
    {
        const std::size_t index = i;
        const DungeonLockConfig& lock = m_draft.locks[i];

        if (Rml::Element* row = result.rows[i]->QuerySelector(".lock-type"))
            keep(fieldwidgets::BuildStringField(*row, "lock_type", lock.lock_type,
                                                [this, index](std::string v)
                                                {
                                                    if (index < m_draft.locks.size())
                                                        m_draft.locks[index].lock_type = std::move(v);
                                                    MarkDirty();
                                                }));
        if (Rml::Element* row = result.rows[i]->QuerySelector(".lock-count"))
            keep(fieldwidgets::BuildIntField(*row, "count", lock.count,
                                             [this, index](int v)
                                             {
                                                 if (index < m_draft.locks.size())
                                                     m_draft.locks[index].count = v;
                                                 MarkDirty();
                                             }));
    }

    for (auto& listener : result.listeners)
        m_lock_row_listeners.push_back(std::move(listener));
}

void DungeonEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Dungeon id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "A dungeon already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SaveDungeon(target, m_draft);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        ReloadDungeonLibrary();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("DungeonEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

// -- Preview ------------------------------------------------------------------

void DungeonEditorLayer::RegeneratePreview()
{
    m_preview.reset();
    m_preview_error.clear();
    try
    {
        m_preview = GenerateDungeon(m_draft, m_pieces, m_preview_seed);
    }
    catch (const std::exception& error)
    {
        m_preview_error = error.what();
    }
    RefreshErrorDisplay();
}

void DungeonEditorLayer::RerollPreview()
{
    std::random_device seed_source;
    m_preview_seed = (static_cast<std::uint64_t>(seed_source()) << 32) ^ static_cast<std::uint64_t>(seed_source());
    RegeneratePreview();
}

// -- Render -------------------------------------------------------------------

void DungeonEditorLayer::InitializeRenderer(SDL_Renderer& renderer)
{
    if (m_renderer_initialized)
        return;
    m_tile_atlas.emplace(renderer, EditorFilepaths::TexturesPath);
    m_gpu_pipeline.emplace(renderer, EditorFilepaths::ShadersPath / kVertexShaderPath,
                           EditorFilepaths::ShadersPath / kFragmentShaderPath);
    m_renderer_initialized = true;
}

void DungeonEditorLayer::RenderPreview(SDL_Renderer& renderer, int output_w, int output_h)
{
    if (!m_editor)
        return;
    Rml::Element* panel = m_editor->GetElementById("grid-panel");
    if (!panel || !m_preview)
        return;

    const Rml::Vector2f panel_offset = panel->GetAbsoluteOffset();
    const Rml::Vector2f panel_size = panel->GetBox().GetSize();
    if (panel_size.x <= 0.0f || panel_size.y <= 0.0f)
        return;

    // Compute the world-cell bounding box across every placed piece's cells,
    // so an arbitrarily large/irregular dungeon fits the panel -- unlike
    // PieceEditorLayer's fixed kEditCols x kEditRows canvas (a single
    // piece's footprint is always small; a dungeon's generated extent isn't
    // bounded the same way).
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool any_cell = false;
    for (const PlacedPiece& placed : m_preview->pieces)
    {
        const DungeonPiece* piece = m_pieces.Find(placed.piece_id);
        if (!piece)
            continue;
        for (const PieceCell& cell : piece->cells)
        {
            const Vec2 world = placed.world_offset + ApplyPieceTransform(cell.offset, placed.transform);
            if (!any_cell)
            {
                min_x = max_x = world.x;
                min_y = max_y = world.y;
                any_cell = true;
            }
            min_x = std::min(min_x, world.x);
            min_y = std::min(min_y, world.y);
            max_x = std::max(max_x, world.x);
            max_y = std::max(max_y, world.y);
        }
    }
    if (!any_cell)
        return;

    const int cols = max_x - min_x + 1;
    const int rows = max_y - min_y + 1;

    const SDL_FRect panel_rect{panel_offset.x, panel_offset.y, panel_size.x, panel_size.y};
    const SDL_FRect content_bounds{0.0f, 0.0f, static_cast<float>(cols) * kBaseCellPx,
                                    static_cast<float>(rows) * kBaseCellPx};
    m_preview_canvas.Update(panel_rect, content_bounds);
    RefreshZoomReadout();

    // Everything below draws inside the panel -- clip so a preview window
    // shrunk (via its resize handle) smaller than the content's current
    // pan/zoom fit doesn't bleed sprites/grid lines into the side column.
    const PreviewCanvasClipScope clip_scope(renderer, m_preview_canvas.PanelRect());

    const auto cell_box = [&](Vec2 world) -> SDL_FRect
    {
        return m_preview_canvas.WorldToScreen(SDL_FRect{static_cast<float>(world.x - min_x) * kBaseCellPx,
                                                         static_cast<float>(world.y - min_y) * kBaseCellPx,
                                                         kBaseCellPx, kBaseCellPx});
    };

    const bool gpu_ready = m_tile_atlas && m_tile_atlas->IsLoaded() && m_gpu_pipeline && m_gpu_pipeline->IsLoaded();
    const Vec2 atlas_size = gpu_ready ? m_tile_atlas->GetSize() : Vec2{0, 0};

    std::vector<TileVertex> vertices;
    if (gpu_ready && atlas_size.x > 0 && atlas_size.y > 0)
        for (const PlacedPiece& placed : m_preview->pieces)
        {
            const DungeonPiece* piece = m_pieces.Find(placed.piece_id);
            if (!piece)
                continue;
            for (const PieceCell& cell_data : piece->cells)
            {
                const SDL_FRect box =
                    cell_box(placed.world_offset + ApplyPieceTransform(cell_data.offset, placed.transform));
                for (const PieceCellPrefab& prefab : cell_data.prefabs)
                {
                    auto it = m_renderables.find(prefab.prefab_id);
                    if (it == m_renderables.end() || !it->second.has_renderable)
                        continue;
                    const RenderableTile& r = it->second.renderable;
                    if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x,
                                                                                   r.texture_size.y, r.uv.x, r.uv.y))
                        AppendSpriteQuad(vertices, ZoomedSizeRect(box, r.texture_size, m_preview_canvas.GetZoom()),
                                         *src, atlas_size, r.color_1, r.color_2, output_w, output_h);
                }
            }
        }
    if (gpu_ready && atlas_size.x > 0 && atlas_size.y > 0)
        for (const DeadEndSocket& dead_end : m_preview->dead_ends)
        {
            auto it = m_renderables.find(dead_end.fallback_prefab_id);
            if (it == m_renderables.end() || !it->second.has_renderable)
                continue;
            const SDL_FRect box = cell_box(dead_end.world_cell);
            const RenderableTile& r = it->second.renderable;
            if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x,
                                                                           r.texture_size.y, r.uv.x, r.uv.y))
                AppendSpriteQuad(vertices, ZoomedSizeRect(box, r.texture_size, m_preview_canvas.GetZoom()), *src,
                                 atlas_size, r.color_1, r.color_2, output_w, output_h);
        }
    if (!vertices.empty())
        m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), vertices, output_w, output_h);

    // Debug overlay: outline every cell, tint dead-end sockets amber (their
    // fallback prefab's sprite, if any, was already drawn in the sprite pass
    // above -- this just marks the cell as a dead end on top of it), tint the
    // key room for each lock cyan, and outline locked connections red -- so
    // the loopback/dead-end/lock-key structure is visually inspectable while
    // tuning the draft's params.
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_BLEND);
    for (const PlacedPiece& placed : m_preview->pieces)
    {
        const DungeonPiece* piece = m_pieces.Find(placed.piece_id);
        if (!piece)
            continue;
        for (const PieceCell& cell_data : piece->cells)
        {
            const SDL_FRect box =
                cell_box(placed.world_offset + ApplyPieceTransform(cell_data.offset, placed.transform));
            SDL_SetRenderDrawColor(&renderer, 60, 62, 74, 255);
            SDL_RenderRect(&renderer, &box);
        }
    }

    for (const LockAnnotation& lock : m_preview->locks)
    {
        if (lock.key_room_index < m_preview->pieces.size())
        {
            const DungeonPiece* piece = m_pieces.Find(m_preview->pieces[lock.key_room_index].piece_id);
            if (piece)
                for (const PieceCell& cell_data : piece->cells)
                {
                    const PlacedPiece& key_placed = m_preview->pieces[lock.key_room_index];
                    const SDL_FRect box =
                        cell_box(key_placed.world_offset + ApplyPieceTransform(cell_data.offset, key_placed.transform));
                    SDL_SetRenderDrawColor(&renderer, 92, 200, 255, 60);
                    SDL_RenderFillRect(&renderer, &box);
                }
        }
        SDL_SetRenderDrawColor(&renderer, 230, 80, 80, 255);
        SDL_FRect a = cell_box(lock.edge.cell_a);
        SDL_FRect b = cell_box(lock.edge.cell_b);
        SDL_RenderRect(&renderer, &a);
        SDL_RenderRect(&renderer, &b);
    }

    for (const DeadEndSocket& dead_end : m_preview->dead_ends)
    {
        SDL_FRect box = cell_box(dead_end.world_cell);
        SDL_SetRenderDrawColor(&renderer, 230, 160, 60, 90);
        SDL_RenderFillRect(&renderer, &box);
    }
}

void DungeonEditorLayer::RefreshZoomReadout()
{
    if (!m_editor)
        return;
    if (Rml::Element* readout = m_editor->GetElementById("zoom-readout"))
    {
        // Only touch the DOM when the value actually changes: SetInnerRML
        // destroys and recreates the text node every call, and doing that
        // unconditionally every frame never gives the new node a chance to
        // survive a layout pass -- it renders as a permanent zero-size box.
        const std::string text = std::to_string(m_preview_canvas.ZoomPercent()) + "%";
        if (readout->GetInnerRML() != text)
            readout->SetInnerRML(text);
    }
}

void DungeonEditorLayer::OnRender(SDL_Renderer* renderer)
{
    // Drains a reorder requested by fieldwidgets::WireDragReorder, if any --
    // see its doc comment for why this can't run synchronously from the
    // "dragdrop" handler itself.
    if (m_pending_action)
    {
        const std::function<void()> action = std::exchange(m_pending_action, nullptr);
        action();
    }

    if (!renderer)
        return;

    SDL_SetRenderDrawColor(renderer, 20, 21, 26, 255);
    SDL_RenderClear(renderer);

    InitializeRenderer(*renderer);

    int output_w = 0;
    int output_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &output_w, &output_h);
    m_output_w = output_w;
    m_output_h = output_h;

    if (m_mode == Mode::Edit)
        RenderPreview(*renderer, output_w, output_h);
}

// -- Events -------------------------------------------------------------------

void DungeonEditorLayer::OnEvent(Event& event)
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
                RefreshDungeonList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
