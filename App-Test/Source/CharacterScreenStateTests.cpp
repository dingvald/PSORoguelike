#include "States/CharacterScreenState.h"

#include "Combat/LevelingConfig.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"
#include "Messages/CharacterScreenMessage.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <vector>

namespace {

using namespace psr;

entt::entity MakeWeapon(Registry& registry)
{
    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<StatsComponent>(weapon);
    registry.Emplace<WeaponComponent>(weapon);
    return weapon;
}

struct Fixture
{
    Registry registry;
    Grid grid{5, 5};
    AffixLibrary affixes;
    LevelingConfig leveling;
    TurnCoordinator turn_coordinator{registry};
    MessageBus message_bus;
    MessageQueue queue;
    entt::entity player = entt::null;

    Fixture()
    {
        queue.RegisterHandler<CharacterScreenMessage>([this](const CharacterScreenMessage& m) { last = m; });
        message_bus.Subscribe<CharacterScreenMessage>(queue);

        player = registry.CreateEntity();
        registry.Emplace<Position>(player, Position{Vec2{2, 2}});
        registry.Emplace<StatsComponent>(player);
        registry.Emplace<EquipmentComponent>(player, EquipmentComponent{MakeWeapon(registry)});
        registry.Emplace<InventoryComponent>(
            player, InventoryComponent{{MakeWeapon(registry), MakeWeapon(registry)}, InventoryComponent::kDefaultCapacity});
    }

    GameplayContext Context() { return GameplayContext{registry, grid, turn_coordinator, player, message_bus}; }

    // Drains the queue and returns the most recently published
    // CharacterScreenMessage's focus (a fresh Send/OnEnter always publishes
    // exactly one).
    const CharacterScreenMessage& Latest()
    {
        queue.HandleQueuedMessages();
        return last;
    }

    bool Send(CharacterScreenState& state, GameplayContext& context, int key_code)
    {
        KeyPressedEvent key_event(key_code, /*repeat=*/false);
        return state.HandleEvent(key_event, context);
    }

    CharacterScreenMessage last;
};

} // namespace

TEST_CASE("CharacterScreenState focuses the first equipment slot on OnEnter", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();

    state.OnEnter(context);

    const CharacterScreenMessage& message = fixture.Latest();
    REQUIRE(message.focus.equipment_slot.has_value());
    CHECK(*message.focus.equipment_slot == EquipmentSlot::Weapon);
    CHECK_FALSE(message.focus.inventory_index.has_value());
}

TEST_CASE("CharacterScreenState moves focus down through equipment then into inventory", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    // 5 equipment slots (0-4) -- 5 Down presses should land on inventory[0].
    for (int i = 0; i < 5; ++i)
        fixture.Send(state, context, SDLK_DOWN);

    const CharacterScreenMessage& message = fixture.Latest();
    REQUIRE(message.focus.inventory_index.has_value());
    CHECK(*message.focus.inventory_index == 0);
    CHECK_FALSE(message.focus.equipment_slot.has_value());
}

TEST_CASE("CharacterScreenState wraps focus from the last inventory row back to the first equipment slot",
          "[CharacterScreenState]")
{
    Fixture fixture; // 5 equipment slots + 2 inventory items == 7 rows total
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    for (int i = 0; i < 7; ++i)
        fixture.Send(state, context, SDLK_DOWN);

    const CharacterScreenMessage& message = fixture.Latest();
    REQUIRE(message.focus.equipment_slot.has_value());
    CHECK(*message.focus.equipment_slot == EquipmentSlot::Weapon);
}

TEST_CASE("CharacterScreenState Up wraps from the first equipment slot to the last inventory row",
          "[CharacterScreenState]")
{
    Fixture fixture; // 5 equipment slots + 2 inventory items
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    fixture.Send(state, context, SDLK_UP);

    const CharacterScreenMessage& message = fixture.Latest();
    REQUIRE(message.focus.inventory_index.has_value());
    CHECK(*message.focus.inventory_index == 1); // last of the 2 inventory items
}

TEST_CASE("CharacterScreenState WASD and numpad alias the arrow keys", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    fixture.Send(state, context, SDLK_S);
    CHECK(*fixture.Latest().focus.equipment_slot == EquipmentSlot::Head);

    fixture.Send(state, context, SDLK_KP_8);
    CHECK(*fixture.Latest().focus.equipment_slot == EquipmentSlot::Weapon);

    fixture.Send(state, context, SDLK_KP_2);
    CHECK(*fixture.Latest().focus.equipment_slot == EquipmentSlot::Head);

    fixture.Send(state, context, SDLK_W);
    CHECK(*fixture.Latest().focus.equipment_slot == EquipmentSlot::Weapon);
}

TEST_CASE("CharacterScreenState Space unequips the focused equipment slot", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context); // focused on Weapon

    const int inventory_before = static_cast<int>(fixture.registry.GetComponent<InventoryComponent>(fixture.player).items.size());

    fixture.Send(state, context, SDLK_SPACE);

    CHECK(fixture.registry.GetComponent<EquipmentComponent>(fixture.player).weapon == entt::null);
    CHECK(static_cast<int>(fixture.registry.GetComponent<InventoryComponent>(fixture.player).items.size()) ==
          inventory_before + 1);
}

TEST_CASE("CharacterScreenState Space equips the focused inventory item", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    const entt::entity original_weapon = fixture.registry.GetComponent<EquipmentComponent>(fixture.player).weapon;
    const entt::entity swapped_in = fixture.registry.GetComponent<InventoryComponent>(fixture.player).items[0];

    for (int i = 0; i < 5; ++i) // move onto inventory[0]
        fixture.Send(state, context, SDLK_DOWN);
    fixture.Send(state, context, SDLK_SPACE);

    CHECK(fixture.registry.GetComponent<EquipmentComponent>(fixture.player).weapon == swapped_in);
    const std::vector<entt::entity>& inventory = fixture.registry.GetComponent<InventoryComponent>(fixture.player).items;
    CHECK(std::find(inventory.begin(), inventory.end(), original_weapon) != inventory.end());
}

TEST_CASE("CharacterScreenState Escape/'C' requests close", "[CharacterScreenState]")
{
    Fixture fixture;
    CharacterScreenState state(fixture.affixes, fixture.leveling);
    GameplayContext context = fixture.Context();
    state.OnEnter(context);

    CHECK(state.Update(context, 0.0f).kind == StateTransitionKind::None);

    fixture.Send(state, context, SDLK_ESCAPE);

    CHECK(state.Update(context, 0.0f).kind == StateTransitionKind::Pop);
}
