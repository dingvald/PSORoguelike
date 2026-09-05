#include "UI/LogMarkup.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ConvertLogMarkupToRml escapes plain text with no tags", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("Player hit something for 7 damage") == "Player hit something for 7 damage");
    REQUIRE(psr::ConvertLogMarkupToRml("A & B < C > D \"E\"") == "A &amp; B &lt; C &gt; D &quot;E&quot;");
}

TEST_CASE("ConvertLogMarkupToRml wraps a color tag in a styled span", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("for [c=#d43f3f]7[/c] damage") ==
            "for <span style=\"color:#d43f3f;\">7</span> damage");
}

TEST_CASE("ConvertLogMarkupToRml wraps bold and italic tags", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[b]bold[/b]") == "<span style=\"font-weight:bold;\">bold</span>");
    REQUIRE(psr::ConvertLogMarkupToRml("[i]italic[/i]") == "<span style=\"font-style:italic;\">italic</span>");
}

TEST_CASE("ConvertLogMarkupToRml supports properly nested tags", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[b][c=#f6470a]Level up![/c][/b]") ==
            "<span style=\"font-weight:bold;\"><span style=\"color:#f6470a;\">Level up!</span></span>");
}

TEST_CASE("ConvertLogMarkupToRml closes out-of-order tags without breaking balance", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[b][c=#f6470a]text[/b][/c]") ==
            "<span style=\"font-weight:bold;\"><span style=\"color:#f6470a;\">text</span></span>");
}

TEST_CASE("ConvertLogMarkupToRml treats a malformed color as literal text", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[c=not-a-color]text[/c]") == "[c=not-a-color]text");
}

TEST_CASE("ConvertLogMarkupToRml treats an unrecognized tag as literal text", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[glow]text[/glow]") == "[glow]text[/glow]");
}

TEST_CASE("ConvertLogMarkupToRml auto-closes an unclosed tag at the end of the string", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[b]bold forever") == "<span style=\"font-weight:bold;\">bold forever</span>");
}

TEST_CASE("ConvertLogMarkupToRml ignores a stray closing tag with nothing open", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("plain[/b]text") == "plaintext");
}

TEST_CASE("ConvertLogMarkupToRml escapes special characters inside a styled span", "[LogMarkup]")
{
    REQUIRE(psr::ConvertLogMarkupToRml("[b]A & B[/b]") == "<span style=\"font-weight:bold;\">A &amp; B</span>");
}
