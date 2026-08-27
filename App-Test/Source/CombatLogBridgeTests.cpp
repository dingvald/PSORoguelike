#include "Systems/CombatLogBridge.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/Combat/PhotonArtCastEvent.h"
#include "Engine/Combat/PhotonArtLibrary.h"
#include "Engine/Combat/TechniqueCastEvent.h"
#include "Engine/Combat/TechniqueLibrary.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/PPComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/PlayerStatusMessage.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

namespace {

psr::Entity MakePlayer(psr::Registry& registry, int hp, int tp)
{
    entt::entity handle = registry.CreateEntity();
    psr::Entity player(registry, handle);
    psr::HealthComponent health;
    health.current_hp = hp;
    health.max_hp = hp;
    player.Emplace<psr::HealthComponent>(health);
    psr::TPComponent tp_component;
    tp_component.current_tp = tp;
    tp_component.max_tp = tp;
    player.Emplace<psr::TPComponent>(tp_component);
    return player;
}

} // namespace

TEST_CASE("CombatLogBridge publishes a log entry and a fresh PlayerStatusMessage on a player-dealt hit",
          "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::TechniqueLibrary techniques;
    psr::PhotonArtLibrary photon_arts;

    psr::Entity player = MakePlayer(registry, /*hp=*/50, /*tp=*/20);
    entt::entity enemy = registry.CreateEntity();

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>(
        [&](const psr::CombatLogEntryMessage& message) { log_lines.push_back(message.text); });
    int status_updates = 0;
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>([&](const psr::PlayerStatusMessage&) { ++status_updates; });
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.Subscribe(player);

    psr::AfterDamageEvent event{psr::Entity(registry, enemy), /*amount=*/7, /*target_defeated=*/false};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 1);
    REQUIRE(log_lines[0] == "Player hit something for 7 damage");
    REQUIRE(status_updates == 1); // actor is the player -> re-publish status
}

TEST_CASE("CombatLogBridge publishes a defeat line when AfterDamageEvent reports the target defeated",
          "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::TechniqueLibrary techniques;
    psr::PhotonArtLibrary photon_arts;

    psr::Entity player = MakePlayer(registry, /*hp=*/50, /*tp=*/20);
    entt::entity enemy = registry.CreateEntity();

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>(
        [&](const psr::CombatLogEntryMessage& message) { log_lines.push_back(message.text); });
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>([](const psr::PlayerStatusMessage&) {});
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.Subscribe(player);

    psr::AfterDamageEvent event{psr::Entity(registry, enemy), /*amount=*/50, /*target_defeated=*/true};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 2);
    REQUIRE(log_lines[0] == "Player hit something for 50 damage");
    REQUIRE(log_lines[1] == "Player defeated something");
}

TEST_CASE("CombatLogBridge publishes a cast line on AfterTechniqueCastEvent, resolving the technique's name",
          "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;

    psr::Technique technique;
    technique.id = 1;
    technique.name = "Foie";
    psr::TechniqueLibrary techniques(std::vector<psr::Technique>{technique});
    psr::PhotonArtLibrary photon_arts;

    psr::Entity player = MakePlayer(registry, /*hp=*/50, /*tp=*/20);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>(
        [&](const psr::CombatLogEntryMessage& message) { log_lines.push_back(message.text); });
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>([](const psr::PlayerStatusMessage&) {});
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.Subscribe(player);

    psr::AfterTechniqueCastEvent event{technique.id};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 1);
    REQUIRE(log_lines[0] == "Player cast Foie");
}

TEST_CASE("CombatLogBridge publishes a use line on AfterPhotonArtCastEvent, resolving the art's name",
          "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;

    psr::PhotonArt art;
    art.id = 1;
    art.name = "Rising Strike";
    psr::PhotonArtLibrary photon_arts(std::vector<psr::PhotonArt>{art});
    psr::TechniqueLibrary techniques;

    psr::Entity player = MakePlayer(registry, /*hp=*/50, /*tp=*/20);

    std::vector<std::string> log_lines;
    hud_queue.RegisterHandler<psr::CombatLogEntryMessage>(
        [&](const psr::CombatLogEntryMessage& message) { log_lines.push_back(message.text); });
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>([](const psr::PlayerStatusMessage&) {});
    bus.Subscribe<psr::CombatLogEntryMessage>(hud_queue);
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.Subscribe(player);

    psr::AfterPhotonArtCastEvent event{art.id};
    player.Dispatch(event);

    hud_queue.HandleQueuedMessages();

    REQUIRE(log_lines.size() == 1);
    REQUIRE(log_lines[0] == "Player used Rising Strike");
}

TEST_CASE("CombatLogBridge::PublishPlayerStatus reports TP as the player's secondary resource", "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::TechniqueLibrary techniques;
    psr::PhotonArtLibrary photon_arts;

    psr::Entity player = MakePlayer(registry, /*hp=*/30, /*tp=*/15);

    std::optional<psr::PlayerStatusMessage> received;
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>(
        [&](const psr::PlayerStatusMessage& message) { received = message; });
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.PublishPlayerStatus();

    hud_queue.HandleQueuedMessages();

    REQUIRE(received.has_value());
    REQUIRE(received->current_hp == 30);
    REQUIRE(received->max_hp == 30);
    REQUIRE(received->has_secondary);
    REQUIRE(received->secondary_label == "TP");
    REQUIRE(received->current_secondary == 15);
}

TEST_CASE("CombatLogBridge::PublishPlayerStatus reports no secondary resource when the player has neither TP nor PP",
          "[CombatLogBridge]")
{
    psr::Registry registry;
    psr::MessageBus bus;
    psr::MessageQueue hud_queue;
    psr::TechniqueLibrary techniques;
    psr::PhotonArtLibrary photon_arts;

    entt::entity handle = registry.CreateEntity();
    psr::Entity player(registry, handle);
    psr::HealthComponent health;
    health.current_hp = 30;
    health.max_hp = 30;
    player.Emplace<psr::HealthComponent>(health);

    std::optional<psr::PlayerStatusMessage> received;
    hud_queue.RegisterHandler<psr::PlayerStatusMessage>(
        [&](const psr::PlayerStatusMessage& message) { received = message; });
    bus.Subscribe<psr::PlayerStatusMessage>(hud_queue);

    psr::CombatLogBridge bridge(registry, bus, techniques, photon_arts, player.Handle());
    bridge.PublishPlayerStatus();

    hud_queue.HandleQueuedMessages();

    REQUIRE(received.has_value());
    REQUIRE_FALSE(received->has_secondary);
}
