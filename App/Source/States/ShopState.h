#pragma once

#include "States/GameState.h"

namespace psr {

class AffixLibrary;
struct ShopStock;

// Modal Shop screen (Stock / Your Items), pushed by GameplayLayer's
// Space-key interaction handling when the player is standing on the hub's
// shopkeeper entity (see Hub/HubInteraction.h), popped by this state itself
// on Escape. Mirrors CharacterScreenState's shape, minus RequestClose() --
// Buy/Sell are free/instant mutations (see Items/Shop.h), not IActions,
// since nothing else can act while this screen is open regardless.
class ShopState : public GameState
{
public:
    ShopState(const ShopStock& stock, const AffixLibrary& affixes);

    GameStateId GetId() const override { return GameStateId::Shop; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    const ShopStock* m_stock;
    const AffixLibrary* m_affixes;
    bool m_close_requested = false;
};

} // namespace psr
