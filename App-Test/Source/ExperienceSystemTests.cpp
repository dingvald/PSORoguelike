#include "Systems/ExperienceSystem.h"

#include "Combat/LevelingConfig.h"
#include "Components/ExperienceRewardComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Color.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Engine/World/Grid.h"
#include "Messages/CombatLogEntryMessage.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

using namespace psr;

Entity MakeActorAt(Registry& registry, Grid& grid, Vec2 tile)
{
    entt::entity handle = registry.CreateEntity();
    Entity actor(registry, handle);
    actor.Emplace<Position>(tile);
    grid.AddEntity(tile, handle);
    return actor;
}

LevelingConfig MakeConfig(int exp_base, StatsComponent growth = {})
{
    LevelingConfig config;
    config.exp_base = exp_base;
    config.exp_growth_exponent = 1.0f; // linear: ExpRequiredForLevel(level) == exp_base * level
    config.stat_growth_per_level = growth;
    return config;
}

} // namespace

TEST_CASE("ExperienceSystem no-ops when the hit did not defeat the target", "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    FloatingTextSystem floating_text;
    LevelingConfig config = MakeConfig(100);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<ExperienceRewardComponent>(ExperienceRewardComponent{50});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/5, /*target_defeated=*/false};
    player.Dispatch(event);

    CHECK_FALSE(player.Has<LevelComponent>());
    CHECK(floating_text.Active().empty());
}

TEST_CASE("ExperienceSystem no-ops when the defeated target has no ExperienceRewardComponent", "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    FloatingTextSystem floating_text;
    LevelingConfig config = MakeConfig(100);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    CHECK_FALSE(player.Has<LevelComponent>());
    CHECK(floating_text.Active().empty());
}

TEST_CASE("ExperienceSystem does not grant EXP for a kill by a non-subscribed (non-player) source",
          "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    FloatingTextSystem floating_text;
    LevelingConfig config = MakeConfig(100);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity enemy_attacker = MakeActorAt(registry, grid, {2, 2}); // never Subscribe()'d
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<ExperienceRewardComponent>(ExperienceRewardComponent{50});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    // AfterDamageEvent is dispatched at the attacker -- an enemy landing the
    // killing blow only reaches enemy_attacker's own (unsubscribed)
    // EventHandlerComponent, never the player's.
    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    enemy_attacker.Dispatch(event);

    CHECK_FALSE(player.Has<LevelComponent>());
}

TEST_CASE("ExperienceSystem grants EXP, spawns purple floating text, and logs a line on a lethal hit",
          "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    MessageQueue log_queue;
    FloatingTextSystem floating_text;
    LevelingConfig config = MakeConfig(1000); // high threshold -- this single kill must not level up

    std::vector<std::string> log_lines;
    log_queue.RegisterHandler<CombatLogEntryMessage>([&](const CombatLogEntryMessage& m) { log_lines.push_back(m.text); });
    bus.Subscribe<CombatLogEntryMessage>(log_queue);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<ExperienceRewardComponent>(ExperienceRewardComponent{50});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    REQUIRE(player.Has<LevelComponent>());
    CHECK(player.Get<LevelComponent>().current_exp == 50);
    CHECK(player.Get<LevelComponent>().level == 1); // below the 1000 threshold -- no level-up yet

    REQUIRE(floating_text.Active().size() == 1);
    CHECK(floating_text.Active().front().text == "EXP +50");
    CHECK(floating_text.Active().front().color == Color{0x7a, 0x3f, 0xd4});

    log_queue.HandleQueuedMessages();
    REQUIRE(log_lines.size() == 1);
    CHECK(log_lines.front() == "Gained 50 EXP");
}

TEST_CASE("ExperienceSystem levels up when EXP crosses the threshold, growing stats and logging every nonzero gain",
          "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    MessageQueue log_queue;
    FloatingTextSystem floating_text;

    StatsComponent growth;
    growth.atp = 3;
    growth.ata = 2;
    growth.evp = 4;
    // mst/dfp/lck left at 0 -- only nonzero stats should produce a log line.
    LevelingConfig config = MakeConfig(10, growth); // level 2 needs 20 exp

    std::vector<std::string> log_lines;
    log_queue.RegisterHandler<CombatLogEntryMessage>([&](const CombatLogEntryMessage& m) { log_lines.push_back(m.text); });
    bus.Subscribe<CombatLogEntryMessage>(log_queue);

    Entity player = MakeActorAt(registry, grid, {0, 0});
    player.Emplace<StatsComponent>(StatsComponent{});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<ExperienceRewardComponent>(ExperienceRewardComponent{20});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    REQUIRE(player.Has<LevelComponent>());
    CHECK(player.Get<LevelComponent>().level == 2);
    CHECK(player.Get<LevelComponent>().current_exp == 20);

    const StatsComponent& stats = player.Get<StatsComponent>();
    CHECK(stats.atp == 3);
    CHECK(stats.ata == 2);
    CHECK(stats.evp == 4);
    CHECK(stats.mst == 0);
    CHECK(stats.dfp == 0);
    CHECK(stats.lck == 0);

    // One EXP text, one LEVEL UP text.
    REQUIRE(floating_text.Active().size() == 2);
    CHECK(floating_text.Active()[1].text == "LEVEL UP");
    CHECK(floating_text.Active()[1].color == Color{0xd4, 0xc9, 0x3f});

    log_queue.HandleQueuedMessages();
    // "Gained 20 EXP", "Level up 1 -> 2", "ATP +3", "ATA +2", "EVP +4" -- no
    // lines for the zero-gain stats.
    REQUIRE(log_lines.size() == 5);
    CHECK(log_lines[0] == "Gained 20 EXP");
    CHECK(log_lines[1] == "Level up 1 -> 2");
    CHECK(log_lines[2] == "ATP +3");
    CHECK(log_lines[3] == "ATA +2");
    CHECK(log_lines[4] == "EVP +4");
}

TEST_CASE("ExperienceSystem crosses multiple levels from a single big kill", "[ExperienceSystem]")
{
    Registry registry;
    Grid grid{4, 4};
    MessageBus bus;
    FloatingTextSystem floating_text;
    LevelingConfig config = MakeConfig(10); // level 2 needs 20 exp, level 3 needs 30, ...

    Entity player = MakeActorAt(registry, grid, {0, 0});
    Entity target = MakeActorAt(registry, grid, {1, 1});
    target.Emplace<ExperienceRewardComponent>(ExperienceRewardComponent{1000});

    ExperienceSystem system(registry, bus, floating_text, config);
    system.Subscribe(player);

    AfterDamageEvent event{target, /*amount=*/999, /*target_defeated=*/true};
    player.Dispatch(event);

    REQUIRE(player.Has<LevelComponent>());
    CHECK(player.Get<LevelComponent>().level > 2);
    CHECK(player.Get<LevelComponent>().current_exp == 1000);
}
