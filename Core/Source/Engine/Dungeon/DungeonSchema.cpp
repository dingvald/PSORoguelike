#include "Engine/Dungeon/DungeonSchema.h"

#include "Engine/Dungeon/Dungeon.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureValueTypeRegistered

#include <entt/entt.hpp>

namespace psr {

namespace {

    template <typename T> std::vector<FieldSchema> ReflectFields(entt::meta_ctx& ctx)
    {
        return detail::EnsureValueTypeRegistered<T>(ctx);
    }

} // namespace

DungeonSchemaModel BuildDungeonSchemaModel()
{
    entt::meta_ctx ctx;

    DungeonSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(FieldSchema{"area_tag", FieldKind::String});
    model.fields.push_back(FieldSchema{"room_count_min", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"room_count_max", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"loopback_count_min", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"loopback_count_max", FieldKind::Integer});

    FieldSchema pieces{"pieces", FieldKind::Array};
    FieldSchema piece_item{"item", FieldKind::Object};
    piece_item.children = ReflectFields<DungeonPieceRef>(ctx);
    pieces.children.push_back(std::move(piece_item));
    model.fields.push_back(std::move(pieces));

    model.fields.push_back(FieldSchema{"lock_count", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"unlocked_door_prefab_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"locked_door_prefab_id", FieldKind::NameId});
    model.fields.push_back(FieldSchema{"switch_prefab_id", FieldKind::NameId});

    return model;
}

} // namespace psr
