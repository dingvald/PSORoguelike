#pragma once

#include "Engine/Math/Color.h"

#include <cstdint>
#include <functional>
#include <string>

namespace psr {

// Where a Color/NameId(texture) field's "Pick.../Choose..." button hands off
// to -- kept separate from FieldWidgets so it doesn't have to know about
// ColorPickerPopup/TexturePickerPopup (the owning editor layer owns both
// popups and passes their "open" entry points down through this struct).
// Shared by every schema-informed field-form builder.
struct FieldPickers
{
    std::function<void(Color, std::function<void(Color)>)> open_color_picker;
    std::function<void(std::uint32_t, std::function<void(std::uint32_t, std::string)>)> open_texture_picker;
};

} // namespace psr
