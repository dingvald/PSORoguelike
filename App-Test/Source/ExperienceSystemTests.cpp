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

#include <random>
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
    std::mt19937 rng{42};

    // Gains are now randomized (see GrowthCurve::EvaluateRandomGain), so
    // this only pins which stats have a nonzero `rate` (and therefore must
    // appear in the log line) vs. a zero rate (which must stay omitted,
    // since Jitter(0, rng) == 0 deterministically) -- not their exact values.
    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 10.0f};
    growth_curve.max_hp = {.rate = 20.0f};
    growth_curve.max_tp = {.rate = 5.0f};
    growth_curve.atp = {.rate = 5.0f};
    growth_curve.dfp = {.rate = 2.0f};
    growth_curve.lck = {.rate = 3.0f};
    // ata/mst/evp left at rate = 0 -- must not appear.

    psr::StatsComponent starting_stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, starting_stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text, rng);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 2);
    const std::string& level_up_line = log_lines[1];
    REQUIRE(level_up_line.starts_with("[b][c=#f6470a]Player reached level 2![/c][/b] "));
    CHECK(level_up_line.find("HP +") != std::string::npos);
    CHECK(level_up_line.find("TP +") != std::string::npos);
    CHECK(level_up_line.find("ATP +") != std::string::npos);
    CHECK(level_up_line.find("DFP +") != std::string::npos);
    CHECK(level_up_line.find("LCK +") != std::string::npos);
    CHECK(level_up_line.find("ATA") == std::string::npos);
    CHECK(level_up_line.find("MST") == std::string::npos);
    CHECK(level_up_line.find("EVP") == std::string::npos);
}

TEST_CASE("ExperienceSystem omits stats a level-up didn't move", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;
    std::mt19937 rng{42};

    psr::StatsComponent unchanged_stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};

    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 10.0f};
    growth_curve.max_hp = {.rate = 20.0f};
    // max_tp and every stat left at rate = 0 -- must stay unchanged/omitted.

    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, unchanged_stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text, rng);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 2);
    const std::string& level_up_line = log_lines[1];
    REQUIRE(level_up_line.starts_with("[b][c=#f6470a]Player reached level 2![/c][/b] HP +"));
    CHECK(level_up_line.find(", ") == std::string::npos); // exactly one stat in the gain list
}

TEST_CASE("ExperienceSystem spawns exactly one LEVEL UP floating text across a multi-level jump", "[ExperienceSystem]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::FloatingTextSystem floating_text;
    std::mt19937 rng{42};

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};

    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 10.0f};
    growth_curve.max_hp = {.rate = 10.0f};

    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/20); // crosses both levels at once

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text, rng);
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
    std::mt19937 rng{42};

    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 10.0f};

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::ExperienceSystem system(bus, growth_curve, floating_text, rng);
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
    std::mt19937 rng{42};

    psr::GrowthCurve growth_curve;
    growth_curve.xp_to_next = {.base = 100.0f};

    psr::StatsComponent stats{/*atp=*/10, /*ata=*/10, /*mst=*/10, /*dfp=*/10, /*evp=*/10, /*lck=*/5};
    psr::Entity player = MakePlayer(registry, /*hp=*/100, /*max_hp=*/100, /*tp=*/50, /*max_tp=*/50, stats);
    psr::Entity target = MakeExperienceTarget(registry, /*xp=*/10); // well short of xp_to_next

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>([&](const psr::CombatLogEntryMessage& message)
                                                          { log_lines.push_back(message.text); });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);

    psr::ExperienceSystem system(bus, growth_curve, floating_text, rng);
    system.Subscribe(player);

    psr::AfterDamageEvent event{.target = target, .amount = 10, .target_defeated = true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 1); // just the XP-gain line
    REQUIRE(floating_text.Active().empty());
}
