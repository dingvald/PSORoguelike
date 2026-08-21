#include "UI/FieldWidgets.h"

#include "UI/RmlClickListener.h"

#include <RmlUi/Core.h>

#include <entt/core/hashed_string.hpp>

#include <cstdio>
#include <memory>
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

} // namespace psr::fieldwidgets
