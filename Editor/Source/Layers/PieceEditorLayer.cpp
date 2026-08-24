#include "Layers/PieceEditorLayer.h"

#include "Components/RegisterComponents.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
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
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "piece_editor.rml";
    const std::filesystem::path kVertexShaderPath = "TileSprite.vert.spv";
    const std::filesystem::path kFragmentShaderPath = "TileSprite.frag.spv";

    // Turns an entered id ("forest.l_corridor") into its file path
    // ("Pieces/forest/l_corridor.json"), mirroring LoadJsonDirectory's reverse
    // rule ('.' -> path separator, ".json" appended).
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::PiecesPath;
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

    std::vector<std::string> EdgeOptions()
    {
        std::vector<std::string> options;
        for (const auto& [text, value] : EnumNames<EdgeDirection>::kValues)
        {
            (void)value;
            options.push_back(std::string{text});
        }
        return options;
    }

    std::string EdgeToString(EdgeDirection edge)
    {
        for (const auto& [text, value] : EnumNames<EdgeDirection>::kValues)
            if (value == edge)
                return std::string{text};
        return std::string{EnumNames<EdgeDirection>::kValues.front().first};
    }

    EdgeDirection EdgeFromString(const std::string& text)
    {
        for (const auto& [name, value] : EnumNames<EdgeDirection>::kValues)
            if (name == text)
                return value;
        return EdgeDirection::North;
    }

    std::vector<std::string> CategoryOptions()
    {
        std::vector<std::string> options;
        for (const auto& [text, value] : EnumNames<PieceCategory>::kValues)
        {
            (void)value;
            options.push_back(std::string{text});
        }
        return options;
    }

    std::string CategoryToString(PieceCategory category)
    {
        for (const auto& [text, value] : EnumNames<PieceCategory>::kValues)
            if (value == category)
                return std::string{text};
        return std::string{EnumNames<PieceCategory>::kValues.front().first};
    }

    PieceCategory CategoryFromString(const std::string& text)
    {
        for (const auto& [name, value] : EnumNames<PieceCategory>::kValues)
            if (name == text)
                return value;
        return PieceCategory::Room;
    }
} // namespace

PieceEditorLayer::PieceEditorLayer() : Layer("PieceEditorLayer") {}
PieceEditorLayer::~PieceEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void PieceEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: PieceEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: PieceEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    BuildPalette();
    LoadDocuments();
    ReloadPieceLibrary();
    RefreshPieceList();
    ShowScreen(Mode::List);
}

void PieceEditorLayer::OnDetach()
{
    m_inspector_listeners.clear();
    m_form_listeners.clear();
    m_preview_chrome_listeners.clear();
    m_grid_listeners.clear();
    m_palette_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void PieceEditorLayer::BuildPalette()
{
    m_palette.clear();
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
            PaletteEntry palette_entry;
            palette_entry.id_string = entry.id;
            palette_entry.prefab_id = entt::hashed_string::value(entry.id.c_str());

            const entt::entity instance = registry.CreateEntity(palette_entry.prefab_id);
            if (instance != entt::null)
            {
                if (std::optional<RenderableTile> tile = lookup.GetRenderableTile(instance))
                {
                    palette_entry.renderable = *tile;
                    palette_entry.has_renderable = true;
                }
                palette_entry.is_socket = registry.HasComponent<SocketComponent>(instance);
            }
            m_palette.push_back(std::move(palette_entry));
        }
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
    }
}

void PieceEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: PieceEditorLayer has no editor document");
        return;
    }

    WireButtonClick("new-piece", [this] { BeginNewPiece(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-piece", [this] { SaveDraft(); });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshPieceList();
                    });
    if (Rml::Element* preview_window = m_editor->GetElementById("preview-window"))
        for (auto& listener : previewwindow::Build(*preview_window, m_preview_canvas))
            m_preview_chrome_listeners.push_back(std::move(listener));

    m_editor->Show();
    WireGridInteraction();
}

void PieceEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void PieceEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void PieceEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void PieceEditorLayer::ReloadPieceLibrary()
{
    try
    {
        m_pieces = LoadPieceLibrary(EditorFilepaths::PiecesPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_pieces = PieceLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void PieceEditorLayer::RefreshPieceList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("piece-list");
    if (!list)
        return;

    const std::vector<DungeonPiece>& pieces = m_pieces.All();
    if (pieces.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No pieces yet -- click New Piece to create one.</div>");
        return;
    }

    std::string markup;
    for (const DungeonPiece& piece : pieces)
    {
        const bool confirming = piece.id_string == m_pending_delete_id;
        const std::string label = piece.name.empty() ? piece.id_string : piece.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < pieces.size(); ++i)
    {
        const std::string id = pieces[i].id_string;
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

void PieceEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshPieceList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    ReloadPieceLibrary();
    RefreshPieceList();
}

void PieceEditorLayer::OpenForEdit(const std::string& id)
{
    const DungeonPiece* found = m_pieces.Find(entt::hashed_string::value(id.c_str()));
    if (!found)
        return;
    m_draft = *found;

    m_draft_id = id;
    m_original_id = id;
    m_is_new = false;
    m_dirty = false;
    m_pending_delete_id.clear();
    m_selected_cell.reset();
    m_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

void PieceEditorLayer::BeginNewPiece()
{
    m_draft = DungeonPiece{};

    m_draft_id.clear();
    m_original_id.clear();
    m_is_new = true;
    m_dirty = true;
    m_pending_delete_id.clear();
    m_selected_cell.reset();
    m_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

// -- Edit mode ----------------------------------------------------------------

void PieceEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void PieceEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void PieceEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new piece)"} : m_draft_id;
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
                                                        EscapeRml(m_draft_id.empty() ? std::string{"(new piece)"}
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

    if (Rml::Element* row = m_editor->GetElementById("field-category"))
        keep(fieldwidgets::BuildEnumField(*row, "category", CategoryOptions(), CategoryToString(m_draft.category),
                                          [this](std::string v)
                                          {
                                              m_draft.category = CategoryFromString(v);
                                              MarkDirty();
                                          }));

    RefreshPaletteList();
    RefreshInspector();
    RefreshDirtyDisplay();
}

void PieceEditorLayer::RefreshPaletteList()
{
    if (!m_editor)
        return;
    m_palette_listeners.clear();
    m_palette_icon_elements.clear();

    Rml::Element* list = m_editor->GetElementById("palette-list");
    if (!list)
        return;

    std::string markup = "<div class=\"palette-row eraser\"><div class=\"palette-icon\"></div><span "
                         "class=\"palette-name\">(eraser)</span></div>";
    for (const PaletteEntry& entry : m_palette)
        markup += "<div class=\"palette-row\"><div class=\"palette-icon\"></div><span class=\"palette-name\">" +
                  EscapeRml(entry.id_string) + (entry.is_socket ? " (socket)" : "") + "</span></div>";
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".palette-row");
    if (!rows.empty())
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_active_brush = -1;
                RefreshPaletteSelection();
            });
        listener->Attach(*rows[0]);
        m_palette_listeners.push_back(std::move(listener));
    }
    for (std::size_t i = 1; i < rows.size() && (i - 1) < m_palette.size(); ++i)
    {
        const int index = static_cast<int>(i - 1);
        m_palette_icon_elements.push_back(rows[i]->QuerySelector(".palette-icon"));
        auto listener = std::make_unique<RmlClickListener>(
            [this, index]
            {
                m_active_brush = index;
                RefreshPaletteSelection();
            });
        listener->Attach(*rows[i]);
        m_palette_listeners.push_back(std::move(listener));
    }
    RefreshPaletteSelection();
}

void PieceEditorLayer::RefreshPaletteSelection()
{
    if (!m_editor)
        return;
    Rml::Element* list = m_editor->GetElementById("palette-list");
    if (!list)
        return;
    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".palette-row");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const bool selected = m_active_brush < 0 ? (i == 0) : (static_cast<int>(i) == m_active_brush + 1);
        rows[i]->SetClass("selected", selected);
    }
}

void PieceEditorLayer::RefreshInspector()
{
    if (!m_editor)
        return;
    m_inspector_listeners.clear();

    Rml::Element* panel = m_editor->GetElementById("cell-inspector");
    if (!panel)
        return;

    if (!m_selected_cell)
    {
        panel->SetInnerRML("<div class=\"list-empty\">Click a cell to edit it.</div>");
        return;
    }

    const Vec2 offset = *m_selected_cell;
    PieceCell* cell = FindCell(offset);
    if (!cell)
    {
        panel->SetInnerRML("<div class=\"list-empty\">Cell (" + std::to_string(offset.x) + ", " +
                           std::to_string(offset.y) + ") is empty.</div>");
        return;
    }

    panel->SetInnerRML("<div class=\"cell-head\">Cell (" + std::to_string(offset.x) + ", " + std::to_string(offset.y) +
                       ")</div><div id=\"cell-prefabs\"></div>");

    const auto keep = [this](fieldwidgets::Listeners listeners)
    {
        for (auto& listener : listeners)
            m_inspector_listeners.push_back(std::move(listener));
    };

    Rml::Element* prefab_list = m_editor->GetElementById("cell-prefabs");
    if (!prefab_list)
        return;

    std::vector<std::string> content;
    content.reserve(cell->prefabs.size());
    for (const PieceCellPrefab& prefab : cell->prefabs)
    {
        const PaletteEntry* entry = PaletteFor(prefab.prefab_id);
        const std::string label = entry ? entry->id_string : std::string{"(unknown)"};
        std::string row = "<span class=\"prefab-name\">" + EscapeRml(label) + "</span>";
        if (entry && entry->is_socket)
            row += "<div class=\"prefab-edge field-row\"></div>";
        content.push_back(std::move(row));
    }

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *prefab_list, content, "<div class=\"list-empty\">No entities on this cell.</div>",
        [this, offset](std::size_t index)
        {
            PieceCell* c = FindCell(offset);
            if (!c)
                return;
            if (index < c->prefabs.size())
                c->prefabs.erase(c->prefabs.begin() + static_cast<std::ptrdiff_t>(index));
            if (c->prefabs.empty())
                EraseCell(offset);
            MarkDirty();
            RefreshInspector();
        },
        [this, offset](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, offset, from, to]
            {
                PieceCell* c = FindCell(offset);
                if (!c)
                    return;
                fieldwidgets::MoveElement(c->prefabs, from, to);
                MarkDirty();
                RefreshInspector();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < cell->prefabs.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* edge_row = result.rows[i]->QuerySelector(".prefab-edge"))
            keep(fieldwidgets::BuildEnumField(*edge_row, "edge", EdgeOptions(), EdgeToString(cell->prefabs[i].edge),
                                              [this, offset, index](std::string v)
                                              {
                                                  PieceCell* c = FindCell(offset);
                                                  if (!c || index >= c->prefabs.size())
                                                      return;
                                                  c->prefabs[index].edge = EdgeFromString(v);
                                                  MarkDirty();
                                              }));
    }

    for (auto& listener : result.listeners)
        m_inspector_listeners.push_back(std::move(listener));
}

void PieceEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Piece id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "A piece already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SavePiece(target, m_draft);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        ReloadPieceLibrary();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("PieceEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

// -- Cell editing -------------------------------------------------------------

PieceCell* PieceEditorLayer::FindCell(Vec2 offset)
{
    for (PieceCell& cell : m_draft.cells)
        if (cell.offset == offset)
            return &cell;
    return nullptr;
}

PieceCell& PieceEditorLayer::CellAt(Vec2 offset)
{
    if (PieceCell* existing = FindCell(offset))
        return *existing;
    PieceCell cell;
    cell.offset = offset;
    m_draft.cells.push_back(std::move(cell));
    return m_draft.cells.back();
}

void PieceEditorLayer::EraseCell(Vec2 offset)
{
    for (std::size_t i = 0; i < m_draft.cells.size(); ++i)
        if (m_draft.cells[i].offset == offset)
        {
            m_draft.cells.erase(m_draft.cells.begin() + static_cast<std::ptrdiff_t>(i));
            MarkDirty();
            return;
        }
}

EdgeDirection PieceEditorLayer::DefaultExposedEdge(Vec2 offset) const
{
    for (const auto& [name, edge] : EnumNames<EdgeDirection>::kValues)
    {
        (void)name;
        bool neighbor_painted = false;
        const Vec2 neighbor = offset + EdgeDirectionOffset(edge);
        for (const PieceCell& cell : m_draft.cells)
            if (cell.offset == neighbor)
            {
                neighbor_painted = true;
                break;
            }
        if (!neighbor_painted)
            return edge;
    }
    return EdgeDirection::North; // fully interior cell -- arbitrary default, editable after
}

void PieceEditorLayer::PaintCell(Vec2 offset)
{
    if (m_active_brush < 0 || m_active_brush >= static_cast<int>(m_palette.size()))
    {
        EraseCell(offset);
        return;
    }
    const PaletteEntry& entry = m_palette[static_cast<std::size_t>(m_active_brush)];
    PieceCell& cell = CellAt(offset);
    const bool present = std::any_of(cell.prefabs.begin(), cell.prefabs.end(),
                                     [&](const PieceCellPrefab& p) { return p.prefab_id == entry.prefab_id; });
    if (!present)
    {
        PieceCellPrefab prefab;
        prefab.prefab_id = entry.prefab_id;
        if (entry.is_socket)
            prefab.edge = DefaultExposedEdge(offset);
        cell.prefabs.push_back(prefab);
    }
    MarkDirty();
}

const PieceEditorLayer::PaletteEntry* PieceEditorLayer::PaletteFor(std::uint32_t prefab_id) const
{
    for (const PaletteEntry& entry : m_palette)
        if (entry.prefab_id == prefab_id)
            return &entry;
    return nullptr;
}

// -- Grid render + interaction ------------------------------------------------

void PieceEditorLayer::InitializeRenderer(SDL_Renderer& renderer)
{
    if (m_renderer_initialized)
        return;
    m_tile_atlas.emplace(renderer, EditorFilepaths::TexturesPath);
    m_gpu_pipeline.emplace(renderer, EditorFilepaths::ShadersPath / kVertexShaderPath,
                           EditorFilepaths::ShadersPath / kFragmentShaderPath);
    m_renderer_initialized = true;
}

bool PieceEditorLayer::UpdatePreviewCanvas()
{
    if (!m_editor)
        return false;
    Rml::Element* panel = m_editor->GetElementById("grid-panel");
    if (!panel)
        return false;
    const Rml::Vector2f offset = panel->GetAbsoluteOffset();
    const Rml::Vector2f size = panel->GetBox().GetSize();
    if (size.x <= 0.0f || size.y <= 0.0f)
        return false;

    const SDL_FRect panel_rect{offset.x, offset.y, size.x, size.y};
    const SDL_FRect content_bounds{0.0f, 0.0f, static_cast<float>(kEditCols) * kBaseCellPx,
                                    static_cast<float>(kEditRows) * kBaseCellPx};
    m_preview_canvas.Update(panel_rect, content_bounds);
    return true;
}

SDL_FRect PieceEditorLayer::CellBox(Vec2 cell) const
{
    return m_preview_canvas.WorldToScreen(SDL_FRect{static_cast<float>(cell.x) * kBaseCellPx,
                                                     static_cast<float>(cell.y) * kBaseCellPx, kBaseCellPx,
                                                     kBaseCellPx});
}

std::optional<Vec2> PieceEditorLayer::CellUnder(float screen_x, float screen_y) const
{
    if (!m_grid_valid)
        return std::nullopt;
    const SDL_FPoint world = m_preview_canvas.ScreenToWorld(SDL_FPoint{screen_x, screen_y});
    if (world.x < 0.0f || world.y < 0.0f)
        return std::nullopt;
    const int cell_x = static_cast<int>(world.x / kBaseCellPx);
    const int cell_y = static_cast<int>(world.y / kBaseCellPx);
    if (cell_x >= kEditCols || cell_y >= kEditRows)
        return std::nullopt;
    return Vec2{cell_x, cell_y};
}

void PieceEditorLayer::RenderEditContent(SDL_Renderer& renderer, int output_w, int output_h)
{
    const bool have_grid = UpdatePreviewCanvas();
    m_grid_valid = have_grid;
    RefreshZoomReadout();

    const bool gpu_ready = m_tile_atlas && m_tile_atlas->IsLoaded() && m_gpu_pipeline && m_gpu_pipeline->IsLoaded();
    const Vec2 atlas_size = gpu_ready ? m_tile_atlas->GetSize() : Vec2{0, 0};

    std::vector<TileVertex> vertices;
    if (gpu_ready && atlas_size.x > 0 && atlas_size.y > 0)
    {
        if (have_grid)
            for (const PieceCell& cell : m_draft.cells)
            {
                if (cell.offset.x < 0 || cell.offset.y < 0 || cell.offset.x >= kEditCols ||
                    cell.offset.y >= kEditRows)
                    continue;
                const SDL_FRect box = CellBox(cell.offset);
                for (const PieceCellPrefab& prefab : cell.prefabs)
                {
                    const PaletteEntry* entry = PaletteFor(prefab.prefab_id);
                    if (!entry || !entry->has_renderable)
                        continue;
                    const RenderableTile& r = entry->renderable;
                    if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x,
                                                                                   r.texture_size.y, r.uv.x, r.uv.y))
                        AppendSpriteQuad(vertices, NativeSizeRect(box, r.texture_size), *src, atlas_size, r.color_1,
                                         r.color_2, output_w, output_h);
                }
            }

        if (Rml::Element* list = m_editor->GetElementById("palette-list"))
        {
            const Rml::Vector2f list_offset = list->GetAbsoluteOffset();
            const Rml::Vector2f list_size = list->GetBox().GetSize();
            const float top = list_offset.y;
            const float bottom = list_offset.y + list_size.y;
            for (std::size_t i = 0; i < m_palette.size() && i < m_palette_icon_elements.size(); ++i)
            {
                Rml::Element* icon = m_palette_icon_elements[i];
                if (!icon || !m_palette[i].has_renderable)
                    continue;
                const Rml::Vector2f pos = icon->GetAbsoluteOffset();
                const Rml::Vector2f size = icon->GetBox().GetSize();
                if (pos.y + size.y < top || pos.y > bottom)
                    continue;
                const RenderableTile& r = m_palette[i].renderable;
                if (std::optional<SDL_FRect> src =
                        m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x, r.texture_size.y, r.uv.x, r.uv.y))
                {
                    const SDL_FRect box{pos.x, pos.y, size.x, size.y};
                    AppendSpriteQuad(vertices, NativeSizeRect(box, r.texture_size), *src, atlas_size, r.color_1,
                                     r.color_2, output_w, output_h);
                }
            }
        }
    }

    if (!vertices.empty())
        m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), vertices, output_w, output_h);

    if (have_grid)
    {
        SDL_SetRenderDrawColor(&renderer, 70, 70, 84, 255);
        for (int y = 0; y < kEditRows; ++y)
            for (int x = 0; x < kEditCols; ++x)
            {
                const SDL_FRect box = CellBox(Vec2{x, y});
                SDL_RenderRect(&renderer, &box);
            }
        if (m_selected_cell)
        {
            const SDL_FRect box = CellBox(*m_selected_cell);
            SDL_SetRenderDrawColor(&renderer, 92, 200, 255, 255);
            SDL_RenderRect(&renderer, &box);
        }
    }
}

void PieceEditorLayer::OnRender(SDL_Renderer* renderer)
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
        RenderEditContent(*renderer, output_w, output_h);
}

void PieceEditorLayer::WireGridInteraction()
{
    if (!m_editor)
        return;
    Rml::Element* target = m_editor->GetElementById("edit-body");
    if (!target)
        return;

    auto down =
        std::make_unique<RmlEventListener>("mousedown", [this](Rml::Event& event) { HandleGridMouseDown(event); });
    down->Attach(*target);
    m_grid_listeners.push_back(std::move(down));

    auto move =
        std::make_unique<RmlEventListener>("mousemove", [this](Rml::Event& event) { HandleGridMouseMove(event); });
    move->Attach(*target);
    m_grid_listeners.push_back(std::move(move));

    auto up = std::make_unique<RmlEventListener>("mouseup", [this](Rml::Event& event) { HandleGridMouseUp(event); });
    up->Attach(*target);
    m_grid_listeners.push_back(std::move(up));

    auto scroll = std::make_unique<RmlEventListener>(
        "mousescroll", [this](Rml::Event& event) { HandleGridMouseScroll(event); });
    scroll->Attach(*target);
    m_grid_listeners.push_back(std::move(scroll));
}

void PieceEditorLayer::HandleGridMouseDown(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));

    // Middle-button drag pans the preview -- handled independently of the
    // cell hit-test below, which is only about left/right-click painting.
    m_preview_canvas.OnMouseDown(mouse_x, mouse_y, button);

    const std::optional<Vec2> cell = CellUnder(mouse_x, mouse_y);
    if (!cell)
        return;

    if (button == 0)
    {
        m_painting = true;
        m_erasing = m_active_brush < 0;
        m_selected_cell = cell;
        if (m_erasing)
            EraseCell(*cell);
        else
            PaintCell(*cell);
        RefreshInspector();
    }
    else if (button == 1)
    {
        m_selected_cell = cell;
        EraseCell(*cell);
        RefreshInspector();
    }
}

void PieceEditorLayer::HandleGridMouseMove(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseMove(mouse_x, mouse_y);

    if (!m_painting)
        return;
    const std::optional<Vec2> cell = CellUnder(mouse_x, mouse_y);
    if (!cell)
        return;
    if (m_erasing)
        EraseCell(*cell);
    else
        PaintCell(*cell);
}

void PieceEditorLayer::HandleGridMouseUp(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    m_preview_canvas.OnMouseUp(button);
    if (button == 0 && m_painting)
    {
        m_painting = false;
        RefreshInspector();
    }
}

void PieceEditorLayer::HandleGridMouseScroll(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    const float wheel_delta = event.GetParameter<float>("wheel_delta", 0.0f);
    m_preview_canvas.OnMouseScroll(mouse_x, mouse_y, wheel_delta);
    event.StopPropagation();
}

void PieceEditorLayer::RefreshZoomReadout()
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

// -- Events -------------------------------------------------------------------

void PieceEditorLayer::OnEvent(Event& event)
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
                RefreshPieceList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
