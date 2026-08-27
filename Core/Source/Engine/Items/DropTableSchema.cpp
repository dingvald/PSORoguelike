#include "Engine/Items/DropTableSchema.h"

#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureValueTypeRegistered
#include "Engine/Items/DropEntry.h"

#include <entt/entt.hpp>

namespace psr {

namespace {

    template <typename T> std::vector<FieldSchema> ReflectFields(entt::meta_ctx& ctx)
    {
        return detail::EnsureValueTypeRegistered<T>(ctx);
    }

} // namespace

DropTableSchemaModel BuildDropTableSchemaModel()
{
    entt::meta_ctx ctx;

    DropTableSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});

    FieldSchema common_entries{"common_entries", FieldKind::Array};
    FieldSchema common_item{"item", FieldKind::Object};
    common_item.children = ReflectFields<DropEntry>(ctx);
    common_entries.children.push_back(std::move(common_item));
    model.fields.push_back(std::move(common_entries));

    FieldSchema rare_entries{"rare_entries", FieldKind::Array};
    FieldSchema rare_item{"item", FieldKind::Object};
    rare_item.children = ReflectFields<DropEntry>(ctx);
    rare_entries.children.push_back(std::move(rare_item));
    model.fields.push_back(std::move(rare_entries));

    model.fields.push_back(FieldSchema{"rare_chance_percent", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"boss_guaranteed_rare", FieldKind::Boolean});
    model.fields.push_back(FieldSchema{"meseta_min", FieldKind::Integer});
    model.fields.push_back(FieldSchema{"meseta_max", FieldKind::Integer});

    return model;
}

} // namespace psr
