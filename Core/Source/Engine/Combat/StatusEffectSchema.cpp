#include "Engine/Combat/StatusEffectSchema.h"

#include "Engine/Combat/StatusEffect.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureEnumRegistered

#include <entt/entt.hpp>

namespace psr {

StatusEffectSchemaModel BuildStatusEffectSchemaModel()
{
    entt::meta_ctx ctx;

    StatusEffectSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});

    FieldSchema type{"type", FieldKind::Enum};
    type.enum_values = detail::EnsureEnumRegistered<StatusEffectType>(ctx);
    model.fields.push_back(std::move(type));

    model.fields.push_back(FieldSchema{"magnitude", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"duration", FieldKind::Integer});

    return model;
}

} // namespace psr
