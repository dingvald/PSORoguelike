#include "UI/AssetReferenceFields.h"

namespace psr {

namespace {

    const std::vector<std::string> kNoReferenceFields;
    const std::vector<std::string> kPieceReferenceFields{"piece_id"};
    const std::vector<std::string> kPrefabReferenceFields{"prefab_id", "item_prefab_id", "projectile_prefab_id",
                                                          "hit_effect_prefab_id", "prefab_id_string"};
    const std::vector<std::string> kStatusEffectReferenceFields{"status_effect_id"};

} // namespace

const std::vector<std::string>& ReferenceFieldNamesFor(AssetKind kind)
{
    switch (kind)
    {
    case AssetKind::Piece:
        return kPieceReferenceFields;
    case AssetKind::Prefab:
        return kPrefabReferenceFields;
    case AssetKind::StatusEffect:
        return kStatusEffectReferenceFields;
    case AssetKind::Affix:
    case AssetKind::Area:
    case AssetKind::Dungeon:
    case AssetKind::PhotonArt:
    case AssetKind::Technique:
        return kNoReferenceFields;
    }
    return kNoReferenceFields;
}

} // namespace psr
