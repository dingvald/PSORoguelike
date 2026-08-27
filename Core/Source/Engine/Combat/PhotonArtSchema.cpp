#include "Engine/Combat/PhotonArtSchema.h"

#include "Engine/Combat/PhotonArt.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureEnumRegistered / EnsureValueTypeRegistered

#include <entt/entt.hpp>

namespace psr {

namespace {

    template <typename T> std::vector<FieldSchema> ReflectFields(entt::meta_ctx& ctx)
    {
        return detail::EnsureValueTypeRegistered<T>(ctx);
    }

} // namespace

PhotonArtSchemaModel BuildPhotonArtSchemaModel()
{
    entt::meta_ctx ctx;

    PhotonArtSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(FieldSchema{"pp_cost", FieldKind::Integer});

    FieldSchema targeting_mode{"targeting_mode", FieldKind::Enum};
    targeting_mode.enum_values = detail::EnsureEnumRegistered<TargetingMode>(ctx);
    model.fields.push_back(std::move(targeting_mode));

    FieldSchema range_shape{"range_shape", FieldKind::Enum};
    range_shape.enum_values = detail::EnsureEnumRegistered<WeaponRangeShape>(ctx);
    model.fields.push_back(std::move(range_shape));

    model.fields.push_back(FieldSchema{"range", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"hits_per_turn", FieldKind::Integer});

    FieldSchema effect_family{"effect_family", FieldKind::Enum};
    effect_family.enum_values = detail::EnsureEnumRegistered<EffectFamily>(ctx);
    model.fields.push_back(std::move(effect_family));

    model.fields.push_back(FieldSchema{"drain_percent", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"status_effect_id", FieldKind::NameId});

    FieldSchema tiers{"tiers", FieldKind::Array};
    FieldSchema tier_item{"item", FieldKind::Object};
    tier_item.children = ReflectFields<PhotonArtTier>(ctx);
    tiers.children.push_back(std::move(tier_item));
    model.fields.push_back(std::move(tiers));

    return model;
}

} // namespace psr
