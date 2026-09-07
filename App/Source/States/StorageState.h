#pragma once

#include "States/GameState.h"

namespace psr {

class AffixLibrary;

// Modal Storage screen (Inventory / Storage), pushed by GameplayLayer's
// Space-key interaction handling when the player is standing on the hub's
// storage-terminal entity (see Hub/HubInteraction.h), popped by this state
// itself on Escape. Mirrors ShopState's shape exactly -- Store/Withdraw are
// free/instant mutations (see Items/Storage.h), not IActions.
class StorageState : public GameState
{
public:
    explicit StorageState(const AffixLibrary& affixes);

    GameStateId GetId() const override { return GameStateId::Storage; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    const AffixLibrary* m_affixes;
    bool m_close_requested = false;
};

} // namespace psr
