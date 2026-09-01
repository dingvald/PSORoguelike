#include "Items/ItemDisplayName.h"

#include "Combat/Element.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Affix.h"
#include "Items/AffixLibrary.h"

#include <optional>

namespace psr {

namespace {

    const char* ElementDisplayName(Element element)
    {
        switch (element)
        {
        case Element::Fire:
            return "Fire";
        case Element::Ice:
            return "Ice";
        case Element::Lightning:
            return "Lightning";
        case Element::Light:
            return "Light";
        case Element::Dark:
            return "Dark";
        case Element::None:
            return "";
        }
        return "";
    }

} // namespace

std::string FormatItemDisplayName(const Registry& registry, entt::entity item, const AffixLibrary& affixes)
{
    std::string base_name = "an item";
    if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item))
    {
        if (const std::optional<std::string> label = NameIdRegistry::Find(prefab_id->value))
            base_name = *label;
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
        name += std::string(ElementDisplayName(weapon->element)) + " ";

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

} // namespace psr
