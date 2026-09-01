#include "Items/ItemDisplayName.h"

#include "Combat/Element.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

namespace {
psr::AffixLibrary g_no_affixes;

std::uint32_t RegisterName(const char* label)
{
    const std::uint32_t hash = entt::hashed_string::value(label);
    psr::NameIdRegistry::Register(hash, label);
    return hash;
}
} // namespace

TEST_CASE("FormatItemDisplayName falls back to a placeholder when the item has no resolvable name",
          "[ItemDisplayName]")
{
    psr::Registry registry;
    entt::entity item = registry.CreateEntity();

    REQUIRE(psr::FormatItemDisplayName(registry, item, g_no_affixes) == "an item");
}

TEST_CASE("FormatItemDisplayName returns just the base name for a non-weapon item", "[ItemDisplayName]")
{
    psr::Registry registry;
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::PrefabIdComponent>(
        item, psr::PrefabIdComponent{RegisterName("test.item_display_name.frame")});

    REQUIRE(psr::FormatItemDisplayName(registry, item, g_no_affixes) == "test.item_display_name.frame");
}

TEST_CASE("FormatItemDisplayName decorates a weapon with its element", "[ItemDisplayName]")
{
    psr::Registry registry;
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::PrefabIdComponent>(
        item, psr::PrefabIdComponent{RegisterName("test.item_display_name.element_saber")});
    psr::WeaponComponent weapon;
    weapon.element = psr::Element::Fire;
    registry.Emplace<psr::WeaponComponent>(item, weapon);

    REQUIRE(psr::FormatItemDisplayName(registry, item, g_no_affixes) == "Fire test.item_display_name.element_saber");
}

TEST_CASE("FormatItemDisplayName appends a suffix affix as 'of <name>'", "[ItemDisplayName]")
{
    psr::Affix power;
    power.id = 101;
    power.name = "Power";
    power.kind = psr::AffixKind::Suffix;
    psr::AffixLibrary affixes{{power}};

    psr::Registry registry;
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::PrefabIdComponent>(
        item, psr::PrefabIdComponent{RegisterName("test.item_display_name.suffix_saber")});
    psr::WeaponComponent weapon;
    weapon.suffix_affix_id = 101;
    registry.Emplace<psr::WeaponComponent>(item, weapon);

    REQUIRE(psr::FormatItemDisplayName(registry, item, affixes) == "test.item_display_name.suffix_saber of Power");
}

TEST_CASE("FormatItemDisplayName appends a nonzero grind level", "[ItemDisplayName]")
{
    psr::Registry registry;
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::PrefabIdComponent>(
        item, psr::PrefabIdComponent{RegisterName("test.item_display_name.grind_saber")});
    psr::WeaponComponent weapon;
    weapon.grind_level = 4;
    registry.Emplace<psr::WeaponComponent>(item, weapon);

    REQUIRE(psr::FormatItemDisplayName(registry, item, g_no_affixes) == "test.item_display_name.grind_saber +4");
}

TEST_CASE("FormatItemDisplayName combines prefix, element, base name, suffix, and grind level", "[ItemDisplayName]")
{
    psr::Affix might;
    might.id = 102;
    might.name = "Godly";
    might.kind = psr::AffixKind::Prefix;

    psr::Affix power;
    power.id = 103;
    power.name = "Power";
    power.kind = psr::AffixKind::Suffix;

    psr::AffixLibrary affixes{{might, power}};

    psr::Registry registry;
    entt::entity item = registry.CreateEntity();
    registry.Emplace<psr::PrefabIdComponent>(item,
                                              psr::PrefabIdComponent{RegisterName("test.item_display_name.saber")});
    psr::WeaponComponent weapon;
    weapon.prefix_affix_id = 102;
    weapon.element = psr::Element::Fire;
    weapon.suffix_affix_id = 103;
    weapon.grind_level = 4;
    registry.Emplace<psr::WeaponComponent>(item, weapon);

    REQUIRE(psr::FormatItemDisplayName(registry, item, affixes) ==
            "Godly Fire test.item_display_name.saber of Power +4");
}
