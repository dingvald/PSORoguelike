#pragma once

#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Layer.h"
#include "Engine/Render/RenderableTile.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "UI/FieldWidgets.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
class Event;
} // namespace Rml

namespace psr {

class Event;
class RmlClickListener;
class RmlEventListener;

// The Piece Editor: browse/create/delete dungeon pieces (List) and paint
// one's footprint (Edit). A piece is a sparse set of cells, each stamped with
// one or more entity prefabs (see Core/Engine/Dungeon/DungeonPiece.h) --
// painting *is* what defines the footprint, so pieces come out arbitrary/
// non-rectangular for free, never a fixed grid. Pick a prefab from a palette
// of every authored entity and paint it onto cells; a prefab carrying
// SocketComponent is a socket -- its edge (which border direction it faces)
// defaults to whichever neighbour is unpainted at paint time but stays
// editable per-stamp in the cell inspector, per this milestone's "few
// constraints" brief. Modelled closely on UnnamedRoguelike's
// FeatureEditorLayer (same List/Edit shell, same palette-paints-a-grid
// interaction), simplified since a piece has no Z layers or generator
// pipeline the way a Feature does.
class PieceEditorLayer : public Layer
{
public:
    PieceEditorLayer();
    ~PieceEditorLayer() override;

    PieceEditorLayer(const PieceEditorLayer&) = delete;
    PieceEditorLayer& operator=(const PieceEditorLayer&) = delete;
    PieceEditorLayer(PieceEditorLayer&&) = delete;
    PieceEditorLayer& operator=(PieceEditorLayer&&) = delete;

    void OnAttach() override;
    void OnDetach() override;
    void OnRender(SDL_Renderer* renderer) override;
    void OnEvent(Event& event) override;

private:
    enum class Mode
    {
        List,
        Edit
    };

    // One palette entry: an entity prefab resolved to the sprite it should
    // draw with and whether it carries SocketComponent. Built once from the
    // Entities directory (see BuildPalette).
    struct PaletteEntry
    {
        std::string id_string;
        std::uint32_t prefab_id = 0;
        RenderableTile renderable;
        bool has_renderable = false;
        bool is_socket = false;
    };

    void ShowScreen(Mode mode);
    void InitializeRenderer(SDL_Renderer& renderer);
    void BuildPalette();

    // -- List mode --
    void ReloadPieceLibrary();
    void RefreshPieceList();
    void OpenForEdit(const std::string& id);
    void BeginNewPiece();
    void RequestDelete(const std::string& id);

    // -- Edit mode --
    void RefreshEditForm();
    void RefreshPaletteList();
    void RefreshPaletteSelection();
    void RefreshInspector();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();
    void SaveDraft();

    // -- Cell editing --
    PieceCell* FindCell(Vec2 offset);
    PieceCell& CellAt(Vec2 offset); // find-or-create
    void EraseCell(Vec2 offset);
    void PaintCell(Vec2 offset); // apply the active brush (prefab or eraser)
    EdgeDirection DefaultExposedEdge(Vec2 offset) const;
    const PaletteEntry* PaletteFor(std::uint32_t prefab_id) const;

    // -- Grid render + interaction --
    void RenderEditContent(SDL_Renderer& renderer, int output_w, int output_h);
    bool GridLayout(); // caches m_grid_*, returns whether valid this frame
    std::optional<Vec2> CellUnder(float mouse_x, float mouse_y) const;
    void WireGridInteraction();
    void HandleGridMouseDown(Rml::Event& event);
    void HandleGridMouseMove(Rml::Event& event);
    void HandleGridMouseUp(Rml::Event& event);

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;         // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners;    // rebuildable piece-list rows
    std::vector<std::unique_ptr<RmlClickListener>> m_palette_listeners; // rebuildable palette rows
    std::vector<std::unique_ptr<RmlEventListener>> m_grid_listeners;    // #edit-body mouse listeners
    fieldwidgets::Listeners m_form_listeners;                           // id/name/area_tag/category fields
    fieldwidgets::Listeners m_inspector_listeners;
    std::vector<Rml::Element*> m_palette_icon_elements; // aligned with m_palette

    // -- Palette --
    std::vector<PaletteEntry> m_palette;
    int m_active_brush = -1; // -1 = eraser, else index into m_palette

    // -- List state --
    PieceLibrary m_pieces;
    std::string m_pending_delete_id;

    // -- Edit state --
    DungeonPiece m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;
    std::optional<Vec2> m_selected_cell;

    // -- Paint drag --
    bool m_painting = false;
    bool m_erasing = false;

    // -- Shared render resources (lazy) --
    bool m_renderer_initialized = false;
    std::optional<TextureAtlas> m_tile_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;

    // Cached grid layout for hit-testing (recomputed each render from
    // #grid-panel). The edit canvas is a fixed, generous kEditCols x
    // kEditRows click area -- a piece has no "size" field of its own (see
    // DungeonPiece.h); painting simply adds/removes cells within this area,
    // so the footprint that gets saved is exactly whatever was touched.
    static constexpr int kEditCols = 24;
    static constexpr int kEditRows = 24;
    float m_grid_x = 0.0f;
    float m_grid_y = 0.0f;
    float m_grid_cell = 0.0f;

    int m_output_w = 0;
    int m_output_h = 0;
};

} // namespace psr
