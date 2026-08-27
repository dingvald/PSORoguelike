#include "Systems/LootDropSystem.h"

#include "Components/GroundItemComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/DropTableComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/MesetaComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SectionIdComponent.h"
#include "Engine/Items/DropResolution.h"

namespace psr {

LootDropSystem::LootDropSystem(Registry& registry, Grid& grid, const DropTableLibrary& drop_tables, std::mt19937& rng)
    : m_registry(&registry), m_grid(&grid), m_drop_tables(&drop_tables), m_rng(&rng)
{
    registry.OnDestroy<entt::entity, &LootDropSystem::OnEntityDestroyed>(*this);
}

LootDropSystem::~LootDropSystem() { m_registry->DisconnectComponentLifecycle<entt::entity>(*this); }

void LootDropSystem::OnEntityDestroyed(entt::registry& runtime_registry, entt::entity entity)
{
    Registry& registry = Registry::FromEntt(runtime_registry);

    // Fires for every entity destruction in the game, not just combat deaths
    // (see the class doc comment for why this signal was chosen over a
    // HealthComponent-specific one) -- only a HealthComponent-bearing entity
    // with a DropTableComponent rolls loot; everything else is a cheap no-op.
    if (!registry.HasComponent<HealthComponent>(entity))
        return;

    const DropTableComponent* drop_ref = registry.TryGetComponent<DropTableComponent>(entity);
    if (!drop_ref || drop_ref->drop_table_id == 0)
        return;
    const DropTable* table = m_drop_tables->Find(drop_ref->drop_table_id);
    if (!table)
        return;

    // Single-player (per docs/GDD.md's scope decision): whichever entity
    // carries both SectionIdComponent and PlayerControlledComponent is the
    // roller. Stays SectionId::None (unbiased) if the player somehow has no
    // SectionIdComponent yet.
    SectionId roller_section = SectionId::None;
    registry.Each<SectionIdComponent, PlayerControlledComponent>(
        [&roller_section](entt::entity, SectionIdComponent& section) { roller_section = section.section_id; });

    const DropResult result = ResolveDrop(*table, roller_section, *m_rng);

    if (result.meseta > 0)
        registry.Each<MesetaComponent, PlayerControlledComponent>(
            [&result](entt::entity, MesetaComponent& meseta) { meseta.amount += result.meseta; });

    if (result.item_prefab_id != 0 && registry.HasPrefab(result.item_prefab_id))
    {
        if (const Position* position = registry.TryGetComponent<Position>(entity))
        {
            const entt::entity item = registry.CreateEntity(result.item_prefab_id);
            if (item != entt::null)
            {
                registry.Emplace<Position>(item, Position{position->tile});
                registry.Emplace<GroundItemComponent>(item);
                m_grid->AddEntity(position->tile, item);
            }
        }
    }
}

} // namespace psr
