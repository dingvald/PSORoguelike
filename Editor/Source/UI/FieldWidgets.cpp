#include "UI/FieldWidgets.h"

#include "UI/RmlClickListener.h"
#include "UI/RmlText.h"

#include <RmlUi/Core.h>

#include <entt/core/hashed_string.hpp>

#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace psr::fieldwidgets {

namespace {

    Rml::Element* Input(Rml::Element& row) { return row.QuerySelector("input"); }

    Rml::ElementFormControlInput* TextInput(Rml::Element& row)
    {
        return rmlui_dynamic_cast<Rml::ElementFormControlInput*>(Input(row));
    }

    void SetInvalid(Rml::Element& element, bool invalid) { element.SetClass("invalid", invalid); }

    std::string ColorToHex(Color color)
    {
        char buffer[10];
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
        return buffer;
    }

    void RefreshSwatch(Rml::Element& swatch, Color color)
    {
        swatch.SetProperty("background-color", "rgba(" + std::to_string(color.r) + "," + std::to_string(color.g) + "," +
                                                   std::to_string(color.b) + "," +
                                                   std::to_string(static_cast<int>(color.a)) + ")");
    }

} // namespace

Listeners BuildIntField(Rml::Element& row, const std::string& label, int initial, std::function<void(int)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><input type=\"text\" class=\"field-input\"/>");
    Rml::ElementFormControlInput* input = TextInput(row);
    if (!input)
        return {};
    input->SetValue(std::to_string(initial));

    Listeners out;
    auto listener = std::make_unique<RmlEventListener>("change",
                                                       [input, on_commit = std::move(on_commit)](Rml::Event&)
                                                       {
                                                           try
                                                           {
                                                               std::size_t consumed = 0;
                                                               const std::string text = input->GetValue();
                                                               const int value = std::stoi(text, &consumed);
                                                               if (consumed != text.size())
                                                                   throw std::invalid_argument("trailing characters");
                                                               SetInvalid(*input, false);
                                                               on_commit(value);
                                                           }
                                                           catch (const std::exception&)
                                                           {
                                                               SetInvalid(*input, true);
                                                           }
                                                       });
    listener->Attach(*input);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildFloatField(Rml::Element& row, const std::string& label, float initial,
                          std::function<void(float)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><input type=\"text\" class=\"field-input\"/>");
    Rml::ElementFormControlInput* input = TextInput(row);
    if (!input)
        return {};
    input->SetValue(std::to_string(initial));

    Listeners out;
    auto listener = std::make_unique<RmlEventListener>("change",
                                                       [input, on_commit = std::move(on_commit)](Rml::Event&)
                                                       {
                                                           try
                                                           {
                                                               std::size_t consumed = 0;
                                                               const std::string text = input->GetValue();
                                                               const float value = std::stof(text, &consumed);
                                                               if (consumed != text.size())
                                                                   throw std::invalid_argument("trailing characters");
                                                               SetInvalid(*input, false);
                                                               on_commit(value);
                                                           }
                                                           catch (const std::exception&)
                                                           {
                                                               SetInvalid(*input, true);
                                                           }
                                                       });
    listener->Attach(*input);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildStringField(Rml::Element& row, const std::string& label, const std::string& initial,
                           std::function<void(std::string)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><input type=\"text\" class=\"field-input\"/>");
    Rml::ElementFormControlInput* input = TextInput(row);
    if (!input)
        return {};
    input->SetValue(initial);

    Listeners out;
    auto listener = std::make_unique<RmlEventListener>(
        "change",
        [on_commit = std::move(on_commit)](Rml::Event& event)
        {
            auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(event.GetTargetElement());
            if (input)
                on_commit(input->GetValue());
        });
    listener->Attach(*input);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildBoolField(Rml::Element& row, const std::string& label, bool initial, std::function<void(bool)> on_commit)
{
    // checkbox-input (distinct from other .field-input controls): the
    // generic input.field-input rule sizes text/number inputs to 200px wide,
    // which would otherwise apply to this checkbox too and render it hugely
    // oversized with no visible checked/unchecked state.
    row.SetInnerRML("<span class=\"field-label\">" + label +
                    "</span><input type=\"checkbox\" class=\"field-input checkbox-input\"/>");
    Rml::Element* input = Input(row);
    if (!input)
        return {};
    if (initial)
        input->SetAttribute("checked", "");

    Listeners out;
    // A checkbox's "checked" attribute flips during Click's default-action phase,
    // then InputTypeCheckbox::OnAttributeChange dispatches "change" with a
    // "checked" parameter -- listening for "change" (not "click") avoids any
    // dependency on listener/default-action ordering.
    auto listener = std::make_unique<RmlEventListener>("change", [on_commit = std::move(on_commit)](Rml::Event& event)
                                                       { on_commit(event.GetParameter<bool>("checked", false)); });
    listener->Attach(*input);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildNameIdField(Rml::Element& row, const std::string& label, std::uint32_t initial_id,
                           const std::string& initial_text, std::function<void(std::uint32_t, std::string)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><input type=\"text\" class=\"field-input\"/>");
    Rml::ElementFormControlInput* input = TextInput(row);
    if (!input)
        return {};
    input->SetValue(initial_text.empty() ? std::to_string(initial_id) : initial_text);

    Listeners out;
    auto listener =
        std::make_unique<RmlEventListener>("change",
                                           [input, on_commit = std::move(on_commit)](Rml::Event&)
                                           {
                                               const std::string text = input->GetValue();
                                               if (text.empty())
                                               {
                                                   SetInvalid(*input, true);
                                                   return;
                                               }
                                               SetInvalid(*input, false);
                                               try
                                               {
                                                   std::size_t consumed = 0;
                                                   const unsigned long value = std::stoul(text, &consumed);
                                                   if (consumed == text.size())
                                                   {
                                                       // A bare integer: there is no name to retain alongside it.
                                                       on_commit(static_cast<std::uint32_t>(value), std::string{});
                                                       return;
                                                   }
                                               }
                                               catch (const std::exception&)
                                               {
                                                   // Not a bare integer -- fall through to the name-as-id
                                                   // convention below.
                                               }
                                               on_commit(entt::hashed_string::value(text.c_str()), text);
                                           });
    listener->Attach(*input);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildVec2Field(Rml::Element& row, const std::string& label, Vec2 initial, std::function<void(Vec2)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label +
                    "</span><input type=\"text\" class=\"field-input field-input-small\"/>"
                    "<input type=\"text\" class=\"field-input field-input-small\"/>");
    Rml::ElementList inputs;
    row.QuerySelectorAll(inputs, "input");
    if (inputs.size() != 2)
        return {};
    auto* x_input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(inputs[0]);
    auto* y_input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(inputs[1]);
    if (!x_input || !y_input)
        return {};
    x_input->SetValue(std::to_string(initial.x));
    y_input->SetValue(std::to_string(initial.y));

    auto commit = [x_input, y_input, on_commit](Rml::Event&)
    {
        try
        {
            std::size_t x_consumed = 0, y_consumed = 0;
            const std::string x_text = x_input->GetValue();
            const std::string y_text = y_input->GetValue();
            const int x = std::stoi(x_text, &x_consumed);
            const int y = std::stoi(y_text, &y_consumed);
            if (x_consumed != x_text.size() || y_consumed != y_text.size())
                throw std::invalid_argument("trailing characters");
            SetInvalid(*x_input, false);
            SetInvalid(*y_input, false);
            on_commit(Vec2{x, y});
        }
        catch (const std::exception&)
        {
            SetInvalid(*x_input, true);
            SetInvalid(*y_input, true);
        }
    };

    Listeners out;
    auto x_listener = std::make_unique<RmlEventListener>("change", commit);
    x_listener->Attach(*x_input);
    out.push_back(std::move(x_listener));
    auto y_listener = std::make_unique<RmlEventListener>("change", commit);
    y_listener->Attach(*y_input);
    out.push_back(std::move(y_listener));
    return out;
}

Listeners BuildEnumField(Rml::Element& row, const std::string& label, const std::vector<std::string>& options,
                         const std::string& initial, std::function<void(std::string)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><select class=\"field-input\"></select>");
    auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(row.QuerySelector("select"));
    if (!select)
        return {};
    for (const std::string& option : options)
        select->Add(option, option);
    select->SetValue(initial);

    Listeners out;
    auto listener = std::make_unique<RmlEventListener>("change", [select, on_commit = std::move(on_commit)](Rml::Event&)
                                                       { on_commit(select->GetValue()); });
    listener->Attach(*select);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildIdEnumField(Rml::Element& row, const std::string& label,
                           const std::vector<std::pair<std::uint32_t, std::string>>& options,
                           std::uint32_t initial_id, std::function<void(std::uint32_t)> on_commit)
{
    row.SetInnerRML("<span class=\"field-label\">" + label + "</span><select class=\"field-input\"></select>");
    auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(row.QuerySelector("select"));
    if (!select)
        return {};
    for (const auto& [id, display_label] : options)
        select->Add(EscapeRml(display_label), std::to_string(id));
    select->SetValue(std::to_string(initial_id));

    Listeners out;
    auto listener = std::make_unique<RmlEventListener>(
        "change",
        [select, on_commit = std::move(on_commit)](Rml::Event&)
        {
            const std::string value = select->GetValue();
            if (value.empty())
                return;
            on_commit(static_cast<std::uint32_t>(std::stoul(value)));
        });
    listener->Attach(*select);
    out.push_back(std::move(listener));
    return out;
}

Listeners BuildColorField(Rml::Element& row, const std::string& label, Color initial,
                          std::function<void(Color)> on_commit,
                          std::function<void(Color, std::function<void(Color)>)> on_open_picker)
{
    row.SetInnerRML("<span class=\"field-label\">" + label +
                    "</span><input type=\"text\" class=\"field-input field-input-hex\"/>"
                    "<span class=\"color-swatch\"></span><span class=\"btn pick\">Pick...</span>");
    Rml::ElementFormControlInput* input = TextInput(row);
    Rml::Element* swatch = row.QuerySelector(".color-swatch");
    Rml::Element* pick_button = row.QuerySelector(".pick");
    if (!input || !swatch || !pick_button)
        return {};

    input->SetValue(ColorToHex(initial));
    RefreshSwatch(*swatch, initial);

    Listeners out;
    auto text_listener =
        std::make_unique<RmlEventListener>("change",
                                           [input, swatch, on_commit](Rml::Event&)
                                           {
                                               try
                                               {
                                                   const Color color{std::string_view{input->GetValue()}};
                                                   SetInvalid(*input, false);
                                                   RefreshSwatch(*swatch, color);
                                                   on_commit(color);
                                               }
                                               catch (const std::invalid_argument&)
                                               {
                                                   SetInvalid(*input, true);
                                               }
                                           });
    text_listener->Attach(*input);
    out.push_back(std::move(text_listener));

    auto pick_listener = std::make_unique<RmlClickListener>(
        [input, swatch, on_commit, on_open_picker]
        {
            Color current;
            try
            {
                current = Color{std::string_view{input->GetValue()}};
            }
            catch (const std::invalid_argument&)
            {
            }
            on_open_picker(current,
                           [input, swatch, on_commit](Color picked)
                           {
                               input->SetValue(ColorToHex(picked));
                               RefreshSwatch(*swatch, picked);
                               on_commit(picked);
                           });
        });
    pick_listener->Attach(*pick_button);
    out.push_back(std::move(pick_listener));
    return out;
}

Listeners
BuildTextureField(Rml::Element& row, const std::string& label, std::uint32_t initial_id,
                  const std::string& initial_text, std::function<void(std::uint32_t, std::string)> on_commit,
                  std::function<void(std::uint32_t, std::function<void(std::uint32_t, std::string)>)> on_open_picker)
{
    row.SetInnerRML("<span class=\"field-label\">" + label +
                    "</span><span class=\"texture-id-value\"></span><span class=\"btn pick\">Choose...</span>");
    Rml::Element* value_span = row.QuerySelector(".texture-id-value");
    Rml::Element* pick_button = row.QuerySelector(".pick");
    if (!value_span || !pick_button)
        return {};

    auto current = std::make_shared<std::uint32_t>(initial_id);
    value_span->SetInnerRML(initial_text.empty() ? std::to_string(*current) : initial_text);

    Listeners out;
    auto listener = std::make_unique<RmlClickListener>(
        [current, value_span, on_commit, on_open_picker]
        {
            on_open_picker(*current,
                           [current, value_span, on_commit](std::uint32_t picked_id, std::string picked_text)
                           {
                               *current = picked_id;
                               value_span->SetInnerRML(picked_text);
                               on_commit(picked_id, std::move(picked_text));
                           });
        });
    listener->Attach(*pick_button);
    out.push_back(std::move(listener));
    return out;
}

Listeners WireCollapseToggle(Rml::Element& item)
{
    Rml::Element* toggle = item.QuerySelector(".collapse-toggle");
    if (!toggle)
        return {};

    Rml::Element* item_ptr = &item;
    Listeners out;
    auto listener = std::make_unique<RmlClickListener>(
        [item_ptr, toggle]
        {
            // Plain "+"/"-" rather than a Unicode triangle glyph: the editor's
            // pixel font isn't guaranteed to have geometric-shape codepoints,
            // and ASCII is guaranteed to render in any font.
            const bool now_collapsed = !item_ptr->IsClassSet("collapsed");
            item_ptr->SetClass("collapsed", now_collapsed);
            toggle->SetInnerRML(now_collapsed ? "+" : "-");
        });
    listener->Attach(*toggle);
    out.push_back(std::move(listener));
    return out;
}

Listeners WireDragReorder(const std::vector<Rml::Element*>& rows, const std::vector<Rml::Element*>& handles,
                          std::function<void(std::size_t, std::size_t)> request_reorder)
{
    Listeners out;
    if (rows.size() != handles.size())
        return out;

    // Shared by every row's listeners below: which row a drag started from,
    // live only while a drag is in flight.
    auto dragging_index = std::make_shared<std::optional<std::size_t>>();

    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        Rml::Element* handle = handles[i];
        Rml::Element* row = rows[i];
        if (!handle || !row)
            continue;

        auto start_listener = std::make_unique<RmlEventListener>("dragstart", [dragging_index, row, i](Rml::Event&)
                                                                  {
                                                                      *dragging_index = i;
                                                                      row->SetClass("dragging", true);
                                                                  });
        start_listener->Attach(*handle);
        out.push_back(std::move(start_listener));

        // Cleanup only -- reordering itself happens via "dragdrop" below, not
        // here. Fires after "dragdrop" (source is notified once the drag
        // gesture as a whole has ended), so rows[] are still live elements at
        // this point as long as request_reorder only stashed its indices.
        auto end_listener = std::make_unique<RmlEventListener>("dragend", [dragging_index, rows](Rml::Event&)
                                                                {
                                                                    dragging_index->reset();
                                                                    for (Rml::Element* r : rows)
                                                                        if (r)
                                                                        {
                                                                            r->SetClass("dragging", false);
                                                                            r->SetClass("drag-over", false);
                                                                        }
                                                                });
        end_listener->Attach(*handle);
        out.push_back(std::move(end_listener));
    }

    for (std::size_t j = 0; j < rows.size(); ++j)
    {
        Rml::Element* row = rows[j];
        if (!row)
            continue;

        auto over_listener = std::make_unique<RmlEventListener>("dragover", [dragging_index, rows, row, j](Rml::Event&)
                                                                 {
                                                                     if (!*dragging_index || **dragging_index == j)
                                                                         return;
                                                                     for (Rml::Element* r : rows)
                                                                         if (r)
                                                                             r->SetClass("drag-over", false);
                                                                     row->SetClass("drag-over", true);
                                                                 });
        over_listener->Attach(*row);
        out.push_back(std::move(over_listener));

        auto out_listener =
            std::make_unique<RmlEventListener>("dragout", [row](Rml::Event&) { row->SetClass("drag-over", false); });
        out_listener->Attach(*row);
        out.push_back(std::move(out_listener));

        auto drop_listener = std::make_unique<RmlEventListener>("dragdrop", [dragging_index, request_reorder, j](Rml::Event&)
                                                                 {
                                                                     if (*dragging_index && **dragging_index != j)
                                                                         request_reorder(**dragging_index, j);
                                                                 });
        drop_listener->Attach(*row);
        out.push_back(std::move(drop_listener));
    }

    return out;
}

RowList BuildRowList(Rml::Element& container, const std::vector<std::string>& content_html,
                     const std::string& empty_message, std::function<void(std::size_t)> on_remove,
                     std::function<void(std::size_t, std::size_t)> request_reorder)
{
    RowList result;
    if (content_html.empty())
    {
        container.SetInnerRML(empty_message);
        return result;
    }

    std::string markup;
    for (const std::string& inner : content_html)
        markup += "<div class=\"row-card\"><span class=\"drag-handle\">|||</span><div class=\"row-card-content\">" +
                  inner + "</div><span class=\"btn row-card-remove\">x</span></div>";
    container.SetInnerRML(markup);

    Rml::ElementList element_rows;
    container.QuerySelectorAll(element_rows, ".row-card");
    result.rows.assign(element_rows.begin(), element_rows.end());

    std::vector<Rml::Element*> handles;
    handles.reserve(result.rows.size());
    for (Rml::Element* row : result.rows)
        handles.push_back(row ? row->QuerySelector(".drag-handle") : nullptr);

    for (std::size_t i = 0; i < result.rows.size() && i < content_html.size(); ++i)
    {
        const std::size_t index = i;
        if (Rml::Element* remove_button = result.rows[i]->QuerySelector(".row-card-remove"))
        {
            auto listener = std::make_unique<RmlClickListener>([on_remove, index] { on_remove(index); });
            listener->Attach(*remove_button);
            result.listeners.push_back(std::move(listener));
        }
    }

    for (auto& listener : WireDragReorder(result.rows, handles, std::move(request_reorder)))
        result.listeners.push_back(std::move(listener));

    return result;
}

} // namespace psr::fieldwidgets
