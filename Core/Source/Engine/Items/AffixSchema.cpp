#include "Engine/Items/AffixSchema.h"

#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureEnumRegistered
#include "Engine/Items/Affix.h"

#include <entt/entt.hpp>

namespace psr {

AffixSchemaModel BuildAffixSchemaModel()
{
    entt::meta_ctx ctx;

    AffixSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});

    FieldSchema kind{"kind", FieldKind::Enum};
    kind.enum_values = detail::EnsureEnumRegistered<AffixKind>(ctx);
    model.fields.push_back(std::move(kind));

    FieldSchema stat{"stat", FieldKind::Enum};
    stat.enum_values = detail::EnsureEnumRegistered<AffixStat>(ctx);
    model.fields.push_back(std::move(stat));

    model.fields.push_back(FieldSchema{"amount", FieldKind::Integer});

    return model;
}

} // namespace psr
