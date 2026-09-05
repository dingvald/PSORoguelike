#pragma once

#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Layer.h"
#include "Engine/Render/RenderableTile.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/Render/AnimationClock.h"
#include "UI/FieldWidgets.h"
#include "UI/PreviewCanvas.h"
#include "UI/PreviewWindowChrome.h"

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
// of every authored entity and paint it onto cells. Sockets are separate,
// piece-authored data (DungeonPiece::sockets, not stamped prefabs) added via
// the cell inspector's own "Add Socket" -- its edge (which border direction
// it faces) defaults to whichever neighbour is unpainted at add time but
// stays editable after, per this milestone's "few constraints" brief. Spawns
// (DungeonPiece::spawns) are a third, independent cell-inspector list added
// via "Add Spawn" -- an enemy prefab plus a wave number (see
// Engine/Dungeon/SpawnWaveSystem.h for how wave gating works at runtime).
// Modelled closely on UnnamedRoguelike's
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
    void OnUpdate(float delta_time) override;
    void OnEvent(Event& event) override;

private:
    enum class Mode
    {
        List,
        Edit
    };

    enum class Tool
    {
        Selector,
        Painter,
        Eraser
    };

    // One palette entry: an entity prefab resolved to the sprite it should
    // draw with. Built once from the Entities directory (see BuildPalette).
    struct PaletteEntry
    {
        std::string id_string;
        std::uint32_t prefab_id = 0;
        RenderableTile renderable;
        bool has_renderable = false;
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
    void RefreshInspector();
    // Collapses "details-card"/"tags-card" (see WireCollapseToggle) so the
    // cell inspector has room -- called on cell selection.
    void CollapseDetailCards();
    // One nested tag/connects_to_tags row-list within a socket's row in the
    // inspector -- container is that socket's ".socket-tag-list"/
    // ".socket-connects-list" element; connects_to picks which of the
    // socket's two string-array fields this instance edits.
    void RefreshSocketTagRows(Rml::Element& container, std::size_t socket_index, bool connects_to);
    // The piece-level DungeonPiece::tags row-list ("#field-tags"/"#add-tag")
    // -- distinct from RefreshSocketTagRows, which edits a single socket's
    // tags/connects_to_tags instead. Refreshed from RefreshEditForm and
    // self-refreshed on add/remove/reorder, same pattern as RefreshInspector.
    void RefreshTagList();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();
    void SaveDraft();

    // -- Cell editing --
    PieceCell* FindCell(Vec2 offset);
    PieceCell& CellAt(Vec2 offset); // find-or-create
    void EraseCell(Vec2 offset);    // narrow: just the cell/prefabs entry -- used by inspector row-remove
    void EraseCellBroad(Vec2 offset);     // Eraser tool: EraseCell plus any sockets/spawns at this offset
    void EraseBrushFromCell(Vec2 offset); // Painter tool's right-click: just the active brush's prefab entry
    void PaintCell(Vec2 offset);          // apply the active brush
    EdgeDirection DefaultExposedEdge(Vec2 offset) const;
    const PaletteEntry* PaletteFor(std::uint32_t prefab_id) const;

    // -- Floating toolbar / painter dropdown --
    void WireFloatingToolbar();
    // Places the toolbar at the preview window's top-right corner, once per
    // Edit-mode entry (see m_toolbar_default_positioned) and only once
    // "#preview-window"/"#piece-toolbar" have a real laid-out size -- called
    // every frame from RenderEditContent until it succeeds, same "retry
    // until layout is ready" pattern as UpdatePreviewCanvas. Leaves the
    // toolbar wherever the user last dragged it otherwise.
    void PositionToolbarDefault();
    void SelectTool(Tool tool, float mouse_x, float mouse_y);
    void RefreshToolbarSelection();
    void OpenPainterDropdown(float mouse_x, float mouse_y);
    void ClosePainterDropdown();
    void PickBrush(int palette_index);

    // -- Grid render + interaction --
    void RenderEditContent(SDL_Renderer& renderer, int output_w, int output_h);
    bool UpdatePreviewCanvas();         // returns whether #grid-panel is valid this frame
    SDL_FRect CellBox(Vec2 cell) const; // screen-space box for a grid cell, via m_preview_canvas
    std::optional<Vec2> CellUnder(float screen_x, float screen_y) const;
    void WireGridInteraction();
    void HandleGridMouseDown(Rml::Event& event);
    void HandleGridMouseMove(Rml::Event& event);
    void HandleGridMouseUp(Rml::Event& event);
    void HandleGridMouseScroll(Rml::Event& event);
    void RefreshZoomReadout();

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable piece-list rows
    std::vector<std::unique_ptr<RmlEventListener>> m_grid_listeners; // #edit-body mouse listeners
    fieldwidgets::Listeners m_card_listeners; // static Details/Tags inspector-card collapse toggles
    fieldwidgets::Listeners m_form_listeners; // id/name/area_tag/category fields
    fieldwidgets::Listeners m_inspector_listeners;
    fieldwidgets::Listeners m_tag_listeners;            // #field-tags rows -- self-refreshed, see RefreshTagList
    fieldwidgets::Listeners m_preview_chrome_listeners; // #preview-window border/zoom/resize chrome
    fieldwidgets::Listeners m_toolbar_listeners;        // floating toolbar handle + 3 tool buttons, wired once
    fieldwidgets::Listeners m_painter_dropdown_listeners; // painter dropdown rows -- rebuilt each open
    std::vector<Rml::Element*> m_painter_dropdown_icon_elements; // aligned with m_palette, valid while dropdown open

    // Reorder (drag-drop) finalizes here, one frame after the drag gesture
    // itself -- see fieldwidgets::WireDragReorder's doc comment for why.
    // Drained once at the top of OnRender.
    std::function<void()> m_pending_action;

    // -- Tools / floating toolbar --
    Tool m_active_tool = Tool::Selector;
    bool m_painter_dropdown_open = false;
    bool m_toolbar_default_positioned = false; // see PositionToolbarDefault

    // -- Palette --
    std::vector<PaletteEntry> m_palette;
    int m_active_brush = -1; // -1 = no brush picked yet, else index into m_palette

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

    // -- Orientation preview -- transient, UI-only, never saved to m_draft.
    // Non-Identity disables cell painting/selection (see HandleGridMouseDown)
    // so the grid reads as a read-only "how would this look" view.
    PieceTransform m_preview_transform;

    // -- Paint drag -- m_painting/m_erasing track a continuous left-drag
    // under the Painter/Eraser tool respectively (only one is ever true).
    bool m_painting = false;
    bool m_erasing = false;

    // -- Shared render resources (lazy) --
    bool m_renderer_initialized = false;
    std::optional<TextureAtlas> m_tile_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;

    AnimationClock m_animation_clock;

    // Cached grid layout for hit-testing (recomputed each render from
    // #grid-panel). The edit canvas is a fixed, generous kEditCols x
    // kEditRows click area -- a piece has no "size" field of its own (see
    // DungeonPiece.h); painting simply adds/removes cells within this area,
    // so the footprint that gets saved is exactly whatever was touched.
    static constexpr int kEditCols = 24;
    static constexpr int kEditRows = 24;

    // World units for m_preview_canvas -- 1 cell = kBaseCellPx world-pixels
    // at zoom 1.0. The canvas' own pan/zoom then scales this to screen space.
    static constexpr float kBaseCellPx = 48.0f;
    PreviewCanvas m_preview_canvas;
    bool m_grid_valid = false;

    int m_output_w = 0;
    int m_output_h = 0;
};

} // namespace psr
