#include "Items/ItemDisplayName.h"

#include "Combat/Element.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/ExtractDisplayString.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/RarityComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <optional>

namespace psr {

namespace {

    // PSO's own tier-1 elemental special names (Heat/Frost/Shock/Dim are
    // real PSO weapon-special names -- Ephinea/community-documented tiered
    // chains, e.g. Fire's is Heat -> Fire -> Flame; this project has no
    // per-weapon special tier, so every elemental weapon always uses the
    // tier-1 name) rather than the plain element word, so a fire-flavored
    // weapon reads "Heat Saber," not "Fire Saber." Light has no real PSO
    // equivalent (PSO's true elemental-damage specials are only Fire/Ice/
    // Lightning; "Dim" is actually the unrelated instant-kill special
    // chain's tier-1 name, borrowed here for Dark since it fits this
    // project's own five-element roster -- see Element.h) -- "Shine" is an
    // invented name matching the same short, tier-1-sounding cadence.
    const char* ElementalPrefixName(Element element)
    {
        switch (element)
        {
        case Element::Fire:
            return "Heat";
        case Element::Ice:
            return "Frost";
        case Element::Lightning:
            return "Shock";
        case Element::Light:
            return "Shine";
        case Element::Dark:
            return "Dim";
        case Element::None:
            return "";
        }
        return "";
    }

} // namespace

const char* ElementDescription(Element element)
{
    switch (element)
    {
    case Element::Fire:
        return "Deals bonus Fire damage and may ignite the target, resisted by its own Fire resistance.";
    case Element::Ice:
        return "Deals bonus Ice damage and may freeze the target, resisted by its own Ice resistance.";
    case Element::Lightning:
        return "Deals bonus Lightning damage and may shock the target, resisted by its own Lightning resistance.";
    case Element::Light:
        return "Deals bonus Light damage, especially potent against dark creatures and resisted by the target's own "
              "Light resistance.";
    case Element::Dark:
        return "Deals bonus Dark damage, especially potent against light-averse creatures and resisted by the "
              "target's own Dark resistance.";
    case Element::None:
        return "";
    }
    return "";
}

std::string FormatItemDisplayName(const Registry& registry, entt::entity item, const AffixLibrary& affixes)
{
    std::string base_name = "an item";
    if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item))
    {
        if (const std::optional<std::string> label = NameIdRegistry::Find(prefab_id->value))
            base_name = ExtractDisplayString(*label);
    }

    const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(item);
    if (!weapon)
        return base_name;

    std::string name;
    if (weapon->prefix_affix_id != 0)
    {
        if (const Affix* prefix = affixes.Find(weapon->prefix_affix_id))
            name += prefix->name + " ";
    }

    if (weapon->element != Element::None)
        name += std::string(ElementalPrefixName(weapon->element)) + " ";

    name += base_name;

    if (weapon->suffix_affix_id != 0)
    {
        if (const Affix* suffix = affixes.Find(weapon->suffix_affix_id))
            name += " of " + suffix->name;
    }

    if (weapon->grind_level != 0)
        name += " +" + std::to_string(weapon->grind_level);

    return name;
}

int ResolveDisplayRarity(const Registry& registry, entt::entity item)
{
    int stars = 0;
    if (const RarityComponent* rarity = registry.TryGetComponent<RarityComponent>(item))
        stars = rarity->stars;

    if (const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(item))
    {
        if (weapon->element != Element::None)
            ++stars;
        if (weapon->prefix_affix_id != 0)
            ++stars;
    }

    return stars;
}

} // namespace psr
