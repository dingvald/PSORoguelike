#include "Combat/TechniqueSchema.h"

#include "Combat/Technique.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureEnumRegistered / EnsureValueTypeRegistered

#include <entt/entt.hpp>

namespace psr {

namespace {

    template <typename T> std::vector<FieldSchema> ReflectFields(entt::meta_ctx& ctx)
    {
        return detail::EnsureValueTypeRegistered<T>(ctx);
    }

} // namespace

TechniqueSchemaModel BuildTechniqueSchemaModel()
{
    entt::meta_ctx ctx;

    TechniqueSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(FieldSchema{"tp_cost", FieldKind::Integer});

    FieldSchema element{"element", FieldKind::Enum};
    element.enum_values = detail::EnsureEnumRegistered<Element>(ctx);
    model.fields.push_back(std::move(element));

    FieldSchema targeting_mode{"targeting_mode", FieldKind::Enum};
    targeting_mode.enum_values = detail::EnsureEnumRegistered<TargetingMode>(ctx);
    model.fields.push_back(std::move(targeting_mode));

    FieldSchema range_shape{"range_shape", FieldKind::Enum};
    range_shape.enum_values = detail::EnsureEnumRegistered<WeaponRangeShape>(ctx);
    model.fields.push_back(std::move(range_shape));

    model.fields.push_back(FieldSchema{"range", FieldKind::Integer});

    FieldSchema effect_family{"effect_family", FieldKind::Enum};
    effect_family.enum_values = detail::EnsureEnumRegistered<EffectFamily>(ctx);
    model.fields.push_back(std::move(effect_family));

    model.fields.push_back(FieldSchema{"status_effect_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"status_chance_percent", FieldKind::Integer});

    FieldSchema tiers{"tiers", FieldKind::Array};
    FieldSchema tier_item{"item", FieldKind::Object};
    tier_item.children = ReflectFields<TechniqueTier>(ctx);
    tiers.children.push_back(std::move(tier_item));
    model.fields.push_back(std::move(tiers));

    return model;
}

} // namespace psr
