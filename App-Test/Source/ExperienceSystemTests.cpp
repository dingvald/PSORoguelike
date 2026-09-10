#include "Systems/ExperienceSystem.h"

#include "Components/ExperienceValueComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Progression/GrowthCurve.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

psr::Entity MakePlayer(psr::Registry& registry, int hp, int max_hp, int tp, int max_tp,
                       const psr::StatsComponent& stats)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity player(registry, handle);
    player.Emplace<psr::LevelComponent>(psr::LevelComponent{/*level=*/1, /*xp=*/0});

    psr::HealthComponent health;
    health.current_hp = hp;
    health.max_hp = max_hp;
    player.Emplace<psr::HealthComponent>(health);

    psr::TPComponent tp_component;
    tp_component.current_tp = tp;
    tp_component.max_tp = max_tp;
    player.Emplace<psr::TPComponent>(tp_component);

    player.Emplace<psr::StatsComponent>(stats);
    player.Emplace<psr::Position>(psr::Vec2{2, 3});
    return player;
}

psr::Entity MakeExperienceTarget(psr::Registry& registry, int xp)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity target(registry, handle);
    target.Emplace<psr::ExperienceValueComponent>(psr::ExperienceValueComponent{xp});
    return target;
}

} // namespace

TEST_CASE("ExperienceSystem reports only the stats that increased on level-up", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;

    psr::GrowthCurveLevel level_2;
    level_2.level = 2;
    level_2.xp_to_next = 10;
    level_2.max_hp = 120;
    level_2.max_tp = 55;
    level_2.stats = psr::StatsComponent{/*atp=*/15, /*ata=*/10, /*mst=*/10, /*dfp=*/12, /*evp=*/10, /*lck=*/8};
    psr::GrowthCurve growth_curve{{level_2}};

    psr::StatsComponent starting_stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, starting_stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 2);
    REQUIRE(log_lines[1] == "[b][c=#f6470a]Player reached level 2![/c][/b] HP +[c=#7ee787]20[/c], "
                            "TP +[c=#7ee787]5[/c], ATP +[c=#7ee787]5[/c], DFP +[c=#7ee787]2[/c], "
                            "LCK +[c=#7ee787]3[/c]");
}

TEST_CASE("ExperienceSystem omits stats a level-up didn't move", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;

    psr::StatsComponent unchanged_stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};

    psr::GrowthCurveLevel level_2;
    level_2.level = 2;
    level_2.xp_to_next = 10;
    level_2.max_hp = 120;
    level_2.max_tp = 50; // unchanged
    level_2.stats = unchanged_stats;
    psr::GrowthCurve growth_curve{{level_2}};

    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, unchanged_stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 2);
    REQUIRE(log_lines[1] == "[b][c=#f6470a]Player reached level 2![/c][/b] HP +[c=#7ee787]20[/c]");
}

TEST_CASE("ExperienceSystem spawns exactly one LEVEL UP floating text across a multi-level jump", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};

    psr::GrowthCurveLevel level_2;
    level_2.level = 2;
    level_2.xp_to_next = 10;
    level_2.max_hp = 110;
    level_2.max_tp = 50;
    level_2.stats = stats;

    psr::GrowthCurveLevel level_3;
    level_3.level = 3;
    level_3.xp_to_next = 10;
    level_3.max_hp = 120;
    level_3.max_tp = 50;
    level_3.stats = stats;

    psr::GrowthCurve growth_curve{{level_2, level_3}};

    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/20); // crosses both levels at once

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 20, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 3); // XP-gain line + one "reached level" line per level crossed
    REQUIRE(floating_text.Active().size() == 1);
    REQUIRE(floating_text.Active()[0].text == "LEVEL UP!");
    REQUIRE(floating_text.Active()[0].origin_tile == psr::Vec2{2, 3});
}

TEST_CASE("ExperienceSystem's total_xp accumulates across kills and survives a level-up", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;

    psr::GrowthCurveLevel level_2;
    level_2.level = 2;
    level_2.xp_to_next = 10;
    psr::GrowthCurve growth_curve{{level_2}};

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::ExperienceSystem system(bus, growth_curve, floating_text);
    system.Subscribe(player);

    psr::Entity first_kill = MakeExperienceTarget(registry, /*xp=*/6);
    psr::AfterDamageEvent first_event{.target = first_kill, .amount = 6, .target_defeated = true};
    player.Dispatch(first_event);

    CHECK(player.Get<psr::LevelComponent>().level == 1);
    CHECK(player.Get<psr::LevelComponent>().xp == 6);
    CHECK(player.Get<psr::LevelComponent>().total_xp == 6);

    // Second kill crosses xp_to_next (6 + 6 = 12 >= 10): xp resets to the
    // 2-point remainder, but total_xp keeps the full 12 -- it's never consumed.
    psr::Entity second_kill = MakeExperienceTarget(registry, /*xp=*/6);
    psr::AfterDamageEvent second_event{.target = second_kill, .amount = 6, .target_defeated = true};
    player.Dispatch(second_event);

    CHECK(player.Get<psr::LevelComponent>().level == 2);
    CHECK(player.Get<psr::LevelComponent>().xp == 2);
    CHECK(player.Get<psr::LevelComponent>().total_xp == 12);
}

TEST_CASE("ExperienceSystem publishes no level-up line or floating text when no level is crossed", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;

    psr::GrowthCurveLevel level_2;
    level_2.level = 2;
    level_2.xp_to_next = 100;
    psr::GrowthCurve growth_curve{{level_2}};

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10); // well short of xp_to_next

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 1); // just the XP-gain line
    REQUIRE(floating_text.Active().empty());
}
