#include "Layers/PrefabEditorLayer.h"

#include "Components/RegisterComponents.h"
#include "Engine/ECS/EntitySchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"
#include "Layers/EditorMenuLayer.h"
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
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace psr {

namespace {
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "prefab_editor.rml";
    const std::filesystem::path kColorPickerDocument = EditorFilepaths::RmlDocumentsPath / "color_picker.rml";
    const std::filesystem::path kTexturePickerDocument = EditorFilepaths::RmlDocumentsPath / "texture_picker.rml";
    const std::filesystem::path kVertexShaderPath = "TileSprite.vert.spv";
    const std::filesystem::path kFragmentShaderPath = "TileSprite.frag.spv";

    // Cross-references JsonEntityLoader.cpp's own private constant of the same
    // value -- this editor writes/reads the same "schema_version": 1 entity
    // file shape that loader consumes.
    constexpr int kEntitySchemaVersion = 1;

    // Turns an entered id ("terrain.floor") into its file path
    // ("Entities/terrain/floor.json"), mirroring LoadJsonDirectory's reverse
    // rule, same as PieceEditorLayer.cpp's/DungeonEditorLayer.cpp's IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::EntitiesPath;
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
    // (empty if never seen this process), same as DungeonEditorLayer.cpp's.
    std::string LabelFor(std::uint32_t id)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return *label;
        return {};
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    Vec2 ReadVec2(const rapidjson::Value& object, const char* key)
    {
        Vec2 out;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return out;
        if (!it->value.IsObject())
            return out;
        if (auto x = it->value.FindMember("x"); x != it->value.MemberEnd() && x->value.IsNumber())
            out.x = static_cast<int>(x->value.GetDouble());
        if (auto y = it->value.FindMember("y"); y != it->value.MemberEnd() && y->value.IsNumber())
            out.y = static_cast<int>(y->value.GetDouble());
        return out;
    }

    rapidjson::Value WriteVec2(Vec2 v, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("x", v.x, allocator);
        object.AddMember("y", v.y, allocator);
        return object;
    }

    // A name-id field: a prefab string hashed to an entt id, or a raw numeric
    // id. Mirrors PieceLibraryFile.cpp's ReadNameId -- the source string is
    // captured into NameIdRegistry here, the only place it's still alive.
    std::uint32_t ReadNameId(const rapidjson::Value& object, const char* key, std::uint32_t fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (it->value.IsString())
        {
            const std::uint32_t hash = entt::hashed_string::value(it->value.GetString());
            NameIdRegistry::Register(hash, it->value.GetString());
            return hash;
        }
        if (it->value.IsUint())
            return it->value.GetUint();
        if (it->value.IsInt())
            return static_cast<std::uint32_t>(it->value.GetInt());
        return fallback;
    }

    // Writes a NameId field from its label in NameIdRegistry when known (see
    // PieceLibraryFile.cpp's WriteCellPrefab), else the raw id.
    rapidjson::Value WriteNameId(std::uint32_t id, rapidjson::Document::AllocatorType& allocator)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return StringValue(*label, allocator);
        rapidjson::Value value;
        value.SetUint(id);
        return value;
    }

    std::uint8_t ReadChannel(const rapidjson::Value& object, const char* key, std::uint8_t fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd() || !it->value.IsUint() || it->value.GetUint() > 255)
            return fallback;
        return static_cast<std::uint8_t>(it->value.GetUint());
    }

    // Accepts a hex string or an {r,g,b,a} object, mirroring
    // JsonEntityLoader.cpp's private MakeColor (returning Color directly
    // instead of an entt::meta_any).
    Color ReadColor(const rapidjson::Value& object, const char* key, Color fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (it->value.IsString())
        {
            try
            {
                return Color{std::string_view{it->value.GetString(), it->value.GetStringLength()}};
            }
            catch (const std::invalid_argument&)
            {
                return fallback;
            }
        }
        if (it->value.IsObject())
            return Color{ReadChannel(it->value, "r", 0), ReadChannel(it->value, "g", 0), ReadChannel(it->value, "b", 0),
                        ReadChannel(it->value, "a", 255)};
        return fallback;
    }

    // Always emits hex, same formatting as FieldWidgets.cpp's private
    // ColorToHex.
    rapidjson::Value WriteColor(Color color, rapidjson::Document::AllocatorType& allocator)
    {
        char buffer[10];
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
        return StringValue(buffer, allocator);
    }

    std::vector<std::string> ReadTags(const rapidjson::Value& object, const char* key)
    {
        std::vector<std::string> tags;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd() || !it->value.IsArray())
            return tags;
        for (const auto& entry : it->value.GetArray())
            if (entry.IsString())
                tags.emplace_back(entry.GetString(), entry.GetStringLength());
        return tags;
    }

    rapidjson::Value WriteTags(const std::vector<std::string>& tags, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const std::string& tag : tags)
            array.PushBack(StringValue(tag, allocator), allocator);
        return array;
    }

    RenderableComponent ReadRenderableBody(const rapidjson::Value& body)
    {
        RenderableComponent renderable;
        renderable.texture_id = ReadNameId(body, "texture_id", 0);
        renderable.texture_size = ReadVec2(body, "texture_size");
        renderable.uv = ReadVec2(body, "uv");
        renderable.color_1 = ReadColor(body, "color_1", Color{});
        renderable.color_2 = ReadColor(body, "color_2", Color{});
        auto layer = body.FindMember("render_layer");
        if (layer != body.MemberEnd() && layer->value.IsInt())
            renderable.render_layer = layer->value.GetInt();
        return renderable;
    }

    rapidjson::Value WriteRenderableBody(const RenderableComponent& renderable,
                                         rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("texture_id", WriteNameId(renderable.texture_id, allocator), allocator);
        object.AddMember("texture_size", WriteVec2(renderable.texture_size, allocator), allocator);
        object.AddMember("uv", WriteVec2(renderable.uv, allocator), allocator);
        object.AddMember("color_1", WriteColor(renderable.color_1, allocator), allocator);
        object.AddMember("color_2", WriteColor(renderable.color_2, allocator), allocator);
        object.AddMember("render_layer", renderable.render_layer, allocator);
        return object;
    }

    SocketComponent ReadSocketBody(const rapidjson::Value& body)
    {
        SocketComponent socket;
        socket.tags = ReadTags(body, "tags");
        socket.fallback_prefab_id = ReadNameId(body, "fallback_prefab_id", 0);
        return socket;
    }

    rapidjson::Value WriteSocketBody(const SocketComponent& socket, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("tags", WriteTags(socket.tags, allocator), allocator);
        object.AddMember("fallback_prefab_id", WriteNameId(socket.fallback_prefab_id, allocator), allocator);
        return object;
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd() || !it->value.IsInt())
            return fallback;
        return it->value.GetInt();
    }

    StatsComponent ReadStatsBody(const rapidjson::Value& body)
    {
        StatsComponent stats;
        stats.atp = ReadInt(body, "atp", 0);
        stats.ata = ReadInt(body, "ata", 0);
        stats.mst = ReadInt(body, "mst", 0);
        stats.dfp = ReadInt(body, "dfp", 0);
        stats.evp = ReadInt(body, "evp", 0);
        stats.lck = ReadInt(body, "lck", 0);
        return stats;
    }

    rapidjson::Value WriteStatsBody(const StatsComponent& stats, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("atp", stats.atp, allocator);
        object.AddMember("ata", stats.ata, allocator);
        object.AddMember("mst", stats.mst, allocator);
        object.AddMember("dfp", stats.dfp, allocator);
        object.AddMember("evp", stats.evp, allocator);
        object.AddMember("lck", stats.lck, allocator);
        return object;
    }

    RaceComponent ReadRaceBody(const rapidjson::Value& body)
    {
        RaceComponent race;
        race.race_id = ReadNameId(body, "race_id", 0);
        return race;
    }

    rapidjson::Value WriteRaceBody(const RaceComponent& race, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("race_id", WriteNameId(race.race_id, allocator), allocator);
        return object;
    }

    // Per-kind chrome for an Inspector-style component card: the accent dot
    // colour (reusing existing theme.rcss/palette accents, not a new
    // palette), the card's title, and its body markup -- the same fixed-id
    // field-row placeholders RefreshEditForm has always wired via
    // fieldwidgets::Build*Field, just relocated from the static .rml into
    // each card's dynamically-built body since a card (and thus its ids) now
    // only exists in the DOM while that component is present.
    struct ComponentKind
    {
        const char* key;
        const char* title;
        const char* icon_color;
        const char* body_html;
    };

    constexpr std::array<ComponentKind, 4> kComponentKinds = {
        {{"renderable", "Renderable", "#5cc8ff",
         "<div id=\"field-texture-id\" class=\"field-row\"></div>"
         "<div id=\"field-texture-size\" class=\"field-row\"></div>"
         "<div id=\"field-uv\" class=\"field-row\"></div>"
         "<div id=\"field-color-1\" class=\"field-row\"></div>"
         "<div id=\"field-color-2\" class=\"field-row\"></div>"
         "<div id=\"field-render-layer\" class=\"field-row\"></div>"},
        {"socket", "Socket", "#4caf82",
         "<div id=\"field-fallback-prefab\" class=\"field-row\"></div>"
         "<h3>Tags<span id=\"add-tag\" class=\"btn\">Add Tag</span></h3>"
         "<div id=\"tag-list\" class=\"ref-scroll\"></div>"},
        {"stats", "Stats", "#e8a33d",
         "<div id=\"field-atp\" class=\"field-row\"></div>"
         "<div id=\"field-ata\" class=\"field-row\"></div>"
         "<div id=\"field-mst\" class=\"field-row\"></div>"
         "<div id=\"field-dfp\" class=\"field-row\"></div>"
         "<div id=\"field-evp\" class=\"field-row\"></div>"
         "<div id=\"field-lck\" class=\"field-row\"></div>"},
        {"race", "Race", "#b17ce8", "<div id=\"field-race-id\" class=\"field-row\"></div>"}}};

    const ComponentKind* FindComponentKind(std::string_view key)
    {
        for (const ComponentKind& kind : kComponentKinds)
            if (key == kind.key)
                return &kind;
        return nullptr;
    }
} // namespace

PrefabEditorLayer::PrefabEditorLayer() : Layer("PrefabEditorLayer")
{
    Registry registry;
    m_schema = RegisterComponents(registry);
}

PrefabEditorLayer::~PrefabEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void PrefabEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: PrefabEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: PrefabEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    LoadDocuments();
    RefreshPrefabList();
    ShowScreen(Mode::List);
}

void PrefabEditorLayer::OnDetach()
{
    m_tag_row_listeners.clear();
    m_form_listeners.clear();
    m_preview_chrome_listeners.clear();
    m_preview_listeners.clear();
    m_list_listeners.clear();
    m_add_component_listener.reset();
    m_listeners.clear();

    m_color_picker.Unbind();
    m_texture_picker.Unbind();

    if (m_color_picker_document)
    {
        m_color_picker_document->Close();
        m_color_picker_document = nullptr;
    }
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

void PrefabEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
        m_color_picker_document = gui_context->LoadDocument(kColorPickerDocument.string().c_str());
        m_texture_picker_document = gui_context->LoadDocument(kTexturePickerDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: PrefabEditorLayer has no editor document");
        return;
    }

    if (m_color_picker_document)
        m_color_picker.Bind(*m_color_picker_document);
    if (m_texture_picker_document)
        m_texture_picker.Bind(*m_texture_picker_document);

    m_pickers.open_color_picker = [this](Color color, std::function<void(Color)> on_pick)
    { m_color_picker.Open(color, std::move(on_pick)); };
    m_pickers.open_texture_picker = [this](std::uint32_t id, std::function<void(std::uint32_t, std::string)> on_pick)
    { m_texture_picker.Open(EditorFilepaths::TexturesPath, id, std::move(on_pick)); };

    WireButtonClick("new-prefab", [this] { BeginNewPrefab(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-prefab", [this] { SaveDraft(); });
    if (Rml::Element* add_component = m_editor->GetElementById("add-component-select"))
    {
        auto listener = std::make_unique<RmlEventListener>(
            "change",
            [this](Rml::Event&)
            {
                auto* select =
                    rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(m_editor->GetElementById("add-component-select"));
                if (!select)
                    return;
                const std::string key = select->GetValue();
                if (key.empty() || HasComponent(key))
                    return;
                m_component_order.push_back(key);
                MarkDirty();
                RefreshEditForm();
            });
        listener->Attach(*add_component);
        m_add_component_listener = std::move(listener);
    }
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshPrefabList();
                    });
    if (Rml::Element* preview_window = m_editor->GetElementById("preview-window"))
        for (auto& listener : previewwindow::Build(*preview_window, m_preview_canvas))
            m_preview_chrome_listeners.push_back(std::move(listener));

    m_editor->Show();
    WirePreviewInteraction();
}

void PrefabEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void PrefabEditorLayer::WirePreviewInteraction()
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

void PrefabEditorLayer::HandlePreviewMouseDown(Rml::Event& event)
{
    const int button = event.GetParameter<int>("button", -1);
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseDown(mouse_x, mouse_y, button);
}

void PrefabEditorLayer::HandlePreviewMouseMove(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    m_preview_canvas.OnMouseMove(mouse_x, mouse_y);
}

void PrefabEditorLayer::HandlePreviewMouseUp(Rml::Event& event)
{
    m_preview_canvas.OnMouseUp(event.GetParameter<int>("button", -1));
}

void PrefabEditorLayer::HandlePreviewMouseScroll(Rml::Event& event)
{
    const float mouse_x = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouse_y = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
    const float wheel_delta = event.GetParameter<float>("wheel_delta", 0.0f);
    m_preview_canvas.OnMouseScroll(mouse_x, mouse_y, wheel_delta);
    event.StopPropagation();
}

void PrefabEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void PrefabEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void PrefabEditorLayer::RefreshPrefabList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();
    m_prefab_ids.clear();

    try
    {
        for (const JsonDirectoryEntry& entry : LoadJsonDirectory(EditorFilepaths::EntitiesPath, kEntitySchemaVersion))
            m_prefab_ids.push_back(entry.id);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
    }
    RefreshErrorDisplay();

    Rml::Element* list = m_editor->GetElementById("prefab-list");
    if (!list)
        return;

    if (m_prefab_ids.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No prefabs yet -- click New Prefab to create one.</div>");
        return;
    }

    std::string markup;
    for (const std::string& id : m_prefab_ids)
    {
        const bool confirming = id == m_pending_delete_id;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(id) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < m_prefab_ids.size(); ++i)
    {
        const std::string id = m_prefab_ids[i];
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

void PrefabEditorLayer::RequestDelete(const std::string& id)
{
    if (m_pending_delete_id != id)
    {
        m_pending_delete_id = id;
        RefreshPrefabList();
        return;
    }

    m_pending_delete_id.clear();
    std::error_code error_code;
    std::filesystem::remove(IdToPath(id), error_code);
    RefreshPrefabList();
}

void PrefabEditorLayer::OpenForEdit(const std::string& id)
{
    try
    {
        // Set before LoadDraftFromDocument, not after: that call's own
        // RefreshEditForm() renders the title/id field from m_draft_id
        // immediately, so assigning it afterward left both showing stale
        // "(new prefab)" state for the entire first render of an existing
        // prefab (component cards loaded fine regardless, since those come
        // from the document itself, not m_draft_id).
        m_draft_id = id;
        m_original_id = id;
        LoadDraftFromDocument(ReadJsonFile(IdToPath(id), kEntitySchemaVersion));
        m_is_new = false;
        m_dirty = false;
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        RefreshErrorDisplay();
    }
}

void PrefabEditorLayer::BeginNewPrefab()
{
    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", kEntitySchemaVersion, document.GetAllocator());
    document.AddMember("components", rapidjson::Value(rapidjson::kObjectType), document.GetAllocator());

    LoadDraftFromDocument(std::move(document));
    m_draft_id.clear();
    m_original_id.clear();
    m_is_new = true;
    m_dirty = true;
}

void PrefabEditorLayer::LoadDraftFromDocument(rapidjson::Document document)
{
    m_draft_document = std::move(document);
    if (!m_draft_document.IsObject())
        m_draft_document.SetObject();

    if (!m_draft_document.HasMember("components"))
        m_draft_document.AddMember("components", rapidjson::Value(rapidjson::kObjectType),
                                   m_draft_document.GetAllocator());
    const rapidjson::Value& components = m_draft_document["components"];

    // Display/save order follows the JSON's own member order (rapidjson
    // preserves insertion order) -- whatever order the file was authored in,
    // or previously saved in, is what the component cards render in.
    m_component_order.clear();
    for (auto it = components.MemberBegin(); it != components.MemberEnd(); ++it)
    {
        const std::string_view key{it->name.GetString(), it->name.GetStringLength()};
        if (key == "renderable" || key == "socket" || key == "stats" || key == "race")
            m_component_order.emplace_back(key);
    }

    m_renderable = components.HasMember("renderable") ? ReadRenderableBody(components["renderable"]) : RenderableComponent{};
    m_renderable_texture_name = LabelFor(m_renderable.texture_id);

    m_socket = components.HasMember("socket") ? ReadSocketBody(components["socket"]) : SocketComponent{};
    m_socket_fallback_name = LabelFor(m_socket.fallback_prefab_id);

    m_stats = components.HasMember("stats") ? ReadStatsBody(components["stats"]) : StatsComponent{};

    m_race = components.HasMember("race") ? ReadRaceBody(components["race"]) : RaceComponent{};
    m_race_name = LabelFor(m_race.race_id);

    m_pending_delete_id.clear();
    m_error.clear();

    m_mode = Mode::Edit;
    ShowScreen(Mode::Edit);
    RefreshEditForm();
    RefreshErrorDisplay();
}

// -- Edit mode ----------------------------------------------------------------

bool PrefabEditorLayer::HasComponent(std::string_view key) const
{
    return std::find(m_component_order.begin(), m_component_order.end(), key) != m_component_order.end();
}

void PrefabEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void PrefabEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void PrefabEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new prefab)"} : m_draft_id;
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
                                                        EscapeRml(m_draft_id.empty() ? std::string{"(new prefab)"}
                                                                                     : m_draft_id));
                                            }));

    // -- Component cards: one Inspector-style card per m_component_order
    // entry, rebuilt fresh here (mirrors every other rebuildable list in
    // this codebase) -- see kComponentKinds for each card's fixed-id body
    // markup, which the field-wiring below (otherwise unchanged from before
    // this refactor) still looks up by id, now simply absent from the DOM
    // when that component isn't present rather than checkbox-hidden.
    std::string card_markup;
    for (const std::string& key : m_component_order)
    {
        const ComponentKind* kind = FindComponentKind(key);
        if (!kind)
            continue;
        card_markup += "<div class=\"inspector-card list-item\"><div class=\"inspector-card-header\">"
                       "<span class=\"drag-handle\">|||</span>"
                       "<span class=\"component-icon\" style=\"background-color: " +
                       std::string(kind->icon_color) + ";\"></span>" + "<span class=\"collapse-toggle\">-</span>" +
                       "<span class=\"component-title\">" + std::string(kind->title) + "</span>" +
                       "<span class=\"btn row-card-remove\">x</span></div>"
                       "<div class=\"inspector-card-body list-item-body\">" +
                       std::string(kind->body_html) + "</div></div>";
    }
    if (m_component_order.empty())
        card_markup = "<div class=\"list-empty\">No components -- use Add Component below.</div>";

    Rml::ElementList card_elements;
    if (Rml::Element* list = m_editor->GetElementById("component-list"))
    {
        list->SetInnerRML(card_markup);
        list->QuerySelectorAll(card_elements, ".inspector-card");
    }
    const std::vector<Rml::Element*> cards(card_elements.begin(), card_elements.end());

    std::vector<Rml::Element*> handles;
    handles.reserve(cards.size());
    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        Rml::Element* card = cards[i];
        handles.push_back(card->QuerySelector(".drag-handle"));
        keep(fieldwidgets::WireCollapseToggle(*card));

        if (Rml::Element* remove_button = card->QuerySelector(".row-card-remove"))
        {
            const std::size_t index = i;
            auto listener = std::make_unique<RmlClickListener>(
                [this, index]
                {
                    if (index < m_component_order.size())
                        m_component_order.erase(m_component_order.begin() + static_cast<std::ptrdiff_t>(index));
                    MarkDirty();
                    RefreshEditForm();
                });
            listener->Attach(*remove_button);
            m_form_listeners.push_back(std::move(listener));
        }
    }
    keep(fieldwidgets::WireDragReorder(cards, handles,
                                       [this](std::size_t from, std::size_t to)
                                       {
                                           m_pending_action = [this, from, to]
                                           {
                                               fieldwidgets::MoveElement(m_component_order, from, to);
                                               MarkDirty();
                                               RefreshEditForm();
                                           };
                                       }));

    RefreshAddComponentOptions();

    // -- Fields inside whichever cards are currently present -- each lookup
    // simply misses (guarded by the `if`) when that component's card isn't
    // in the DOM, same as the old checkbox-hidden-but-present convention
    // this replaces.
    if (Rml::Element* row = m_editor->GetElementById("field-texture-id"))
        keep(fieldwidgets::BuildTextureField(*row, "texture_id", m_renderable.texture_id, m_renderable_texture_name,
                                             [this](std::uint32_t id, std::string name)
                                             {
                                                 m_renderable.texture_id = id;
                                                 if (!name.empty())
                                                 {
                                                     NameIdRegistry::Register(id, name);
                                                     m_renderable_texture_name = std::move(name);
                                                 }
                                                 MarkDirty();
                                             },
                                             m_pickers.open_texture_picker));
    if (Rml::Element* row = m_editor->GetElementById("field-texture-size"))
        keep(fieldwidgets::BuildVec2Field(*row, "texture_size", m_renderable.texture_size,
                                          [this](Vec2 v)
                                          {
                                              m_renderable.texture_size = v;
                                              MarkDirty();
                                          }));
    if (Rml::Element* row = m_editor->GetElementById("field-uv"))
        keep(fieldwidgets::BuildVec2Field(*row, "uv", m_renderable.uv,
                                          [this](Vec2 v)
                                          {
                                              m_renderable.uv = v;
                                              MarkDirty();
                                          }));
    if (Rml::Element* row = m_editor->GetElementById("field-color-1"))
        keep(fieldwidgets::BuildColorField(*row, "color_1", m_renderable.color_1,
                                           [this](Color v)
                                           {
                                               m_renderable.color_1 = v;
                                               MarkDirty();
                                           },
                                           m_pickers.open_color_picker));
    if (Rml::Element* row = m_editor->GetElementById("field-color-2"))
        keep(fieldwidgets::BuildColorField(*row, "color_2", m_renderable.color_2,
                                           [this](Color v)
                                           {
                                               m_renderable.color_2 = v;
                                               MarkDirty();
                                           },
                                           m_pickers.open_color_picker));
    if (Rml::Element* row = m_editor->GetElementById("field-render-layer"))
        keep(fieldwidgets::BuildIntField(*row, "render_layer", m_renderable.render_layer,
                                         [this](int v)
                                         {
                                             m_renderable.render_layer = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-fallback-prefab"))
        keep(fieldwidgets::BuildNameIdField(*row, "fallback_prefab_id", m_socket.fallback_prefab_id,
                                            m_socket_fallback_name,
                                            [this](std::uint32_t id, std::string name)
                                            {
                                                m_socket.fallback_prefab_id = id;
                                                if (!name.empty())
                                                {
                                                    NameIdRegistry::Register(id, name);
                                                    m_socket_fallback_name = std::move(name);
                                                }
                                                MarkDirty();
                                            }));
    if (Rml::Element* row = m_editor->GetElementById("field-atp"))
        keep(fieldwidgets::BuildIntField(*row, "atp", m_stats.atp,
                                         [this](int v)
                                         {
                                             m_stats.atp = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-ata"))
        keep(fieldwidgets::BuildIntField(*row, "ata", m_stats.ata,
                                         [this](int v)
                                         {
                                             m_stats.ata = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-mst"))
        keep(fieldwidgets::BuildIntField(*row, "mst", m_stats.mst,
                                         [this](int v)
                                         {
                                             m_stats.mst = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-dfp"))
        keep(fieldwidgets::BuildIntField(*row, "dfp", m_stats.dfp,
                                         [this](int v)
                                         {
                                             m_stats.dfp = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-evp"))
        keep(fieldwidgets::BuildIntField(*row, "evp", m_stats.evp,
                                         [this](int v)
                                         {
                                             m_stats.evp = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-lck"))
        keep(fieldwidgets::BuildIntField(*row, "lck", m_stats.lck,
                                         [this](int v)
                                         {
                                             m_stats.lck = v;
                                             MarkDirty();
                                         }));
    if (Rml::Element* row = m_editor->GetElementById("field-race-id"))
        keep(fieldwidgets::BuildNameIdField(*row, "race_id", m_race.race_id, m_race_name,
                                            [this](std::uint32_t id, std::string name)
                                            {
                                                m_race.race_id = id;
                                                if (!name.empty())
                                                {
                                                    NameIdRegistry::Register(id, name);
                                                    m_race_name = std::move(name);
                                                }
                                                MarkDirty();
                                            }));

    if (Rml::Element* add_tag = m_editor->GetElementById("add-tag"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_socket.tags.emplace_back();
                MarkDirty();
                RefreshTagRows();
            });
        listener->Attach(*add_tag);
        m_form_listeners.push_back(std::move(listener));
    }

    RefreshTagRows();
    RefreshDirtyDisplay();
}

void PrefabEditorLayer::RefreshAddComponentOptions()
{
    if (!m_editor)
        return;
    auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(m_editor->GetElementById("add-component-select"));
    if (!select)
        return;

    select->RemoveAll();
    select->Add("+ Add Component", "");
    // Driven by m_schema, not kComponentKinds: a component is offered iff the
    // registered schema says it's authorable. kComponentKinds is consulted
    // only for chrome (title) once a component's already been approved here --
    // if this build has no card UI for an otherwise-authorable component, it's
    // silently skipped rather than offered with nothing to edit.
    for (const ComponentSchema& component : m_schema.components)
        if (component.authorable && !HasComponent(component.id))
            if (const ComponentKind* kind = FindComponentKind(component.id))
                select->Add(kind->title, kind->key);
    select->SetValue("");
}

void PrefabEditorLayer::RefreshTagRows()
{
    if (!m_editor)
        return;
    m_tag_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("tag-list");
    if (!list)
        return;

    const std::vector<std::string> content(m_socket.tags.size(), "<div class=\"tag-id field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No tags configured.</div>",
        [this](std::size_t index)
        {
            if (index < m_socket.tags.size())
                m_socket.tags.erase(m_socket.tags.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshTagRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_socket.tags, from, to);
                MarkDirty();
                RefreshTagRows();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < m_socket.tags.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".tag-id"))
            for (auto& listener : fieldwidgets::BuildStringField(*row, "tag", m_socket.tags[i],
                                                                  [this, index](std::string v)
                                                                  {
                                                                      if (index < m_socket.tags.size())
                                                                          m_socket.tags[index] = std::move(v);
                                                                      MarkDirty();
                                                                  }))
                m_tag_row_listeners.push_back(std::move(listener));
    }
    for (auto& listener : result.listeners)
        m_tag_row_listeners.push_back(std::move(listener));
}

void PrefabEditorLayer::ApplyDraftToDocument()
{
    rapidjson::Document::AllocatorType& allocator = m_draft_document.GetAllocator();
    rapidjson::Value& components = m_draft_document["components"];

    // Remove the two known component keys first (any unknown keys the
    // document carries stay untouched, preserving this class's round-trip
    // guarantee for components this build doesn't know about -- see the
    // class doc comment), then re-add whichever are present in
    // m_component_order's sequence. This is what makes drag-reordering the
    // component cards actually persist to disk: JSON member order is
    // otherwise only affected by add/remove, never by in-place value
    // updates.
    for (const char* key : {"renderable", "socket", "stats", "race"})
        if (auto it = components.FindMember(key); it != components.MemberEnd())
            components.RemoveMember(it);

    for (const std::string& key : m_component_order)
    {
        rapidjson::Value body;
        if (key == "renderable")
            body = WriteRenderableBody(m_renderable, allocator);
        else if (key == "socket")
            body = WriteSocketBody(m_socket, allocator);
        else if (key == "stats")
            body = WriteStatsBody(m_stats, allocator);
        else if (key == "race")
            body = WriteRaceBody(m_race, allocator);
        else
            continue;
        components.AddMember(rapidjson::Value(key.c_str(), allocator), std::move(body), allocator);
    }
}

void PrefabEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Prefab id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "A prefab already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        ApplyDraftToDocument();

        ValidateEntityDocument(m_draft_document, m_schema);

        WriteJsonFile(target, m_draft_document);
        if (!m_original_id.empty() && m_original_id != m_draft_id)
        {
            std::error_code error_code;
            std::filesystem::remove(IdToPath(m_original_id), error_code);
        }
        m_original_id = m_draft_id;
        m_is_new = false;
        m_dirty = false;
        m_error.clear();
        RefreshDirtyDisplay();
    }
    catch (const std::exception& error)
    {
        m_error = error.what();
        SDL_Log("PrefabEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

// -- Preview render -------------------------------------------------------------

void PrefabEditorLayer::InitializeRenderer(SDL_Renderer& renderer)
{
    if (m_renderer_initialized)
        return;
    m_tile_atlas.emplace(renderer, EditorFilepaths::TexturesPath);
    m_gpu_pipeline.emplace(renderer, EditorFilepaths::ShadersPath / kVertexShaderPath,
                           EditorFilepaths::ShadersPath / kFragmentShaderPath);
    m_renderer_initialized = true;
}

void PrefabEditorLayer::RenderPreview(SDL_Renderer& renderer, int output_w, int output_h)
{
    if (!m_editor || !HasComponent("renderable"))
        return;
    Rml::Element* panel = m_editor->GetElementById("grid-panel");
    if (!panel)
        return;

    const bool gpu_ready = m_tile_atlas && m_tile_atlas->IsLoaded() && m_gpu_pipeline && m_gpu_pipeline->IsLoaded();
    if (!gpu_ready)
        return;
    const Vec2 atlas_size = m_tile_atlas->GetSize();
    if (atlas_size.x <= 0 || atlas_size.y <= 0)
        return;

    const std::optional<SDL_FRect> src =
        m_tile_atlas->GetSourceRect(m_renderable.texture_id, m_renderable.texture_size.x, m_renderable.texture_size.y,
                                    m_renderable.uv.x, m_renderable.uv.y);
    if (!src)
        return;

    const Rml::Vector2f panel_offset = panel->GetAbsoluteOffset();
    const Rml::Vector2f panel_size = panel->GetBox().GetSize();
    if (panel_size.x <= 0.0f || panel_size.y <= 0.0f)
        return;

    const SDL_FRect panel_rect{panel_offset.x, panel_offset.y, panel_size.x, panel_size.y};
    const SDL_FRect content_bounds{0.0f, 0.0f, static_cast<float>(m_renderable.texture_size.x),
                                    static_cast<float>(m_renderable.texture_size.y)};
    m_preview_canvas.Update(panel_rect, content_bounds);
    RefreshZoomReadout();

    const SDL_FRect box = m_preview_canvas.WorldToScreen(content_bounds);

    std::vector<TileVertex> vertices;
    AppendSpriteQuad(vertices, box, *src, atlas_size, m_renderable.color_1, m_renderable.color_2, output_w, output_h);
    m_gpu_pipeline->Draw(renderer, *m_tile_atlas->GetGpuTexture(), vertices, output_w, output_h);
}

void PrefabEditorLayer::RefreshZoomReadout()
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

void PrefabEditorLayer::OnRender(SDL_Renderer* renderer)
{
    // Drains a reorder requested by fieldwidgets::WireDragReorder, if any --
    // deferred to here (a frame after the drag gesture ended) rather than run
    // synchronously from the "dragdrop" handler, since that handler's rebuild
    // would destroy RmlUi elements still live in the just-finished drag's own
    // event dispatch. See WireDragReorder's doc comment.
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

void PrefabEditorLayer::OnEvent(Event& event)
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
                RefreshPrefabList();
                break;
            case Mode::List:
                TransitionTo<EditorMenuLayer>();
                break;
            }
            return true;
        });
}

} // namespace psr
