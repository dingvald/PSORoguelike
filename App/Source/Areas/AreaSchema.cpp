#include "Areas/AreaSchema.h"

#include "Areas/Area.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureEnumRegistered

#include <entt/entt.hpp>

namespace psr {

AreaSchemaModel BuildAreaSchemaModel()
{
    entt::meta_ctx ctx;

    AreaSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(FieldSchema{"tag", FieldKind::String});
    model.fields.push_back(FieldSchema{"race_id", FieldKind::NameId});

    FieldSchema hazard{"hazard", FieldKind::Enum};
    hazard.enum_values = detail::EnsureEnumRegistered<HazardType>(ctx);
    model.fields.push_back(std::move(hazard));

    model.fields.push_back(FieldSchema{"floor_texture_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"wall_texture_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"accent_texture_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"unlock_predecessor_tag", FieldKind::String});

    FieldSchema dungeon_id_strings{"dungeon_id_strings", FieldKind::Array};
    dungeon_id_strings.children.push_back(FieldSchema{"", FieldKind::String});
    model.fields.push_back(std::move(dungeon_id_strings));

    return model;
}

} // namespace psr
