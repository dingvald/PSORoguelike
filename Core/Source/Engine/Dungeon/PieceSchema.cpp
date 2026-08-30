#include "Engine/Dungeon/PieceSchema.h"

#include "Engine/Dungeon/DungeonPiece.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h" // detail::EnsureValueTypeRegistered

#include <entt/entt.hpp>

namespace psr {

namespace {

    // Reflect one Describable value type into its field list using a private
    // meta_ctx -- the piece path never deserialises through entt-meta (the
    // hand loader does that), so these registrations are throwaway and
    // isolated. Mirrors UnnamedRoguelike's BiomeSchema.cpp ReflectFields.
    template <typename T> std::vector<FieldSchema> ReflectFields(entt::meta_ctx& ctx)
    {
        return detail::EnsureValueTypeRegistered<T>(ctx);
    }

} // namespace

PieceSchemaModel BuildPieceSchemaModel()
{
    entt::meta_ctx ctx;

    PieceSchemaModel model;
    model.fields.push_back(FieldSchema{"name", FieldKind::String});
    model.fields.push_back(FieldSchema{"area_tag", FieldKind::String});

    FieldSchema category{"category", FieldKind::Enum};
    category.enum_values = detail::EnsureEnumRegistered<PieceCategory>(ctx);
    model.fields.push_back(std::move(category));

    FieldSchema cells{"cells", FieldKind::Array};
    FieldSchema cell_item{"item", FieldKind::Object};
    cell_item.children = ReflectFields<PieceCell>(ctx);
    cells.children.push_back(std::move(cell_item));
    model.fields.push_back(std::move(cells));

    FieldSchema sockets{"sockets", FieldKind::Array};
    FieldSchema socket_item{"item", FieldKind::Object};
    socket_item.children = ReflectFields<PieceSocket>(ctx);
    sockets.children.push_back(std::move(socket_item));
    model.fields.push_back(std::move(sockets));

    return model;
}

} // namespace psr
