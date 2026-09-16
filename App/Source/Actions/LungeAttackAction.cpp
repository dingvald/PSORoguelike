#include "Actions/LungeAttackAction.h"

#include "Actions/WeaponAttackAction.h"
#include "Components/BlocksMovementComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Render/VisualEffectSystem.h"

#include <memory>

namespace psr {

LungeAttackAction::LungeAttackAction(Grid& grid, const AffixLibrary& affixes, VisualEffectSystem& visual_effects,
                                     std::mt19937& rng, Vec2 direction, std::uint32_t ghost_effect_prefab_id,
                                     float ghost_effect_duration)
    : m_grid(&grid), m_affixes(&affixes), m_visual_effects(&visual_effects), m_rng(&rng), m_direction(direction),
      m_ghost_effect_prefab_id(ghost_effect_prefab_id), m_ghost_effect_duration(ghost_effect_duration)
{
}

ActionResult LungeAttackAction::Perform(Entity actor)
{
    Position& position = actor.Get<Position>();
    const Vec2 origin = position.tile;
    const Vec2 pounce_tile = origin + m_direction;

    if (!m_grid->Contains(pounce_tile))
        return ActionResult(0);

    Registry& registry = actor.GetRegistry();
    for (entt::entity occupant : m_grid->GetEntities(pounce_tile))
    {
        // Unlike MoveAction's own bump check, any blocker here is a hard
        // stop -- there's no attack to fall back into on the pounce tile
        // itself, since the intended target is kLungeRange tiles out, not
        // adjacent.
        if (registry.HasComponent<BlocksMovementComponent>(occupant))
            return ActionResult(0);
    }

    m_visual_effects->Spawn(m_ghost_effect_prefab_id, origin, m_ghost_effect_duration, EasingCurve::Linear, 200, 0);

    m_grid->RemoveEntity(origin, actor.Handle());
    m_grid->AddEntity(pounce_tile, actor.Handle());
    position.tile = pounce_tile;

    actor.GetOrEmplace<TweenComponent>().queue.push_back(
        Tween{Vec2f{static_cast<float>(origin.x - pounce_tile.x), static_cast<float>(origin.y - pounce_tile.y)},
              Vec2f{}, kPounceTweenDuration, 0.0f, nullptr});

    return ActionResult(0, std::make_unique<WeaponAttackAction>(*m_grid, *m_affixes, *m_rng, m_direction));
}

} // namespace psr
