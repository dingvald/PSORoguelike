#pragma once

#include <string>
#include <vector>

namespace psr {

// The content types content editors let the user rename (one enumerator per
// *EditorLayer). Extend this and ReferenceFieldNamesFor's table together when
// a new content type gains its own editor.
enum class AssetKind
{
    Affix,
    Area,
    Dungeon,
    PhotonArt,
    Piece,
    Prefab,
    Technique,
    StatusEffect,
};

// The JSON key names, across every content type's authored files, that hold
// an id reference to an asset of `kind`. Hand-maintained -- there's no
// schema-declared target-type for FieldKind::NameId fields (see
// Core/Engine/ECS/ComponentSchema.h), so this is the single place a new
// reference field needs registering for AssetRenameCascade to find it.
const std::vector<std::string>& ReferenceFieldNamesFor(AssetKind kind);

} // namespace psr
