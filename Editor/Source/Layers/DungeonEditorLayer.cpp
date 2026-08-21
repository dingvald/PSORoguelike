#include "Layers/DungeonEditorLayer.h"

#include "Components/RegisterComponents.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/SocketComponent.h"
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

    // The authored label for a NameId field, recovered via NameIdRegistry
    // (empty if never seen this process, e.g. a fresh/unauthored id).
    std::string LabelFor(std::uint32_t id)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return *label;
        return {};
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
    m_lock_row_button_listeners.clear();
    m_piece_row_button_listeners.clear();
    m_lock_row_listeners.clear();
    m_piece_row_listeners.clear();
    m_form_listeners.clear();
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
    m_sockets.clear();
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

            if (const SocketComponent* socket = registry.TryGetComponent<SocketComponent>(instance))
            {
                SocketInfo info;
                info.tags = socket->tags;
                info.fallback_prefab_id = socket->fallback_prefab_id;
                m_sockets.emplace(prefab_id, std::move(info));
            }
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

    m_editor->Show();
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
    m_piece_row_button_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("piece-ref-list");
    if (!list)
        return;

    std::string markup;
    for (std::size_t i = 0; i < m_draft.pieces.size(); ++i)
        markup += "<div class=\"ref-row\"><div class=\"ref-id field-row\"></div><div class=\"ref-weight "
                  "field-row\"></div><div class=\"ref-max field-row\"></div><span class=\"btn remove\">x</span></div>";
    if (m_draft.pieces.empty())
        markup = "<div class=\"list-empty\">No pieces referenced yet.</div>";
    list->SetInnerRML(markup);

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_piece_row_listeners.push_back(std::move(listener));
    };

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".ref-row");
    for (std::size_t i = 0; i < rows.size() && i < m_draft.pieces.size(); ++i)
    {
        const std::size_t index = i;
        const DungeonPieceRef& ref = m_draft.pieces[i];

        if (Rml::Element* row = rows[i]->QuerySelector(".ref-id"))
            keep(fieldwidgets::BuildNameIdField(*row, "piece_id", ref.piece_id, LabelFor(ref.piece_id),
                                                [this, index](std::uint32_t id, std::string)
                                                {
                                                    if (index < m_draft.pieces.size())
                                                        m_draft.pieces[index].piece_id = id;
                                                    MarkDirty();
                                                }));
        if (Rml::Element* row = rows[i]->QuerySelector(".ref-weight"))
            keep(fieldwidgets::BuildFloatField(*row, "weight", ref.weight,
                                               [this, index](float v)
                                               {
                                                   if (index < m_draft.pieces.size())
                                                       m_draft.pieces[index].weight = v;
                                                   MarkDirty();
                                               }));
        if (Rml::Element* row = rows[i]->QuerySelector(".ref-max"))
            keep(fieldwidgets::BuildIntField(*row, "max_occurrences", ref.max_occurrences,
                                             [this, index](int v)
                                             {
                                                 if (index < m_draft.pieces.size())
                                                     m_draft.pieces[index].max_occurrences = v;
                                                 MarkDirty();
                                             }));

        if (Rml::Element* remove_button = rows[i]->QuerySelector(".remove"))
        {
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]
                {
                    if (index < m_draft.pieces.size())
                        m_draft.pieces.erase(m_draft.pieces.begin() + static_cast<std::ptrdiff_t>(index));
                    MarkDirty();
                    RefreshPieceRefRows();
                });
            listener->Attach(*remove_button);
            m_piece_row_button_listeners.push_back(std::move(listener));
        }
    }
}

void DungeonEditorLayer::RefreshLockRows()
{
    if (!m_editor)
        return;
    m_lock_row_listeners.clear();
    m_lock_row_button_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("lock-list");
    if (!list)
        return;

    std::string markup;
    for (std::size_t i = 0; i < m_draft.locks.size(); ++i)
        markup += "<div class=\"lock-row\"><div class=\"lock-type field-row\"></div><div class=\"lock-count "
                  "field-row\"></div><span class=\"btn remove\">x</span></div>";
    if (m_draft.locks.empty())
        markup = "<div class=\"list-empty\">No locks configured.</div>";
    list->SetInnerRML(markup);

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_lock_row_listeners.push_back(std::move(listener));
    };

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".lock-row");
    for (std::size_t i = 0; i < rows.size() && i < m_draft.locks.size(); ++i)
    {
        const std::size_t index = i;
        const DungeonLockConfig& lock = m_draft.locks[i];

        if (Rml::Element* row = rows[i]->QuerySelector(".lock-type"))
            keep(fieldwidgets::BuildStringField(*row, "lock_type", lock.lock_type,
                                                [this, index](std::string v)
                                                {
                                                    if (index < m_draft.locks.size())
                                                        m_draft.locks[index].lock_type = std::move(v);
                                                    MarkDirty();
                                                }));
        if (Rml::Element* row = rows[i]->QuerySelector(".lock-count"))
            keep(fieldwidgets::BuildIntField(*row, "count", lock.count,
                                             [this, index](int v)
                                             {
                                                 if (index < m_draft.locks.size())
                                                     m_draft.locks[index].count = v;
                                                 MarkDirty();
                                             }));

        if (Rml::Element* remove_button = rows[i]->QuerySelector(".remove"))
        {
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]
                {
                    if (index < m_draft.locks.size())
                        m_draft.locks.erase(m_draft.locks.begin() + static_cast<std::ptrdiff_t>(index));
                    MarkDirty();
                    RefreshLockRows();
                });
            listener->Attach(*remove_button);
            m_lock_row_button_listeners.push_back(std::move(listener));
        }
    }
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
        SocketLookup lookup = [this](std::uint32_t id) -> std::optional<SocketInfo>
        {
            auto it = m_sockets.find(id);
            return it == m_sockets.end() ? std::nullopt : std::make_optional(it->second);
        };
        m_preview = GenerateDungeon(m_draft, m_pieces, lookup, m_preview_seed);
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
            const Vec2 world = placed.world_offset + cell.offset;
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
    float cell = std::min(panel_size.x / static_cast<float>(cols), panel_size.y / static_cast<float>(rows));
    cell = std::clamp(cell, 2.0f, 56.0f);
    const float grid_x = panel_offset.x + (panel_size.x - cell * static_cast<float>(cols)) * 0.5f;
    const float grid_y = panel_offset.y + (panel_size.y - cell * static_cast<float>(rows)) * 0.5f;

    const auto cell_box = [&](Vec2 world) -> SDL_FRect
    {
        return SDL_FRect{grid_x + static_cast<float>(world.x - min_x) * cell,
                         grid_y + static_cast<float>(world.y - min_y) * cell, cell, cell};
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
                const SDL_FRect box = cell_box(placed.world_offset + cell_data.offset);
                for (const PieceCellPrefab& prefab : cell_data.prefabs)
                {
                    auto it = m_renderables.find(prefab.prefab_id);
                    if (it == m_renderables.end() || !it->second.has_renderable)
                        continue;
                    const RenderableTile& r = it->second.renderable;
                    if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x,
                                                                                   r.texture_size.y, r.uv.x, r.uv.y))
                        AppendSpriteQuad(vertices, NativeSizeRect(box, r.texture_size), *src, atlas_size, r.color_1,
                                         r.color_2, output_w, output_h);
                }
            }
        }
    if (!vertices.empty())
        m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), vertices, output_w, output_h);

    // Debug overlay: outline every cell, tint dead-end sockets amber, tint the
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
            const SDL_FRect box = cell_box(placed.world_offset + cell_data.offset);
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
                    const SDL_FRect box =
                        cell_box(m_preview->pieces[lock.key_room_index].world_offset + cell_data.offset);
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

void DungeonEditorLayer::OnRender(SDL_Renderer* renderer)
{
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
