#include "UI/ColorPickerPopup.h"

#include "UI/RmlClickListener.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace psr {

namespace {

    Color HsvaToColor(float h, float s, float v, std::uint8_t a)
    {
        const float c = v * s;
        const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        const float m = v - c;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        if (h < 60.0f)
        {
            r = c;
            g = x;
        }
        else if (h < 120.0f)
        {
            r = x;
            g = c;
        }
        else if (h < 180.0f)
        {
            g = c;
            b = x;
        }
        else if (h < 240.0f)
        {
            g = x;
            b = c;
        }
        else if (h < 300.0f)
        {
            r = x;
            b = c;
        }
        else
        {
            r = c;
            b = x;
        }
        return Color{static_cast<std::uint8_t>((r + m) * 255.0f), static_cast<std::uint8_t>((g + m) * 255.0f),
                     static_cast<std::uint8_t>((b + m) * 255.0f), a};
    }

    void ColorToHsv(Color color, float& h, float& s, float& v)
    {
        const float r = color.r / 255.0f, g = color.g / 255.0f, b = color.b / 255.0f;
        const float max_c = std::max({r, g, b});
        const float min_c = std::min({r, g, b});
        const float delta = max_c - min_c;

        v = max_c;
        s = max_c <= 0.0f ? 0.0f : delta / max_c;

        if (delta <= 0.0f)
            h = 0.0f;
        else if (max_c == r)
            h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        else if (max_c == g)
            h = 60.0f * ((b - r) / delta + 2.0f);
        else
            h = 60.0f * ((r - g) / delta + 4.0f);
        if (h < 0.0f)
            h += 360.0f;
    }

    std::string ColorToHex(Color color)
    {
        char buffer[10];
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x", color.r, color.g, color.b, color.a);
        return buffer;
    }

    std::string HueToHex(float hue)
    {
        const Color pure = HsvaToColor(hue, 1.0f, 1.0f, 255);
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", pure.r, pure.g, pure.b);
        return buffer;
    }

} // namespace

void ColorPickerPopup::Bind(Rml::ElementDocument& document)
{
    m_document = &document;
    m_sv_square = document.GetElementById("sv-square");
    m_sv_saturation_layer = document.GetElementById("sv-saturation-layer");
    m_sv_value_layer = document.GetElementById("sv-value-layer");
    m_hue_strip = document.GetElementById("hue-strip");
    m_hue_gradient = document.GetElementById("hue-gradient");
    m_sv_indicator = document.GetElementById("sv-indicator");
    m_hue_indicator = document.GetElementById("hue-indicator");
    m_alpha_slider = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(document.GetElementById("alpha-slider"));
    m_hex_input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(document.GetElementById("hex-input"));

    BuildValueOverlayRows();
    BuildHueGradientRows();

    // dragstart's own parameters carry the position the drag began at (see
    // Context::ProcessMouseMove's drag_start_parameters); "drag" fires
    // continuously afterward, following the pointer even once it leaves the
    // element's bounds (real drag capture, not element-local mousemove).
    if (m_sv_square)
    {
        for (const char* event_name : {"dragstart", "drag"})
        {
            auto listener =
                std::make_unique<RmlEventListener>(event_name,
                                                   [this](Rml::Event& event)
                                                   {
                                                       HandleSvPointer(event.GetParameter<float>("mouse_x", 0.0f),
                                                                       event.GetParameter<float>("mouse_y", 0.0f));
                                                   });
            listener->Attach(*m_sv_square);
            m_listeners.push_back(std::move(listener));
        }
    }
    if (m_hue_strip)
    {
        for (const char* event_name : {"dragstart", "drag"})
        {
            auto listener =
                std::make_unique<RmlEventListener>(event_name, [this](Rml::Event& event)
                                                   { HandleHuePointer(event.GetParameter<float>("mouse_y", 0.0f)); });
            listener->Attach(*m_hue_strip);
            m_listeners.push_back(std::move(listener));
        }
    }
    if (m_alpha_slider)
    {
        auto listener =
            std::make_unique<RmlEventListener>("change",
                                               [this](Rml::Event&)
                                               {
                                                   try
                                                   {
                                                       const int value =
                                                           std::stoi(std::string(m_alpha_slider->GetValue()));
                                                       m_alpha = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
                                                   }
                                                   catch (const std::exception&)
                                                   {
                                                   }
                                                   RefreshFromHsva();
                                               });
        listener->Attach(*m_alpha_slider);
        m_listeners.push_back(std::move(listener));
    }
    if (m_hex_input)
    {
        auto listener = std::make_unique<RmlEventListener>("change", [this](Rml::Event&) { HandleHexCommit(); });
        listener->Attach(*m_hex_input);
        m_listeners.push_back(std::move(listener));
    }
    if (Rml::Element* done = document.GetElementById("picker-done"))
    {
        auto listener = std::make_unique<RmlClickListener>([this] { Close(); });
        listener->Attach(*done);
        m_listeners.push_back(std::move(listener));
    }
}

void ColorPickerPopup::Unbind()
{
    m_listeners.clear();
    m_document = nullptr;
    m_sv_square = nullptr;
    m_sv_saturation_layer = nullptr;
    m_sv_value_layer = nullptr;
    m_hue_strip = nullptr;
    m_hue_gradient = nullptr;
    m_sv_indicator = nullptr;
    m_hue_indicator = nullptr;
    m_alpha_slider = nullptr;
    m_hex_input = nullptr;
    m_sv_saturation_columns.clear();
}

void ColorPickerPopup::Open(Color initial, std::function<void(Color)> on_pick)
{
    if (!m_document)
        return;
    m_on_pick = std::move(on_pick);
    ColorToHsv(initial, m_hue, m_saturation, m_value);
    m_alpha = initial.a;

    if (m_alpha_slider)
        m_alpha_slider->SetValue(std::to_string(static_cast<int>(m_alpha)));
    if (m_hex_input)
        m_hex_input->SetValue(ColorToHex(initial));

    RefreshSvGradient();
    RefreshIndicators();

    m_open = true;
    m_document->Show();
}

void ColorPickerPopup::Close()
{
    m_open = false;
    m_on_pick = nullptr;
    if (m_document)
        m_document->Hide();
}

void ColorPickerPopup::RefreshFromHsva()
{
    const Color color = HsvaToColor(m_hue, m_saturation, m_value, m_alpha);
    if (m_hex_input)
        m_hex_input->SetValue(ColorToHex(color));
    RefreshIndicators();
    if (m_on_pick)
        m_on_pick(color);
}

void ColorPickerPopup::RefreshIndicators()
{
    if (m_sv_square && m_sv_indicator)
    {
        const Rml::Vector2f size = m_sv_square->GetBox().GetSize();
        m_sv_indicator->SetProperty("left", std::to_string(m_saturation * size.x) + "px");
        m_sv_indicator->SetProperty("top", std::to_string((1.0f - m_value) * size.y) + "px");
    }
    if (m_hue_strip && m_hue_indicator)
    {
        const Rml::Vector2f size = m_hue_strip->GetBox().GetSize();
        m_hue_indicator->SetProperty("top", std::to_string((m_hue / 360.0f) * size.y) + "px");
    }
    RefreshSvGradient();
}

void ColorPickerPopup::RefreshSvGradient()
{
    if (!m_sv_saturation_layer)
        return;

    constexpr int kColumns = 24;
    if (m_sv_saturation_columns.size() != static_cast<std::size_t>(kColumns))
    {
        // float:left (rather than inline-block) sidesteps the classic
        // inline-block whitespace-gap gotcha entirely, regardless of how the
        // generated markup happens to be formatted.
        std::string markup;
        const std::string width = std::to_string(100.0 / kColumns) + "%";
        for (int i = 0; i < kColumns; ++i)
            markup += "<div style=\"display:block; float:left; width:" + width + "; height:100%;\"></div>";
        m_sv_saturation_layer->SetInnerRML(markup);

        m_sv_saturation_columns.clear();
        m_sv_saturation_columns.reserve(kColumns);
        for (int i = 0; i < kColumns; ++i)
            m_sv_saturation_columns.push_back(m_sv_saturation_layer->GetChild(i));
    }

    // Recolour every column: white (s=0, left) fading to the fully-saturated
    // hue colour (s=1, right) -- the horizontal half of the classic
    // two-gradient SV-square technique (see the class doc comment for why
    // this is discrete columns rather than a `linear-gradient` decorator).
    const Color hue_color = HsvaToColor(m_hue, 1.0f, 1.0f, 255);
    for (int i = 0; i < kColumns; ++i)
    {
        Rml::Element* column = m_sv_saturation_columns[static_cast<std::size_t>(i)];
        if (!column)
            continue;
        const float t = static_cast<float>(i) / static_cast<float>(kColumns - 1);
        const auto lerp_channel = [t](std::uint8_t hue_channel)
        { return static_cast<std::uint8_t>(255.0f + t * (static_cast<float>(hue_channel) - 255.0f)); };
        const std::uint8_t r = lerp_channel(hue_color.r);
        const std::uint8_t g = lerp_channel(hue_color.g);
        const std::uint8_t b = lerp_channel(hue_color.b);
        column->SetProperty("background-color",
                            "rgba(" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ",255)");
    }
}

void ColorPickerPopup::BuildValueOverlayRows()
{
    if (!m_sv_value_layer)
        return;
    // The vertical half of the classic two-gradient SV-square technique:
    // transparent at the top (full value) fading to opaque black at the
    // bottom (value 0), alpha-blended over RefreshSvGradient's saturation
    // columns beneath. Doesn't depend on hue, so this is built once here
    // rather than every RefreshSvGradient call.
    constexpr int kRows = 48;
    std::string markup;
    const std::string height = std::to_string(100.0 / kRows) + "%";
    for (int i = 0; i < kRows; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kRows - 1);
        const int alpha = static_cast<int>(t * 255.0f);
        markup += "<div style=\"display:block; width:100%; height:" + height + "; background-color:rgba(0,0,0," +
                  std::to_string(alpha) + ");\"></div>";
    }
    m_sv_value_layer->SetInnerRML(markup);
}

void ColorPickerPopup::BuildHueGradientRows()
{
    if (!m_hue_gradient)
        return;
    // The same discrete-strip technique as the SV-square's layers above,
    // stepping hue 0->360 top to bottom to approximate the rainbow
    // `linear-gradient` this strip used to declare directly.
    constexpr int kRows = 72;
    std::string markup;
    const std::string height = std::to_string(100.0 / kRows) + "%";
    for (int i = 0; i < kRows; ++i)
    {
        const float hue = (static_cast<float>(i) / static_cast<float>(kRows)) * 360.0f;
        markup += "<div style=\"display:block; width:100%; height:" + height + "; background-color:" + HueToHex(hue) +
                  ";\"></div>";
    }
    m_hue_gradient->SetInnerRML(markup);
}

void ColorPickerPopup::HandleSvPointer(float window_x, float window_y)
{
    if (!m_sv_square)
        return;
    const Rml::Vector2f offset = m_sv_square->GetAbsoluteOffset();
    const Rml::Vector2f size = m_sv_square->GetBox().GetSize();
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;

    m_saturation = std::clamp((window_x - offset.x) / size.x, 0.0f, 1.0f);
    m_value = std::clamp(1.0f - (window_y - offset.y) / size.y, 0.0f, 1.0f);
    RefreshFromHsva();
}

void ColorPickerPopup::HandleHuePointer(float window_y)
{
    if (!m_hue_strip)
        return;
    const Rml::Vector2f offset = m_hue_strip->GetAbsoluteOffset();
    const Rml::Vector2f size = m_hue_strip->GetBox().GetSize();
    if (size.y <= 0.0f)
        return;

    m_hue = std::clamp((window_y - offset.y) / size.y, 0.0f, 1.0f) * 360.0f;
    RefreshFromHsva();
}

void ColorPickerPopup::HandleHexCommit()
{
    if (!m_hex_input)
        return;
    try
    {
        const Color color{std::string_view{m_hex_input->GetValue()}};
        ColorToHsv(color, m_hue, m_saturation, m_value);
        m_alpha = color.a;
        if (m_alpha_slider)
            m_alpha_slider->SetValue(std::to_string(static_cast<int>(m_alpha)));
        RefreshIndicators();
        if (m_on_pick)
            m_on_pick(color);
    }
    catch (const std::invalid_argument&)
    {
        m_hex_input->SetClass("invalid", true);
        return;
    }
    m_hex_input->SetClass("invalid", false);
}

} // namespace psr
