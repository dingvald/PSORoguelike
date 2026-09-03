#include "Layers/PieceEditorLayer.h"

#include "Components/RegisterComponents.h"
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
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "piece_editor.rml";
    const std::filesystem::path kVertexShaderPath = "TileSprite.vert.spv";
    const std::filesystem::path kFragmentShaderPath = "TileSprite.frag.spv";

    // Matches .painter-dropdown's RCSS width/max-height exactly, so
    // OpenPainterDropdown's edge-flip/clamp math can use these fixed values
    // instead of reading the just-built element's GetBox() (which may still
    // reflect the pre-layout box in the same call that made it visible).
    // Using max-height rather than the dropdown's actual (possibly shorter)
    // content height only ever makes the flip-up decision more eager, never
    // less -- it can never render off-screen, just occasionally flip a
    // little earlier than strictly necessary for a short palette.
    constexpr float kPainterDropdownWidth = 200.0f;
    constexpr float kPainterDropdownMaxHeight = 280.0f;

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

    // Human-readable, unique-per-orientation label -- doubles as the
    // BuildEnumField option key (see its "select->Add(option, option)"), so
    // no separate id<->transform table is needed.
    std::string TransformLabel(PieceTransform transform)
    {
        std::vector<std::string> parts;
        if (transform.mirrored)
            parts.emplace_back("Mirrored");
        if (transform.rotation_steps != 0)
            parts.push_back("Rotated " + std::to_string(transform.rotation_steps * 90) + " deg");
        if (parts.empty())
            return "Normal";
        std::string label = parts.front();
        for (std::size_t i = 1; i < parts.size(); ++i)
            label += " + " + parts[i];
        return label;
    }

    // Moves `toolbar` to follow the drag's absolute pointer position,
    // clamped to stay fully within its parent's (#edit-body's) box. Mirrors
    // PreviewWindowChrome::HandleResizeDrag, but repositions (left/top)
    // rather than resizes (width/height).
    void HandleToolbarDrag(Rml::Element& toolbar, Rml::Event& event)
    {
        Rml::Element* parent = toolbar.GetParentNode();
        if (!parent)
            return;

        const Rml::Vector2f toolbar_size = toolbar.GetBox().GetSize();
        const Rml::Vector2f parent_offset = parent->GetAbsoluteOffset();
        const Rml::Vector2f parent_size = parent->GetBox().GetSize();

        const float mouse_x = event.GetParameter<float>("mouse_x", 0.0f);
        const float mouse_y = event.GetParameter<float>("mouse_y", 0.0f);

        const float new_left =
            std::clamp(mouse_x - parent_offset.x, 0.0f, std::max(0.0f, parent_size.x - toolbar_size.x));
        const float new_top =
            std::clamp(mouse_y - parent_offset.y, 0.0f, std::max(0.0f, parent_size.y - toolbar_size.y));

        toolbar.SetProperty("left", std::to_string(new_left) + "px");
        toolbar.SetProperty("top", std::to_string(new_top) + "px");
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
    m_card_listeners.clear();
    m_inspector_listeners.clear();
    m_form_listeners.clear();
    m_tag_listeners.clear();
    m_preview_chrome_listeners.clear();
    m_grid_listeners.clear();
    m_toolbar_listeners.clear();
    m_painter_dropdown_listeners.clear();
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

    for (const char* card_id : {"details-card", "tags-card"})
        if (Rml::Element* card = m_editor->GetElementById(card_id))
            for (auto& listener : fieldwidgets::WireCollapseToggle(*card, /*use_chevron=*/true))
                m_card_listeners.push_back(std::move(listener));

    m_editor->Show();
    WireGridInteraction();
    WireFloatingToolbar();
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
    SelectTool(Tool::Selector, 0.0f, 0.0f);
    m_toolbar_default_positioned = false;

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
    SelectTool(Tool::Selector, 0.0f, 0.0f);
    m_toolbar_default_positioned = false;

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
                                                    title->SetInnerRML(EscapeRml(
                                                        m_draft_id.empty() ? std::string{"(new piece)"} : m_draft_id));
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

    // Rebuilding the form (to refresh the preview dropdown's option set)
    // can't happen synchronously from within the very listener callback
    // that's triggering it -- see m_pending_action's doc comment on
    // WireDragReorder for why; it's drained one frame later on OnRender.
    if (Rml::Element* row = m_editor->GetElementById("field-can-rotate"))
        keep(fieldwidgets::BuildBoolField(*row, "can_rotate", m_draft.can_rotate,
                                          [this](bool v)
                                          {
                                              m_draft.can_rotate = v;
                                              MarkDirty();
                                              m_pending_action = [this] { RefreshEditForm(); };
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-can-mirror"))
        keep(fieldwidgets::BuildBoolField(*row, "can_mirror", m_draft.can_mirror,
                                          [this](bool v)
                                          {
                                              m_draft.can_mirror = v;
                                              MarkDirty();
                                              m_pending_action = [this] { RefreshEditForm(); };
                                          }));

    // Preview control: cycles through the orientations the flags above
    // currently allow. Transient/UI-only -- reset if the flag change above
    // dropped the previously selected orientation from the allowed set.
    const std::vector<PieceTransform> allowed_transforms =
        EnumeratePieceTransforms(m_draft.can_rotate, m_draft.can_mirror);
    if (std::find(allowed_transforms.begin(), allowed_transforms.end(), m_preview_transform) ==
        allowed_transforms.end())
        m_preview_transform = PieceTransform{};

    if (Rml::Element* row = m_editor->GetElementById("field-preview"))
    {
        std::vector<std::string> options;
        for (const PieceTransform& transform : allowed_transforms)
            options.push_back(TransformLabel(transform));
        keep(fieldwidgets::BuildEnumField(*row, "preview", options, TransformLabel(m_preview_transform),
                                          [this, allowed_transforms](std::string v)
                                          {
                                              for (const PieceTransform& transform : allowed_transforms)
                                                  if (TransformLabel(transform) == v)
                                                  {
                                                      m_preview_transform = transform;
                                                      break;
                                                  }
                                          }));
    }

    RefreshTagList();
    RefreshInspector();
    RefreshDirtyDisplay();
}

void PieceEditorLayer::WireFloatingToolbar()
{
    if (!m_editor)
        return;
    Rml::Element* toolbar = m_editor->GetElementById("piece-toolbar");
    Rml::Element* handle = m_editor->GetElementById("piece-toolbar-handle");
    Rml::Element* body = m_editor->GetElementById("edit-body");
    if (!toolbar || !handle || !body)
        return;

    // dragstart/drag is RmlUi's real pointer-capture drag -- see
    // PreviewWindowChrome::Build's .preview-resize-handle for the same
    // precedent this mirrors (repositioning left/top here instead of
    // resizing width/height), including why there is deliberately no
    // mousedown listener here: RmlUi only arms dragstart/drag if that
    // mousedown finishes propagating unimpeded, so stopping it would disarm
    // dragging entirely, and it isn't needed -- #grid-panel's paint/erase
    // listener is a DOM sibling of this toolbar, not an ancestor, and
    // #edit-body's painter-dropdown dismiss handler already excludes clicks
    // on the toolbar or its descendants.
    for (const char* event_name : {"dragstart", "drag"})
    {
        auto listener = std::make_unique<RmlEventListener>(event_name,
                                                            [this, toolbar](Rml::Event& event)
                                                            {
                                                                HandleToolbarDrag(*toolbar, event);
                                                                event.StopPropagation();
                                                            });
        listener->Attach(*handle);
        m_toolbar_listeners.push_back(std::move(listener));
    }

    const std::array<std::pair<const char*, Tool>, 3> tool_buttons = {
        {{"tool-selector", Tool::Selector}, {"tool-painter", Tool::Painter}, {"tool-eraser", Tool::Eraser}}};
    for (const auto& [element_id, tool] : tool_buttons)
    {
        if (Rml::Element* button = m_editor->GetElementById(element_id))
        {
            // "mousedown", not "click" -- RmlClickListener discards its
            // event and there's no proof RmlUi's synthesized "click" event
            // carries mouse_x/mouse_y. Don't "simplify" this to
            // RmlClickListener: it would silently break the dropdown's
            // open-at-the-mouse positioning.
            auto listener = std::make_unique<RmlEventListener>(
                "mousedown",
                [this, tool](Rml::Event& event)
                {
                    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
                    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
                    SelectTool(tool, mouse_x, mouse_y);
                    event.StopPropagation();
                });
            listener->Attach(*button);
            m_toolbar_listeners.push_back(std::move(listener));
        }
    }

    // Outside-click dismiss for the painter dropdown: any mousedown within
    // #edit-body that didn't land on the toolbar or the dropdown itself
    // closes it. Runs on the bubble phase after the specific element's own
    // handler (if any), so e.g. a dropdown row's own click (picking a brush)
    // is unaffected -- this only fires for clicks genuinely outside both.
    auto dismiss = std::make_unique<RmlEventListener>(
        "mousedown",
        [this](Rml::Event& event)
        {
            if (!m_painter_dropdown_open || !m_editor)
                return;
            Rml::Element* toolbar_elem = m_editor->GetElementById("piece-toolbar");
            Rml::Element* dropdown_elem = m_editor->GetElementById("painter-dropdown");
            Rml::Element* target = event.GetTargetElement();
            for (Rml::Element* walk = target; walk; walk = walk->GetParentNode())
                if (walk == toolbar_elem || walk == dropdown_elem)
                    return;
            ClosePainterDropdown();
        });
    dismiss->Attach(*body);
    m_toolbar_listeners.push_back(std::move(dismiss));
}

void PieceEditorLayer::SelectTool(Tool tool, float mouse_x, float mouse_y)
{
    const bool was_painter = m_active_tool == Tool::Painter;
    m_active_tool = tool;
    RefreshToolbarSelection();
    if (tool == Tool::Painter)
        OpenPainterDropdown(mouse_x, mouse_y);
    else if (was_painter)
        ClosePainterDropdown();
}

void PieceEditorLayer::RefreshToolbarSelection()
{
    if (!m_editor)
        return;
    const std::array<std::pair<Tool, const char*>, 3> tool_buttons = {
        {{Tool::Selector, "tool-selector"}, {Tool::Painter, "tool-painter"}, {Tool::Eraser, "tool-eraser"}}};
    for (const auto& [tool, element_id] : tool_buttons)
        if (Rml::Element* button = m_editor->GetElementById(element_id))
            button->SetClass("selected", tool == m_active_tool);
}

void PieceEditorLayer::OpenPainterDropdown(float mouse_x, float mouse_y)
{
    if (!m_editor)
        return;
    Rml::Element* body = m_editor->GetElementById("edit-body");
    Rml::Element* dropdown = m_editor->GetElementById("painter-dropdown");
    if (!body || !dropdown)
        return;

    m_painter_dropdown_listeners.clear();
    m_painter_dropdown_icon_elements.clear();

    std::string markup;
    for (const PaletteEntry& entry : m_palette)
        markup += "<div class=\"palette-row\"><div class=\"palette-icon\"></div><span class=\"palette-name\">" +
                  EscapeRml(entry.id_string) + "</span></div>";
    dropdown->SetInnerRML(markup);
    dropdown->SetProperty("display", "block");
    m_painter_dropdown_open = true;

    Rml::ElementList rows;
    dropdown->QuerySelectorAll(rows, ".palette-row");
    for (std::size_t i = 0; i < rows.size() && i < m_palette.size(); ++i)
    {
        const int index = static_cast<int>(i);
        m_painter_dropdown_icon_elements.push_back(rows[i]->QuerySelector(".palette-icon"));
        auto listener = std::make_unique<RmlClickListener>([this, index] { PickBrush(index); });
        listener->Attach(*rows[i]);
        m_painter_dropdown_listeners.push_back(std::move(listener));
    }

    const Rml::Vector2f body_offset = body->GetAbsoluteOffset();
    const Rml::Vector2f body_size = body->GetBox().GetSize();

    float local_x = mouse_x - body_offset.x;
    float local_y = mouse_y - body_offset.y;

    // Flip to bottom-origin (extend upward from the mouse) if the dropdown
    // would otherwise overflow the bottom edge; clamp horizontally either
    // way. Uses the fixed kPainterDropdownWidth/kPainterDropdownMaxHeight
    // constants rather than dropdown->GetBox() -- see their doc comment.
    if (local_y + kPainterDropdownMaxHeight > body_size.y)
        local_y = std::max(0.0f, local_y - kPainterDropdownMaxHeight);
    local_x = std::clamp(local_x, 0.0f, std::max(0.0f, body_size.x - kPainterDropdownWidth));
    local_y = std::clamp(local_y, 0.0f, std::max(0.0f, body_size.y - kPainterDropdownMaxHeight));

    dropdown->SetProperty("left", std::to_string(local_x) + "px");
    dropdown->SetProperty("top", std::to_string(local_y) + "px");
}

void PieceEditorLayer::ClosePainterDropdown()
{
    if (Rml::Element* dropdown = m_editor ? m_editor->GetElementById("painter-dropdown") : nullptr)
        dropdown->SetProperty("display", "none");
    m_painter_dropdown_listeners.clear();
    m_painter_dropdown_icon_elements.clear();
    m_painter_dropdown_open = false;
}

void PieceEditorLayer::PickBrush(int palette_index)
{
    if (palette_index < 0 || palette_index >= static_cast<int>(m_palette.size()))
        return;
    m_active_brush = palette_index;
    ClosePainterDropdown();
}

void PieceEditorLayer::RefreshInspector()
{
    if (!m_editor)
        return;
    m_inspector_listeners.clear();

    Rml::Element* section = m_editor->GetElementById("selected-cell-section");
    Rml::Element* panel = m_editor->GetElementById("cell-inspector");
    if (!section || !panel)
        return;

    if (!m_selected_cell)
    {
        section->SetProperty("display", "none");
        return;
    }
    section->SetProperty("display", "block");

    const Vec2 offset = *m_selected_cell;
    PieceCell* cell = FindCell(offset);
    if (!cell)
    {
        panel->SetInnerRML("<div class=\"list-empty\">Cell (" + std::to_string(offset.x) + ", " +
                           std::to_string(offset.y) + ") is empty.</div>");
        return;
    }

    panel->SetInnerRML(
        "<div class=\"cell-head\">Cell (" + std::to_string(offset.x) + ", " + std::to_string(offset.y) +
        ")</div><div id=\"cell-prefabs\"></div>"
        "<h3>Sockets<span id=\"add-socket\" class=\"btn\">Add Socket</span></h3><div id=\"cell-sockets\"></div>"
        "<h3>Spawns<span id=\"add-spawn\" class=\"btn\">Add Spawn</span></h3><div id=\"cell-spawns\"></div>");

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
        content.push_back("<span class=\"prefab-name\">" + EscapeRml(label) + "</span>");
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

    for (auto& listener : result.listeners)
        m_inspector_listeners.push_back(std::move(listener));

    // -- Sockets on this cell -- piece-authored data, not stamped prefabs
    // (see DungeonPiece.h's PieceSocket), so this is a second, independent
    // row list keyed by DungeonPiece::sockets' own cell_offset match rather
    // than anything in cell->prefabs.
    Rml::Element* sockets_container = m_editor->GetElementById("cell-sockets");
    if (sockets_container)
    {
        std::vector<std::size_t> socket_indices;
        for (std::size_t i = 0; i < m_draft.sockets.size(); ++i)
            if (m_draft.sockets[i].cell_offset == offset)
                socket_indices.push_back(i);

        const std::vector<std::string> socket_content(
            socket_indices.size(), "<div class=\"socket-body\">"
                                   "<div class=\"socket-edge field-row\"></div>"
                                   "<div class=\"socket-fallback field-row\"></div>"
                                   "<h3>Tags<span class=\"btn add-socket-tag\">+</span></h3>"
                                   "<div class=\"socket-tag-list ref-scroll\"></div>"
                                   "<h3>Connects To Tags<span class=\"btn add-socket-connects\">+</span></h3>"
                                   "<div class=\"socket-connects-list ref-scroll\"></div>"
                                   "</div>");

        fieldwidgets::RowList socket_result = fieldwidgets::BuildRowList(
            *sockets_container, socket_content, "<div class=\"list-empty\">No sockets on this cell.</div>",
            [this, offset](std::size_t row_index)
            {
                std::vector<std::size_t> indices;
                for (std::size_t i = 0; i < m_draft.sockets.size(); ++i)
                    if (m_draft.sockets[i].cell_offset == offset)
                        indices.push_back(i);
                if (row_index < indices.size())
                    m_draft.sockets.erase(m_draft.sockets.begin() + static_cast<std::ptrdiff_t>(indices[row_index]));
                MarkDirty();
                RefreshInspector();
            },
            [](std::size_t, std::size_t) {}); // order among a cell's sockets carries no meaning

        for (std::size_t row = 0; row < socket_result.rows.size() && row < socket_indices.size(); ++row)
        {
            const std::size_t socket_index = socket_indices[row];
            Rml::Element& row_element = *socket_result.rows[row];

            if (Rml::Element* edge_row = row_element.QuerySelector(".socket-edge"))
                keep(fieldwidgets::BuildEnumField(*edge_row, "edge", EdgeOptions(),
                                                  EdgeToString(m_draft.sockets[socket_index].edge),
                                                  [this, socket_index](std::string v)
                                                  {
                                                      if (socket_index < m_draft.sockets.size())
                                                          m_draft.sockets[socket_index].edge = EdgeFromString(v);
                                                      MarkDirty();
                                                  }));

            if (Rml::Element* fallback_row = row_element.QuerySelector(".socket-fallback"))
            {
                std::vector<std::pair<std::uint32_t, std::string>> fallback_options = {{0, "-- None --"}};
                for (const PaletteEntry& entry : m_palette)
                    fallback_options.emplace_back(entry.prefab_id, entry.id_string);
                keep(fieldwidgets::BuildIdEnumField(*fallback_row, "fallback_prefab_id", fallback_options,
                                                    m_draft.sockets[socket_index].fallback_prefab_id,
                                                    [this, socket_index](std::uint32_t id)
                                                    {
                                                        if (socket_index < m_draft.sockets.size())
                                                            m_draft.sockets[socket_index].fallback_prefab_id = id;
                                                        MarkDirty();
                                                    }));
            }

            if (Rml::Element* tags_list = row_element.QuerySelector(".socket-tag-list"))
                RefreshSocketTagRows(*tags_list, socket_index, false);
            if (Rml::Element* connects_list = row_element.QuerySelector(".socket-connects-list"))
                RefreshSocketTagRows(*connects_list, socket_index, true);

            if (Rml::Element* add_tag = row_element.QuerySelector(".add-socket-tag"))
            {
                auto listener = std::make_unique<RmlClickListener>(
                    [this, socket_index]
                    {
                        if (socket_index < m_draft.sockets.size())
                            m_draft.sockets[socket_index].tags.emplace_back();
                        MarkDirty();
                        RefreshInspector();
                    });
                listener->Attach(*add_tag);
                m_inspector_listeners.push_back(std::move(listener));
            }
            if (Rml::Element* add_connects = row_element.QuerySelector(".add-socket-connects"))
            {
                auto listener = std::make_unique<RmlClickListener>(
                    [this, socket_index]
                    {
                        if (socket_index < m_draft.sockets.size())
                            m_draft.sockets[socket_index].connects_to_tags.emplace_back();
                        MarkDirty();
                        RefreshInspector();
                    });
                listener->Attach(*add_connects);
                m_inspector_listeners.push_back(std::move(listener));
            }
        }
        for (auto& listener : socket_result.listeners)
            m_inspector_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* add_socket = m_editor->GetElementById("add-socket"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this, offset]
            {
                PieceSocket socket;
                socket.cell_offset = offset;
                socket.edge = DefaultExposedEdge(offset);
                m_draft.sockets.push_back(std::move(socket));
                MarkDirty();
                RefreshInspector();
            });
        listener->Attach(*add_socket);
        m_inspector_listeners.push_back(std::move(listener));
    }

    // -- Spawns on this cell -- piece-authored data mirroring sockets above,
    // matched the same way by DungeonPiece::spawns' own cell_offset.
    Rml::Element* spawns_container = m_editor->GetElementById("cell-spawns");
    if (spawns_container)
    {
        std::vector<std::size_t> spawn_indices;
        for (std::size_t i = 0; i < m_draft.spawns.size(); ++i)
            if (m_draft.spawns[i].cell_offset == offset)
                spawn_indices.push_back(i);

        const std::vector<std::string> spawn_content(spawn_indices.size(),
                                                     "<div class=\"spawn-prefab field-row\"></div>"
                                                     "<div class=\"spawn-wave field-row\"></div>");

        fieldwidgets::RowList spawn_result = fieldwidgets::BuildRowList(
            *spawns_container, spawn_content, "<div class=\"list-empty\">No spawns on this cell.</div>",
            [this, offset](std::size_t row_index)
            {
                std::vector<std::size_t> indices;
                for (std::size_t i = 0; i < m_draft.spawns.size(); ++i)
                    if (m_draft.spawns[i].cell_offset == offset)
                        indices.push_back(i);
                if (row_index < indices.size())
                    m_draft.spawns.erase(m_draft.spawns.begin() + static_cast<std::ptrdiff_t>(indices[row_index]));
                MarkDirty();
                RefreshInspector();
            },
            [](std::size_t, std::size_t) {}); // order among a cell's spawns carries no meaning

        for (std::size_t row = 0; row < spawn_result.rows.size() && row < spawn_indices.size(); ++row)
        {
            const std::size_t spawn_index = spawn_indices[row];
            Rml::Element& row_element = *spawn_result.rows[row];

            if (Rml::Element* prefab_row = row_element.QuerySelector(".spawn-prefab"))
            {
                std::vector<std::pair<std::uint32_t, std::string>> prefab_options;
                for (const PaletteEntry& entry : m_palette)
                    prefab_options.emplace_back(entry.prefab_id, entry.id_string);
                keep(fieldwidgets::BuildIdEnumField(*prefab_row, "prefab_id", prefab_options,
                                                    m_draft.spawns[spawn_index].prefab_id,
                                                    [this, spawn_index](std::uint32_t id)
                                                    {
                                                        if (spawn_index < m_draft.spawns.size())
                                                            m_draft.spawns[spawn_index].prefab_id = id;
                                                        MarkDirty();
                                                    }));
            }

            if (Rml::Element* wave_row = row_element.QuerySelector(".spawn-wave"))
                keep(fieldwidgets::BuildIntField(*wave_row, "wave", m_draft.spawns[spawn_index].wave,
                                                 [this, spawn_index](int v)
                                                 {
                                                     if (spawn_index < m_draft.spawns.size())
                                                         m_draft.spawns[spawn_index].wave = v;
                                                     MarkDirty();
                                                 }));
        }
        for (auto& listener : spawn_result.listeners)
            m_inspector_listeners.push_back(std::move(listener));
    }

    if (Rml::Element* add_spawn = m_editor->GetElementById("add-spawn"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this, offset]
            {
                PieceSpawn spawn;
                spawn.cell_offset = offset;
                m_draft.spawns.push_back(std::move(spawn));
                MarkDirty();
                RefreshInspector();
            });
        listener->Attach(*add_spawn);
        m_inspector_listeners.push_back(std::move(listener));
    }
}

void PieceEditorLayer::RefreshTagList()
{
    if (!m_editor)
        return;
    m_tag_listeners.clear();

    Rml::Element* container = m_editor->GetElementById("field-tags");
    if (!container)
        return;

    const std::vector<std::string> content(m_draft.tags.size(), "<div class=\"tag-id field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *container, content, "<div class=\"list-empty\">No tags.</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.tags.size())
                m_draft.tags.erase(m_draft.tags.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshTagList();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.tags, from, to);
                MarkDirty();
                RefreshTagList();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.tags.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".tag-id"))
            for (auto& listener : fieldwidgets::BuildStringField(*row, "tag", m_draft.tags[i],
                                                                 [this, index](std::string v)
                                                                 {
                                                                     if (index < m_draft.tags.size())
                                                                         m_draft.tags[index] = std::move(v);
                                                                     MarkDirty();
                                                                 }))
                m_tag_listeners.push_back(std::move(listener));
    }
    for (auto& listener : result.listeners)
        m_tag_listeners.push_back(std::move(listener));

    if (Rml::Element* add_tag = m_editor->GetElementById("add-tag"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_draft.tags.emplace_back();
                MarkDirty();
                RefreshTagList();
            });
        listener->Attach(*add_tag);
        m_tag_listeners.push_back(std::move(listener));
    }
}

void PieceEditorLayer::RefreshSocketTagRows(Rml::Element& container, std::size_t socket_index, bool connects_to)
{
    if (socket_index >= m_draft.sockets.size())
        return;
    const std::vector<std::string>& tags =
        connects_to ? m_draft.sockets[socket_index].connects_to_tags : m_draft.sockets[socket_index].tags;

    const std::vector<std::string> content(tags.size(), "<div class=\"tag-id field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        container, content, "<div class=\"list-empty\">No tags.</div>",
        [this, socket_index, connects_to](std::size_t index)
        {
            if (socket_index >= m_draft.sockets.size())
                return;
            std::vector<std::string>& t =
                connects_to ? m_draft.sockets[socket_index].connects_to_tags : m_draft.sockets[socket_index].tags;
            if (index < t.size())
                t.erase(t.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshInspector();
        },
        [this, socket_index, connects_to](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, socket_index, connects_to, from, to]
            {
                if (socket_index >= m_draft.sockets.size())
                    return;
                std::vector<std::string>& t =
                    connects_to ? m_draft.sockets[socket_index].connects_to_tags : m_draft.sockets[socket_index].tags;
                fieldwidgets::MoveElement(t, from, to);
                MarkDirty();
                RefreshInspector();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < tags.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".tag-id"))
            for (auto& listener :
                 fieldwidgets::BuildStringField(*row, "tag", tags[i],
                                                [this, socket_index, connects_to, index](std::string v)
                                                {
                                                    if (socket_index >= m_draft.sockets.size())
                                                        return;
                                                    std::vector<std::string>& t =
                                                        connects_to ? m_draft.sockets[socket_index].connects_to_tags
                                                                    : m_draft.sockets[socket_index].tags;
                                                    if (index < t.size())
                                                        t[index] = std::move(v);
                                                    MarkDirty();
                                                }))
                m_inspector_listeners.push_back(std::move(listener));
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

void PieceEditorLayer::EraseCellBroad(Vec2 offset)
{
    EraseCell(offset);
    bool changed = false;
    for (std::size_t i = m_draft.sockets.size(); i-- > 0;)
        if (m_draft.sockets[i].cell_offset == offset)
        {
            m_draft.sockets.erase(m_draft.sockets.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
        }
    for (std::size_t i = m_draft.spawns.size(); i-- > 0;)
        if (m_draft.spawns[i].cell_offset == offset)
        {
            m_draft.spawns.erase(m_draft.spawns.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
        }
    if (changed)
        MarkDirty();
}

void PieceEditorLayer::EraseBrushFromCell(Vec2 offset)
{
    if (m_active_brush < 0 || m_active_brush >= static_cast<int>(m_palette.size()))
        return;
    PieceCell* cell = FindCell(offset);
    if (!cell)
        return;
    const std::uint32_t prefab_id = m_palette[static_cast<std::size_t>(m_active_brush)].prefab_id;
    const std::size_t before = cell->prefabs.size();
    cell->prefabs.erase(std::remove_if(cell->prefabs.begin(), cell->prefabs.end(),
                                       [prefab_id](const PieceCellPrefab& p) { return p.prefab_id == prefab_id; }),
                        cell->prefabs.end());
    if (cell->prefabs.size() == before)
        return;
    if (cell->prefabs.empty())
        EraseCell(offset); // narrow erase; already calls MarkDirty()
    else
        MarkDirty();
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
        return; // no brush picked yet
    const PaletteEntry& entry = m_palette[static_cast<std::size_t>(m_active_brush)];
    PieceCell& cell = CellAt(offset);
    const bool present = std::any_of(cell.prefabs.begin(), cell.prefabs.end(),
                                     [&](const PieceCellPrefab& p) { return p.prefab_id == entry.prefab_id; });
    if (!present)
    {
        PieceCellPrefab prefab;
        prefab.prefab_id = entry.prefab_id;
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
    SDL_FRect content_bounds{0.0f, 0.0f, static_cast<float>(kEditCols) * kBaseCellPx,
                             static_cast<float>(kEditRows) * kBaseCellPx};

    // Previewing a transform re-fits the view to the transformed piece's own
    // bounding box instead of the fixed edit grid -- PreviewCanvas::Update
    // auto-refits whenever content_bounds differs from its previous call, so
    // this alone centers/zooms to the rotated/mirrored shape with no extra
    // camera code. The piece isn't authored around a fixed origin, so a
    // rotate/mirror (both pivot on world (0,0), see ApplyPieceTransform) can
    // walk cells into negative world coordinates -- expected here, unlike
    // CellUnder's paint hit-test which rejects negative cells because the
    // editable canvas itself is anchored at (0,0).
    if (m_preview_transform != PieceTransform{} && !m_draft.cells.empty())
    {
        Vec2 min = ApplyPieceTransform(m_draft.cells.front().offset, m_preview_transform);
        Vec2 max = min;
        for (const PieceCell& cell : m_draft.cells)
        {
            const Vec2 transformed = ApplyPieceTransform(cell.offset, m_preview_transform);
            min.x = std::min(min.x, transformed.x);
            min.y = std::min(min.y, transformed.y);
            max.x = std::max(max.x, transformed.x);
            max.y = std::max(max.y, transformed.y);
        }
        content_bounds = SDL_FRect{static_cast<float>(min.x) * kBaseCellPx, static_cast<float>(min.y) * kBaseCellPx,
                                   static_cast<float>(max.x - min.x + 1) * kBaseCellPx,
                                   static_cast<float>(max.y - min.y + 1) * kBaseCellPx};
    }

    m_preview_canvas.Update(panel_rect, content_bounds);
    return true;
}

SDL_FRect PieceEditorLayer::CellBox(Vec2 cell) const
{
    return m_preview_canvas.WorldToScreen(SDL_FRect{
        static_cast<float>(cell.x) * kBaseCellPx, static_cast<float>(cell.y) * kBaseCellPx, kBaseCellPx, kBaseCellPx});
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

void PieceEditorLayer::PositionToolbarDefault()
{
    if (m_toolbar_default_positioned || !m_editor)
        return;

    Rml::Element* toolbar = m_editor->GetElementById("piece-toolbar");
    Rml::Element* preview_window = m_editor->GetElementById("preview-window");
    Rml::Element* body = m_editor->GetElementById("edit-body");
    if (!toolbar || !preview_window || !body)
        return;

    const Rml::Vector2f toolbar_size = toolbar->GetBox().GetSize();
    const Rml::Vector2f preview_size = preview_window->GetBox().GetSize();
    if (toolbar_size.x <= 0.0f || preview_size.x <= 0.0f)
        return; // #edit-body isn't laid out yet this frame -- retry next frame

    constexpr float kMargin = 12.0f;
    const Rml::Vector2f preview_offset = preview_window->GetAbsoluteOffset();
    const Rml::Vector2f body_offset = body->GetAbsoluteOffset();
    const float left = (preview_offset.x - body_offset.x) + preview_size.x - toolbar_size.x - kMargin;
    const float top = (preview_offset.y - body_offset.y) + kMargin;

    toolbar->SetProperty("left", std::to_string(std::max(0.0f, left)) + "px");
    toolbar->SetProperty("top", std::to_string(std::max(0.0f, top)) + "px");
    m_toolbar_default_positioned = true;
}

void PieceEditorLayer::RenderEditContent(SDL_Renderer& renderer, int output_w, int output_h)
{
    const bool have_grid = UpdatePreviewCanvas();
    m_grid_valid = have_grid;
    RefreshZoomReadout();
    PositionToolbarDefault();

    const bool gpu_ready = m_tile_atlas && m_tile_atlas->IsLoaded() && m_gpu_pipeline && m_gpu_pipeline->IsLoaded();
    const Vec2 atlas_size = gpu_ready ? m_tile_atlas->GetSize() : Vec2{0, 0};
    const bool previewing = m_preview_transform != PieceTransform{};

    // Kept as two separate vertex batches (rather than one combined draw)
    // because only grid_vertices lives inside "#grid-panel" -- painter
    // dropdown icons sit in the floating "#painter-dropdown" instead, and
    // clipping the whole draw to the panel rect (see below) would clip them
    // out entirely rather than just the grid content it's meant to contain.
    std::vector<TileVertex> grid_vertices;
    std::vector<TileVertex> palette_vertices;
    if (gpu_ready && atlas_size.x > 0 && atlas_size.y > 0)
    {
        if (have_grid)
            for (const PieceCell& cell : m_draft.cells)
            {
                // Outside preview, cells stay confined to the fixed editable
                // grid by construction (painting can't place one beyond it),
                // so this bound only ever excludes stale/out-of-range data.
                // While previewing, a transform can legitimately walk cells
                // negative or past kEditCols/kEditRows (see
                // UpdatePreviewCanvas), so the bound doesn't apply.
                if (!previewing && (cell.offset.x < 0 || cell.offset.y < 0 || cell.offset.x >= kEditCols ||
                                    cell.offset.y >= kEditRows))
                    continue;
                const Vec2 draw_offset =
                    previewing ? ApplyPieceTransform(cell.offset, m_preview_transform) : cell.offset;
                const SDL_FRect box = CellBox(draw_offset);
                for (const PieceCellPrefab& prefab : cell.prefabs)
                {
                    const PaletteEntry* entry = PaletteFor(prefab.prefab_id);
                    if (!entry || !entry->has_renderable)
                        continue;
                    const RenderableTile& r = entry->renderable;
                    if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(r.texture_id, r.texture_size.x,
                                                                                   r.texture_size.y, r.uv.x, r.uv.y))
                        AppendSpriteQuad(grid_vertices, ZoomedSizeRect(box, r.texture_size, m_preview_canvas.GetZoom()),
                                         *src, atlas_size, r.color_1, r.color_2, output_w, output_h);
                }
            }

        if (m_painter_dropdown_open)
        {
            if (Rml::Element* dropdown = m_editor->GetElementById("painter-dropdown"))
            {
                const Rml::Vector2f list_offset = dropdown->GetAbsoluteOffset();
                const Rml::Vector2f list_size = dropdown->GetBox().GetSize();
                const float top = list_offset.y;
                const float bottom = list_offset.y + list_size.y;
                for (std::size_t i = 0; i < m_palette.size() && i < m_painter_dropdown_icon_elements.size(); ++i)
                {
                    Rml::Element* icon = m_painter_dropdown_icon_elements[i];
                    if (!icon || !m_palette[i].has_renderable)
                        continue;
                    const Rml::Vector2f pos = icon->GetAbsoluteOffset();
                    const Rml::Vector2f size = icon->GetBox().GetSize();
                    if (pos.y + size.y < top || pos.y > bottom)
                        continue;
                    const RenderableTile& r = m_palette[i].renderable;
                    if (std::optional<SDL_FRect> src = m_tile_atlas->GetSourceRect(
                            r.texture_id, r.texture_size.x, r.texture_size.y, r.uv.x, r.uv.y))
                    {
                        const SDL_FRect box{pos.x, pos.y, size.x, size.y};
                        AppendSpriteQuad(palette_vertices, NativeSizeRect(box, r.texture_size), *src, atlas_size,
                                         r.color_1, r.color_2, output_w, output_h);
                    }
                }
            }
        }
    }

    if (have_grid)
    {
        // Clip the grid-panel content (cell sprites, grid lines, selected-cell
        // highlight) to the panel's on-screen rect so it can't bleed into the
        // side column when the preview window is shrunk (via its resize
        // handle) smaller than the grid's current pan/zoom fit.
        const PreviewCanvasClipScope clip_scope(renderer, m_preview_canvas.PanelRect());

        if (!grid_vertices.empty())
            m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), grid_vertices, output_w, output_h);

        if (previewing)
        {
            // Read-only preview: outline just the piece's own (transformed)
            // footprint instead of the fixed editable grid/selection, which
            // don't apply here (painting/selection are disabled -- see
            // HandleGridMouseDown).
            SDL_SetRenderDrawColor(&renderer, 92, 200, 255, 255);
            for (const PieceCell& cell : m_draft.cells)
            {
                const SDL_FRect box = CellBox(ApplyPieceTransform(cell.offset, m_preview_transform));
                SDL_RenderRect(&renderer, &box);
            }
        }
        else
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

    if (!palette_vertices.empty())
        m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), palette_vertices, output_w, output_h);
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
    Rml::Element* body = m_editor->GetElementById("edit-body");
    Rml::Element* panel = m_editor->GetElementById("grid-panel");
    if (!body || !panel)
        return;

    // Paint-click and wheel-zoom are scoped to #grid-panel itself, not the
    // wider #edit-body, so they can never fire for input inside the
    // side-column (text fields, the cell inspector, the entity palette's
    // scrollbars) -- those are simply a different branch of the tree and the
    // event never reaches these listeners at all. mousemove/mouseup stay on
    // #edit-body so an in-progress paint or pan drag keeps tracking the
    // pointer even if it strays outside the panel's bounds mid-drag.
    auto down =
        std::make_unique<RmlEventListener>("mousedown", [this](Rml::Event& event) { HandleGridMouseDown(event); });
    down->Attach(*panel);
    m_grid_listeners.push_back(std::move(down));

    auto move =
        std::make_unique<RmlEventListener>("mousemove", [this](Rml::Event& event) { HandleGridMouseMove(event); });
    move->Attach(*body);
    m_grid_listeners.push_back(std::move(move));

    auto up = std::make_unique<RmlEventListener>("mouseup", [this](Rml::Event& event) { HandleGridMouseUp(event); });
    up->Attach(*body);
    m_grid_listeners.push_back(std::move(up));

    auto scroll =
        std::make_unique<RmlEventListener>("mousescroll", [this](Rml::Event& event) { HandleGridMouseScroll(event); });
    scroll->Attach(*panel);
    m_grid_listeners.push_back(std::move(scroll));
}

void PieceEditorLayer::CollapseDetailCards()
{
    if (!m_editor)
        return;
    for (const char* card_id : {"details-card", "tags-card"})
    {
        Rml::Element* card = m_editor->GetElementById(card_id);
        if (!card || card->IsClassSet("collapsed"))
            continue;
        card->SetClass("collapsed", true);
        if (Rml::Element* toggle = card->QuerySelector(".collapse-toggle"))
            toggle->SetInnerRML("&gt;");
    }
}

void PieceEditorLayer::HandleGridMouseDown(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));

    // Middle-button drag pans the preview -- handled independently of the
    // cell hit-test below, which is only about left/right-click painting.
    m_preview_canvas.OnMouseDown(mouse_x, mouse_y, button);

    if (m_preview_transform != PieceTransform{})
        return; // previewing a transform: read-only, painting/selection disabled

    const std::optional<Vec2> cell = CellUnder(mouse_x, mouse_y);
    if (!cell)
        return;

    const auto refresh_if_selected = [this, &cell]
    {
        if (m_selected_cell && *m_selected_cell == *cell)
            RefreshInspector();
    };

    switch (m_active_tool)
    {
    case Tool::Selector:
        if (button == 0)
        {
            m_selected_cell = cell;
            CollapseDetailCards();
            RefreshInspector();
        }
        break;
    case Tool::Painter:
        if (button == 0)
        {
            m_painting = true;
            PaintCell(*cell);
            refresh_if_selected();
        }
        else if (button == 1)
        {
            EraseBrushFromCell(*cell);
            refresh_if_selected();
        }
        break;
    case Tool::Eraser:
        if (button == 0)
        {
            m_erasing = true;
            EraseCellBroad(*cell);
            refresh_if_selected();
        }
        break;
    }
}

void PieceEditorLayer::HandleGridMouseMove(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseMove(mouse_x, mouse_y);

    if (!m_painting && !m_erasing)
        return;
    const std::optional<Vec2> cell = CellUnder(mouse_x, mouse_y);
    if (!cell)
        return;

    if (m_painting && m_active_tool == Tool::Painter)
        PaintCell(*cell);
    else if (m_erasing && m_active_tool == Tool::Eraser)
        EraseCellBroad(*cell);
    else
        return;

    if (m_selected_cell && *m_selected_cell == *cell)
        RefreshInspector();
}

void PieceEditorLayer::HandleGridMouseUp(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    m_preview_canvas.OnMouseUp(button);
    if (button == 0)
    {
        m_painting = false;
        m_erasing = false;
    }
}

void PieceEditorLayer::HandleGridMouseScroll(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    const float wheel_delta = event.GetParameter<float>("wheel_delta_y", 0.0f);
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
