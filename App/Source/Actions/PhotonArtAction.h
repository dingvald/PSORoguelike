#pragma once

#include "Engine/Actions/IAction.h"
#include "Engine/Combat/PhotonArtLibrary.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/World/Grid.h"

#include <cstdint>
#include <random>

namespace psr {

// Casts a specific weapon-granted Photon Art (photon_art_id, resolved into
// photon_arts). Stateless w.r.t. the target -- unlike AttackAction/MoveAction
// (which take a fixed direction at construction), the target this resolves
// against comes from actor's own SelectedTargetComponent at Perform() time
// (written by TargetSelectionState just before TurnCoordinator::
// SetPendingAction dispatches this), per ActionMap's "bound actions must be
// stateless" contract -- this one just isn't bound via ActionMap at all
// (GameplayLayer constructs and dispatches it directly through
// TurnCoordinator::RequestTargeting, see the M7.2 plan's noted deviation from
// UnnamedRoguelike's SelectTargetAction).
//
// A free no-op (cost 0) if actor has no weapon equipped, the weapon doesn't
// grant photon_art_id, or actor's current TP can't afford the art's tp_cost
// -- the same TPComponent pool TechniqueAction spends, per docs/GDD.md's
// "PP vs. TP (revised -- collapsed to one pool)" section. Otherwise TP is spent and the turn is
// consumed (kPhotonArtCost) regardless of whether the cast connects --
// unlike AttackAction's free-swing-into-empty-air case, the player
// explicitly chose this target through an interactive selection flow, so
// there's no "accidental miss" to refund.
class PhotonArtAction : public IAction
{
public:
    static constexpr int kPhotonArtCost = 100;

    PhotonArtAction(Grid& grid, const PhotonArtLibrary& photon_arts, const AffixLibrary& affixes,
                    std::uint32_t photon_art_id, std::mt19937& rng);

    ActionResult Perform(Entity actor) override;

private:
    Grid* m_grid;
    const PhotonArtLibrary* m_photon_arts;
    const AffixLibrary* m_affixes;
    std::uint32_t m_photon_art_id;
    std::mt19937* m_rng;
};

} // namespace psr
