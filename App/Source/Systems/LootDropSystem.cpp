#include "Systems/LootDropSystem.h"

#include "Components/CurrencyComponent.h"
#include "Components/DropTableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Items/DropTable.h"
#include "Items/DropTableLibrary.h"
#include "Items/DropTableRoller.h"
#include "Messages/LootDropMessage.h"
#include "Messages/MesetaChangedMessage.h"

#include <optional>
#include <string>

namespace psr {

LootDropSystem::LootDropSystem(Registry& registry, Grid& grid, const DropTableLibrary& drop_tables,
                               MessageBus& message_bus, std::mt19937& rng)
    : m_registry(&registry), m_grid(&grid), m_drop_tables(&drop_tables), m_message_bus(&message_bus), m_rng(&rng)
{
}

void LootDropSystem::Subscribe(Entity player)
{
    EventHandlerComponent& events = player.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, LootDropSystem>([this](Entity actor, AfterDamageEvent& event)
                                                       { OnDamage(actor, event); });
}

void LootDropSystem::OnDamage(Entity player, AfterDamageEvent& event)
{
    if (!event.target_defeated)
        return;

    const DropTableComponent* drop_ref = event.target.TryGet<DropTableComponent>();
    if (!drop_ref)
        return;

    const DropTable* table = m_drop_tables->Find(drop_ref->drop_table_id);
    if (!table)
        return;

    const Position* target_position = event.target.TryGet<Position>();
    if (!target_position)
        return;

    SectionId section_id = SectionId::Viridia;
    if (const SectionIdComponent* section = player.TryGet<SectionIdComponent>())
        section_id = section->section_id;

    const DropTableResult result = Roll(*table, section_id, *m_rng);

    for (std::uint32_t item_prefab_id : result.item_prefab_ids)
    {
        if (item_prefab_id == 0 || !m_registry->HasPrefab(item_prefab_id))
            continue;

        const entt::entity item = m_registry->CreateEntity(item_prefab_id);
        m_registry->Emplace<Position>(item, Position{target_position->tile});
        m_grid->AddEntity(target_position->tile, item);

        const std::optional<std::string> label = NameIdRegistry::Find(item_prefab_id);
        m_message_bus->Publish(LootDropMessage{label ? *label : std::string("an item")});
    }

    if (result.meseta > 0)
    {
        CurrencyComponent& currency = player.GetOrEmplace<CurrencyComponent>();
        currency.meseta += result.meseta;
        m_message_bus->Publish(MesetaChangedMessage{currency.meseta, result.meseta});
    }
}

} // namespace psr
