#include "Layers/PhotonArtEditorLayer.h"

#include "Combat/PhotonArtLibraryFile.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
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
    const std::filesystem::path kFontPath = EditorFilepaths::FontsPath / "PixelCode-Regular.ttf";
    const std::filesystem::path kFontPathBold = EditorFilepaths::FontsPath / "PixelCode-Bold.ttf";
    const std::filesystem::path kEditorDocument = EditorFilepaths::RmlDocumentsPath / "photon_art_editor.rml";

    // Mirrors PieceEditorLayer.cpp/DungeonEditorLayer.cpp/AffixEditorLayer.cpp's
    // own IdToPath.
    std::filesystem::path IdToPath(const std::string& id)
    {
        std::filesystem::path path = EditorFilepaths::PhotonArtsPath;
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

    template <typename E> std::vector<std::string> EnumOptions()
    {
        std::vector<std::string> options;
        for (const auto& [text, value] : EnumNames<E>::kValues)
        {
            (void)value;
            options.push_back(std::string{text});
        }
        return options;
    }

    template <typename E> std::string EnumToString(E value)
    {
        for (const auto& [text, candidate] : EnumNames<E>::kValues)
            if (candidate == value)
                return std::string{text};
        return std::string{EnumNames<E>::kValues.front().first}; // unreachable for a valid enum value
    }

    template <typename E> E EnumFromString(const std::string& text, E fallback)
    {
        for (const auto& [name, value] : EnumNames<E>::kValues)
            if (name == text)
                return value;
        return fallback;
    }

    std::string LabelFor(std::uint32_t id)
    {
        if (id == 0)
            return {};
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return *label;
        return {};
    }
} // namespace

PhotonArtEditorLayer::PhotonArtEditorLayer() : Layer("PhotonArtEditorLayer") {}
PhotonArtEditorLayer::~PhotonArtEditorLayer() = default;

// -- Lifecycle ----------------------------------------------------------------

void PhotonArtEditorLayer::OnAttach()
{
    if (!Rml::LoadFontFace(kFontPath.string().c_str()))
        SDL_Log("Warning: PhotonArtEditorLayer failed to load font '%s'", kFontPath.string().c_str());
    if (!Rml::LoadFontFace(kFontPathBold.string().c_str()))
        SDL_Log("Warning: PhotonArtEditorLayer failed to load font '%s'", kFontPathBold.string().c_str());

    LoadDocuments();
    ReloadLibrary();
    RefreshList();
    ShowScreen(Mode::List);
}

void PhotonArtEditorLayer::OnDetach()
{
    m_tier_row_listeners.clear();
    m_form_listeners.clear();
    m_list_listeners.clear();
    m_listeners.clear();

    if (m_editor)
    {
        m_editor->Close();
        m_editor = nullptr;
    }
}

void PhotonArtEditorLayer::LoadDocuments()
{
    {
        GuiContext::LockedAccess gui_context = GetLockedGuiContext();
        m_editor = gui_context->LoadDocument(kEditorDocument.string().c_str());
    }
    if (!m_editor)
    {
        SDL_Log("Warning: PhotonArtEditorLayer has no editor document");
        return;
    }

    WireButtonClick("new-photon-art", [this] { BeginNew(); });
    WireButtonClick("back-to-menu", [this] { TransitionTo<EditorMenuLayer>(); });
    WireButtonClick("save-photon-art", [this] { SaveDraft(); });
    WireButtonClick("back-to-list",
                    [this]
                    {
                        m_mode = Mode::List;
                        ShowScreen(Mode::List);
                        RefreshList();
                    });

    m_editor->Show();
}

void PhotonArtEditorLayer::WireButtonClick(const char* element_id, std::function<void()> on_click)
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

void PhotonArtEditorLayer::ShowScreen(Mode mode)
{
    if (!m_editor)
        return;
    static constexpr std::array<std::pair<Mode, const char*>, 2> kScreenIds = {
        {{Mode::List, "screen-list"}, {Mode::Edit, "screen-edit"}}};
    for (const auto& [screen_mode, element_id] : kScreenIds)
        if (Rml::Element* screen = m_editor->GetElementById(element_id))
            screen->SetProperty("display", screen_mode == mode ? "block" : "none");
}

void PhotonArtEditorLayer::RefreshErrorDisplay()
{
    if (!m_editor)
        return;
    if (Rml::Element* list_error = m_editor->GetElementById("list-error"))
        list_error->SetInnerRML(EscapeRml(m_error));
    if (Rml::Element* edit_error = m_editor->GetElementById("edit-error"))
        edit_error->SetInnerRML(EscapeRml(m_error));
}

// -- List mode ----------------------------------------------------------------

void PhotonArtEditorLayer::ReloadLibrary()
{
    try
    {
        m_photon_arts = LoadPhotonArtLibrary(EditorFilepaths::PhotonArtsPath);
        m_error.clear();
    }
    catch (const std::exception& error)
    {
        m_photon_arts = PhotonArtLibrary{};
        m_error = error.what();
    }
    RefreshErrorDisplay();
}

void PhotonArtEditorLayer::RefreshList()
{
    if (!m_editor)
        return;
    m_list_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("photon-art-list");
    if (!list)
        return;

    const std::vector<PhotonArt>& photon_arts = m_photon_arts.All();
    if (photon_arts.empty())
    {
        list->SetInnerRML("<div class=\"list-empty\">No Photon Arts yet -- click New Photon Art to create one.</div>");
        return;
    }

    std::string markup;
    for (const PhotonArt& art : photon_arts)
    {
        const bool confirming = art.id_string == m_pending_delete_id;
        const std::string label = art.name.empty() ? art.id_string : art.name;
        markup += "<div class=\"list-row\"><span class=\"list-name\">" + EscapeRml(label) +
                  "</span><span class=\"btn edit\">Edit</span><span class=\"btn delete\">" +
                  (confirming ? "Confirm?" : "Delete") + "</span></div>";
    }
    list->SetInnerRML(markup);

    Rml::ElementList rows;
    list->QuerySelectorAll(rows, ".list-row");
    for (std::size_t i = 0; i < rows.size() && i < photon_arts.size(); ++i)
    {
        const std::string id = photon_arts[i].id_string;
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

void PhotonArtEditorLayer::RequestDelete(const std::string& id)
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

void PhotonArtEditorLayer::OpenForEdit(const std::string& id)
{
    const PhotonArt* found = m_photon_arts.Find(entt::hashed_string::value(id.c_str()));
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

void PhotonArtEditorLayer::BeginNew()
{
    m_draft = PhotonArt{};

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

void PhotonArtEditorLayer::MarkDirty()
{
    m_dirty = true;
    RefreshDirtyDisplay();
}

void PhotonArtEditorLayer::RefreshDirtyDisplay()
{
    if (m_editor)
        if (Rml::Element* dirty = m_editor->GetElementById("edit-dirty"))
            dirty->SetInnerRML(m_dirty ? "unsaved" : "");
}

void PhotonArtEditorLayer::RefreshEditForm()
{
    if (!m_editor)
        return;
    m_form_listeners.clear();

    const std::string display_id = m_draft_id.empty() ? std::string{"(new photon art)"} : m_draft_id;
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
                    title->SetInnerRML(EscapeRml(m_draft_id.empty() ? std::string{"(new photon art)"} : m_draft_id));
            }));

    if (Rml::Element* row = m_editor->GetElementById("field-name"))
        keep(fieldwidgets::BuildStringField(*row, "name", m_draft.name,
                                            [this](std::string v)
                                            {
                                                m_draft.name = std::move(v);
                                                MarkDirty();
                                            }));

    if (Rml::Element* row = m_editor->GetElementById("field-tp-cost"))
        keep(fieldwidgets::BuildIntField(*row, "tp_cost", m_draft.tp_cost,
                                         [this](int v)
                                         {
                                             m_draft.tp_cost = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* row = m_editor->GetElementById("field-targeting-mode"))
        keep(fieldwidgets::BuildEnumField(*row, "targeting_mode", EnumOptions<TargetingMode>(),
                                          EnumToString(m_draft.targeting_mode),
                                          [this](std::string v)
                                          {
                                              m_draft.targeting_mode = EnumFromString(v, TargetingMode::Directional);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-range-shape"))
        keep(fieldwidgets::BuildEnumField(*row, "range_shape", EnumOptions<WeaponRangeShape>(),
                                          EnumToString(m_draft.range_shape),
                                          [this](std::string v)
                                          {
                                              m_draft.range_shape = EnumFromString(v, WeaponRangeShape::SingleTarget);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-range"))
        keep(fieldwidgets::BuildIntField(*row, "range", m_draft.range,
                                         [this](int v)
                                         {
                                             m_draft.range = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* row = m_editor->GetElementById("field-hits-per-turn"))
        keep(fieldwidgets::BuildIntField(*row, "hits_per_turn", m_draft.hits_per_turn,
                                         [this](int v)
                                         {
                                             m_draft.hits_per_turn = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* row = m_editor->GetElementById("field-effect-family"))
        keep(fieldwidgets::BuildEnumField(*row, "effect_family", EnumOptions<EffectFamily>(),
                                          EnumToString(m_draft.effect_family),
                                          [this](std::string v)
                                          {
                                              m_draft.effect_family = EnumFromString(v, EffectFamily::Damage);
                                              MarkDirty();
                                          }));

    if (Rml::Element* row = m_editor->GetElementById("field-drain-percent"))
        keep(fieldwidgets::BuildIntField(*row, "drain_percent", m_draft.drain_percent,
                                         [this](int v)
                                         {
                                             m_draft.drain_percent = v;
                                             MarkDirty();
                                         }));

    if (Rml::Element* row = m_editor->GetElementById("field-status-effect"))
        keep(fieldwidgets::BuildNameIdField(*row, "status_effect_id", m_draft.status_effect_id,
                                            LabelFor(m_draft.status_effect_id),
                                            [this](std::uint32_t id, std::string name)
                                            {
                                                m_draft.status_effect_id = id;
                                                if (!name.empty())
                                                    NameIdRegistry::Register(id, name);
                                                MarkDirty();
                                            }));

    if (Rml::Element* add_tier = m_editor->GetElementById("add-tier"))
    {
        auto listener = std::make_unique<RmlClickListener>(
            [this]
            {
                m_draft.tiers.emplace_back();
                MarkDirty();
                RefreshTierRows();
            });
        listener->Attach(*add_tier);
        m_form_listeners.push_back(std::move(listener));
    }

    RefreshTierRows();
    RefreshDirtyDisplay();
}

void PhotonArtEditorLayer::RefreshTierRows()
{
    if (!m_editor)
        return;
    m_tier_row_listeners.clear();

    Rml::Element* list = m_editor->GetElementById("tier-list");
    if (!list)
        return;

    const std::vector<std::string> content(
        m_draft.tiers.size(),
        "<div class=\"tier-value field-row\"></div><div class=\"tier-multiplier field-row\"></div>");

    fieldwidgets::RowList result = fieldwidgets::BuildRowList(
        *list, content, "<div class=\"list-empty\">No tiers configured (defaults to a flat 1.0x multiplier).</div>",
        [this](std::size_t index)
        {
            if (index < m_draft.tiers.size())
                m_draft.tiers.erase(m_draft.tiers.begin() + static_cast<std::ptrdiff_t>(index));
            MarkDirty();
            RefreshTierRows();
        },
        [this](std::size_t from, std::size_t to)
        {
            m_pending_action = [this, from, to]
            {
                fieldwidgets::MoveElement(m_draft.tiers, from, to);
                MarkDirty();
                RefreshTierRows();
            };
        });

    for (std::size_t i = 0; i < result.rows.size() && i < m_draft.tiers.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* row = result.rows[i]->QuerySelector(".tier-value"))
            for (auto& listener : fieldwidgets::BuildIntField(*row, "tier", m_draft.tiers[i].tier,
                                                              [this, index](int v)
                                                              {
                                                                  if (index < m_draft.tiers.size())
                                                                      m_draft.tiers[index].tier = v;
                                                                  MarkDirty();
                                                              }))
                m_tier_row_listeners.push_back(std::move(listener));
        if (Rml::Element* row = result.rows[i]->QuerySelector(".tier-multiplier"))
            for (auto& listener :
                 fieldwidgets::BuildFloatField(*row, "power_multiplier", m_draft.tiers[i].power_multiplier,
                                               [this, index](float v)
                                               {
                                                   if (index < m_draft.tiers.size())
                                                       m_draft.tiers[index].power_multiplier = v;
                                                   MarkDirty();
                                               }))
                m_tier_row_listeners.push_back(std::move(listener));
    }
    for (auto& listener : result.listeners)
        m_tier_row_listeners.push_back(std::move(listener));
}

void PhotonArtEditorLayer::SaveDraft()
{
    if (m_draft_id.empty())
    {
        m_error = "Photon Art id must not be empty";
        RefreshErrorDisplay();
        return;
    }

    const std::filesystem::path target = IdToPath(m_draft_id);
    if (m_draft_id != m_original_id && std::filesystem::exists(target))
    {
        m_error = "A Photon Art already exists at '" + m_draft_id + "'";
        RefreshErrorDisplay();
        return;
    }

    try
    {
        SavePhotonArt(target, m_draft);
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
        SDL_Log("PhotonArtEditorLayer: save failed: %s", m_error.c_str());
    }
    RefreshErrorDisplay();
}

void PhotonArtEditorLayer::OnRender(SDL_Renderer* renderer)
{
    (void)renderer;
    // See fieldwidgets::WireDragReorder's doc comment / PrefabEditorLayer's
    // own m_pending_action precedent -- deferred a frame past the drag
    // gesture that requested it.
    if (m_pending_action)
    {
        const std::function<void()> action = std::exchange(m_pending_action, nullptr);
        action();
    }
}

// -- Events -------------------------------------------------------------------

void PhotonArtEditorLayer::OnEvent(Event& event)
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
