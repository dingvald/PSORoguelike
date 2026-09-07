#pragma once

#include <string>

namespace psr {

// Which authored DungeonPiece is the hub -- a single hand-authored document
// (App/Assets/Data/hub.json), not a per-item content library, same shape as
// GrowthCurve.h's growth curve. GameplayLayer::TransitionToWorld builds a
// one-piece DungeonLayout from piece_id_string directly, skipping
// DungeonStitcher entirely (the hub is never procedurally generated).
struct HubDefinition
{
    std::string piece_id_string;
};

} // namespace psr
