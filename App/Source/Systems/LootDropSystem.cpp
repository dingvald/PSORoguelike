#include "Systems/LootDropSystem.h"

#include "Components/CurrencyPickupComponent.h"
#include "Components/DropTableComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Items/DropTableRoller.h"
#include "Messages/LootDropMessage.h"

#include <entt/core/hashed_string.hpp>

#include <optional>
#include <string>

namespace psr {

namespace {
    constexpr const char* kMesetaPrefabId = "meseta";
} // namespace

LootDropSystem::LootDropSystem(Registry& registry, Grid& grid, MessageBus& message_bus, std::mt19937& rng)
    : m_registry(&registry), m_grid(&grid), m_message_bus(&message_bus), m_rng(&rng)
{
}

void LootDropSystem::Subscribe(Entity player)
{
    EventHandlerComponent& events = player.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, LootDropSystem>([this](Entity actor, AfterDamageEvent& event)
                                                       { OnDamage(actor, event); });
}

void LootDropSystem::OnDamage(Entity /*player*/, AfterDamageEvent& event)
{
    if (!event.target_defeated)
        return;

    const DropTableComponent* table = event.target.TryGet<DropTableComponent>();
    if (!table)
        return;

    const Position* target_position = event.target.TryGet<Position>();
    if (!target_position)
        return;

    const DropTableResult result = Roll(*table, *m_rng);
    if (result.kind == DropTableResult::Kind::None)
        return;

    const std::uint32_t item_prefab_id = result.kind == DropTableResult::Kind::Item
                                             ? result.item_prefab_id
                                             : entt::hashed_string::value(kMesetaPrefabId);

    if (!m_registry->HasPrefab(item_prefab_id))
        return;

    const entt::entity item = m_registry->CreateEntity(item_prefab_id);
    m_registry->Emplace<Position>(item, Position{target_position->tile});

    if (result.kind == DropTableResult::Kind::Meseta)
        m_registry->GetComponent<CurrencyPickupComponent>(item).amount = result.meseta;

    m_grid->AddEntity(target_position->tile, item);

    const std::optional<std::string> label = NameIdRegistry::Find(item_prefab_id);
    m_message_bus->Publish(LootDropMessage{label ? *label : std::string("an item")});
}

} // namespace psr
