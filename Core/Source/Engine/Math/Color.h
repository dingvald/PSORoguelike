#pragma once

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace psr {

namespace detail {

    constexpr std::uint8_t ColorHexNibble(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return static_cast<std::uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f')
        {
            return static_cast<std::uint8_t>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F')
        {
            return static_cast<std::uint8_t>(c - 'A' + 10);
        }
        throw std::invalid_argument("Color: invalid hex digit");
    }

    constexpr std::uint8_t ColorHexByte(char hi, char lo)
    {
        return static_cast<std::uint8_t>((ColorHexNibble(hi) << 4) | ColorHexNibble(lo));
    }

} // namespace detail

// RGBA color, 8 bits per channel. Default alpha 255 (opaque) -- a
// default-constructed Color must not silently render invisible.
struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;

    constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    // Parses "#RRGGBB" or "#RRGGBBAA" (case-insensitive); alpha defaults to
    // 255 when omitted. Constructing from a string literal makes this a
    // compile-time conversion; malformed input is therefore a compile error
    // rather than a runtime surprise. Malformed input from a runtime string
    // throws std::invalid_argument instead.
    constexpr explicit Color(std::string_view hex)
    {
        if ((hex.size() != 7 && hex.size() != 9) || hex[0] != '#')
        {
            throw std::invalid_argument("Color: expected \"#RRGGBB\" or \"#RRGGBBAA\"");
        }

        r = detail::ColorHexByte(hex[1], hex[2]);
        g = detail::ColorHexByte(hex[3], hex[4]);
        b = detail::ColorHexByte(hex[5], hex[6]);
        a = hex.size() == 9 ? detail::ColorHexByte(hex[7], hex[8]) : std::uint8_t{255};
    }

    friend bool operator==(const Color&, const Color&) = default;
};

} // namespace psr
