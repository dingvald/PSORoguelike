#include "Engine/ECS/ExtractDisplayString.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ExtractDisplayString strips the category prefix and capitalizes the remainder", "[ExtractDisplayString]")
{
    REQUIRE(psr::ExtractDisplayString("weapons.saber") == "Saber");
}

TEST_CASE("ExtractDisplayString strips everything up to the last dot", "[ExtractDisplayString]")
{
    REQUIRE(psr::ExtractDisplayString("test.item_display_name.frame") == "Frame");
}

TEST_CASE("ExtractDisplayString turns underscores into spaces and title-cases each word", "[ExtractDisplayString]")
{
    REQUIRE(psr::ExtractDisplayString("enemies.savage_wolf") == "Savage Wolf");
}

TEST_CASE("ExtractDisplayString returns the id unchanged (only capitalized) when it has no dot",
          "[ExtractDisplayString]")
{
    REQUIRE(psr::ExtractDisplayString("meseta") == "Meseta");
}

TEST_CASE("ExtractDisplayString returns an empty string for an empty id", "[ExtractDisplayString]")
{
    REQUIRE(psr::ExtractDisplayString("") == "");
}
