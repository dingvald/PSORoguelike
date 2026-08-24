#pragma once

#include "Components/RenderableComponent.h"
#include "Engine/ECS/ComponentSchema.h"
#include "Engine/ECS/RaceComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/StatsComponent.h"
#include "Engine/Layer.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "UI/ColorPickerPopup.h"
#include "UI/FieldPickers.h"
#include "UI/FieldWidgets.h"
#include "UI/PreviewCanvas.h"
#include "UI/PreviewWindowChrome.h"
#include "UI/TexturePickerPopup.h"

#include <rapidjson/document.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

// The Prefab Editor: browse/create/delete entity prefabs -- the JSON files
// under App/Assets/Data/Entities/ that PieceEditorLayer's palette stamps into
// piece cells (see Core/Engine/Dungeon/DungeonPiece.h). One form section per
// currently-registered *authorable* component with editor support
// (RenderableComponent, SocketComponent, StatsComponent, RaceComponent) --
// not a bespoke enemy-specific "Entity editor"; a dedicated stat-forms/race-
// picker pass (roadmap M5.2) can still build further UI on these same
// sections later. Position and PrefabIdComponent are never exposed; both are
// non-authorable per their own doc comments (engine-derived-only: spawn
// position, clone-source id).
//
// Unlike PieceEditorLayer/DungeonEditorLayer, a draft is held as a raw
// rapidjson::Document (not a typed C++ struct) and only its known
// "components".{renderable,socket,stats,race} members are rewritten on save
// -- every other member (including a future component this build doesn't
// know about) round-trips untouched. There is no reusable Core "Entity"
// struct the way DungeonPiece/Dungeon exist; a prefab is purely entt::meta-
// driven data.
class PrefabEditorLayer : public Layer
{
public:
    PrefabEditorLayer();
    ~PrefabEditorLayer() override;

    PrefabEditorLayer(const PrefabEditorLayer&) = delete;
    PrefabEditorLayer& operator=(const PrefabEditorLayer&) = delete;
    PrefabEditorLayer(PrefabEditorLayer&&) = delete;
    PrefabEditorLayer& operator=(PrefabEditorLayer&&) = delete;

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

    // -- List mode --
    void RefreshPrefabList();
    void OpenForEdit(const std::string& id);
    void BeginNewPrefab();
    void RequestDelete(const std::string& id);

    // -- Draft load/save (JSON <-> typed fields) --
    void LoadDraftFromDocument(rapidjson::Document document);
    void ApplyDraftToDocument(); // rewrites only components.{renderable,socket,stats,race}
    void SaveDraft();

    // -- Edit mode: form --
    void RefreshEditForm();
    void RefreshAddComponentOptions();
    void RefreshTagRows();
    void MarkDirty();
    void RefreshDirtyDisplay();
    void RefreshErrorDisplay();

    // -- Preview render --
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

    // The registered component set (built once via RegisterComponents), the
    // single source of truth for which component ids are authorable -- both
    // RefreshAddComponentOptions (the editor list) and SaveDraft (JSON
    // validation) read from this, so neither re-declares that set itself.
    EntitySchemaModel m_schema;

    Rml::ElementDocument* m_editor = nullptr;
    Rml::ElementDocument* m_color_picker_document = nullptr;
    Rml::ElementDocument* m_texture_picker_document = nullptr;
    ColorPickerPopup m_color_picker;
    TexturePickerPopup m_texture_picker;
    FieldPickers m_pickers;

    std::vector<std::unique_ptr<RmlClickListener>> m_listeners;      // static toolbar buttons
    std::unique_ptr<RmlEventListener> m_add_component_listener;      // static "Add Component" select
    std::vector<std::unique_ptr<RmlClickListener>> m_list_listeners; // rebuildable prefab-list rows
    std::vector<std::unique_ptr<RmlEventListener>> m_preview_listeners; // #edit-body pan/zoom listeners
    fieldwidgets::Listeners m_form_listeners;
    fieldwidgets::Listeners m_tag_row_listeners;
    fieldwidgets::Listeners m_preview_chrome_listeners; // #preview-window border/zoom/resize chrome

    // Reorder (drag-drop) finalizes here, one frame after the drag gesture
    // itself, rather than synchronously from inside the "dragdrop" handler --
    // see fieldwidgets::WireDragReorder's doc comment for why. Drained once
    // at the top of OnRender.
    std::function<void()> m_pending_action;

    // -- List state --
    std::vector<std::string> m_prefab_ids;
    std::string m_pending_delete_id;
    std::string m_error;

    // -- Edit state: base document (preserves unknown component keys) --
    rapidjson::Document m_draft_document;
    std::string m_draft_id;
    std::string m_original_id;
    bool m_is_new = false;
    bool m_dirty = false;

    // -- Edit state: known/editable components --
    // Presence AND display order of a prefab's components (a subset of
    // {"renderable","socket","stats","race"}) -- rendered as one
    // Inspector-style card per entry, in this order. Populated from
    // components' JSON member order on load (rapidjson preserves insertion
    // order) and rewritten back in this same order on save (see
    // ApplyDraftToDocument), so drag-reorder persists to disk without any new
    // schema.
    std::vector<std::string> m_component_order;
    bool HasComponent(std::string_view key) const;

    RenderableComponent m_renderable;
    std::string m_renderable_texture_name;
    SocketComponent m_socket;
    std::string m_socket_fallback_name;
    StatsComponent m_stats;
    RaceComponent m_race;
    std::string m_race_name;

    // -- Shared render resources (lazy) --
    bool m_renderer_initialized = false;
    std::optional<TextureAtlas> m_tile_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;

    // World units for m_preview_canvas are literal pixels -- the content
    // bounds are the renderable's own native texture size.
    PreviewCanvas m_preview_canvas;

    int m_output_w = 0;
    int m_output_h = 0;
};

} // namespace psr
