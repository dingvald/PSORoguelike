#pragma once

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Layer.h"
#include "Engine/Render/RenderableTile.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "UI/FieldWidgets.h"
#include "UI/PreviewCanvas.h"
#include "UI/PreviewWindowChrome.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

// The Dungeon Editor: browse/create/delete Dungeon definitions (List) and
// author one's piece pool / room-count / loopback / lock config, with an
// embedded live regenerate-and-preview canvas (Edit) -- this is M4.4's
// "generation preview & tuning tool" bullet, folded into the Dungeon
// definition's own editor rather than a separate layer, since the params
// that bullet describes (piece-pool filter, target room count, loop
// tolerance) are exactly the Dungeon schema's own fields (see
// Core/Engine/Dungeon/Dungeon.h). Same List/Edit shell as PieceEditorLayer;
// its "Generate" button runs DungeonStitcher::GenerateDungeon against the
// live PieceLibrary and renders the resulting DungeonLayout.
//
// Shares a name with UnnamedRoguelike's own DungeonEditorLayer, which is for
// a completely different purpose (its layered-noise biome pipeline) -- not
// a collision, just a coincidence of naming across two unrelated projects.
class DungeonEditorLayer : public Layer
{
public:
    DungeonEditorLayer();
    ~DungeonEditorLayer() override;

    DungeonEditorLayer(const DungeonEditorLayer&) = delete;
    DungeonEditorLayer& operator=(const DungeonEditorLayer&) = delete;
    DungeonEditorLayer(DungeonEditorLayer&&) = delete;
    DungeonEditorLayer& operator=(DungeonEditorLayer&&) = delete;

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

    void ShowScreen(Mode mode);
    void InitializeRenderer(SDL_Renderer& renderer);

    // Resolves every authored entity prefab once into a renderable cache, for
    // drawing placed pieces' stamped prefabs (and a dead-end socket's
    // fallback prefab, see RenderPreview) -- mirrors
    // PieceEditorLayer::BuildPalette's single enumeration pass.
    void BuildPrefabCaches();

    // -- List mode --
    void ReloadDungeonLibrary();
    void RefreshDungeonList();
    void OpenForEdit(const std::string& id);
    void BeginNewDungeon();
    void RequestDelete(const std::string& id);

    // -- Edit mode --
    void RefreshEditForm();
    void RefreshPieceRefRows();
    void RefreshLockRows();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();
    void SaveDraft();

    // -- Preview --
    void RegeneratePreview();
    void RerollPreview();

    // -- Render --
    void RenderPreview(SDL_Renderer& renderer, int output_w, int output_h);
    void RefreshZoomReadout();

    // -- RmlUi wiring --
    void LoadDocuments();
    void WireButtonClick(const char* element_id, std::function<void()> on_click);
    void WirePreviewInteraction();
    void HandlePreviewMouseDown(Rml::Event& event);
    void HandlePreviewMouseMove(Rml::Event& event);
    void HandlePreviewMouseUp(Rml::Event& event);
    void HandlePreviewMouseScroll(Rml::Event& event);

    Mode m_mode = Mode::List;

    Rml::ElementDocument* m_editor = nullptr;
    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable dungeon-list rows
    std::vector<std::unique_ptr<RmlEventListener>> m_preview_listeners; // #edit-body pan/zoom listeners
    fieldwidgets::Listeners m_card_listeners; // static Details/Pieces/Locks inspector-card collapse toggles
    fieldwidgets::Listeners m_form_listeners; // id/name/area_tag/room/loopback fields
    fieldwidgets::Listeners m_piece_row_listeners;
    fieldwidgets::Listeners m_lock_row_listeners;
    fieldwidgets::Listeners m_preview_chrome_listeners; // #preview-window border/zoom/resize chrome

    // Reorder (drag-drop) finalizes here, one frame after the drag gesture
    // itself -- see fieldwidgets::WireDragReorder's doc comment for why.
    // Drained once at the top of OnRender.
    std::function<void()> m_pending_action;

    // -- Prefab caches (keyed by prefab id) --
    struct PrefabVisual
    {
        RenderableTile renderable;
        bool has_renderable = false;
    };
    std::unordered_map<std::uint32_t, PrefabVisual> m_renderables;

    // -- List state --
    PieceLibrary m_pieces;
    DungeonLibrary m_dungeons;
    std::string m_pending_delete_id;

    // -- Edit state --
    Dungeon m_draft;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;
    std::string m_error;

    // -- Preview state --
    std::uint64_t m_preview_seed = 1;
    std::optional<DungeonLayout> m_preview;
    std::string m_preview_error;

    // -- Shared render resources (lazy) --
    bool m_renderer_initialized = false;
    std::optional<TextureAtlas> m_tile_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;

    // World units for m_preview_canvas -- 1 generated-layout cell =
    // kBaseCellPx world-pixels at zoom 1.0.
    static constexpr float kBaseCellPx = 48.0f;
    PreviewCanvas m_preview_canvas;

    int m_output_w = 0;
    int m_output_h = 0;
};

} // namespace psr
