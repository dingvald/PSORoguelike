#include "Combat/Hostility.h"

#include "Components/AiComponent.h"
#include "Components/FactionComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

namespace {

psr::Entity MakePlain(psr::Registry& registry) { return psr::Entity(registry, registry.CreateEntity()); }

psr::Entity MakePlayer(psr::Registry& registry)
{
    psr::Entity entity = MakePlain(registry);
    entity.Emplace<psr::PlayerControlledComponent>();
    return entity;
}

psr::Entity MakeAi(psr::Registry& registry)
{
    psr::Entity entity = MakePlain(registry);
    entity.Emplace<psr::AiComponent>();
    return entity;
}

} // namespace

TEST_CASE("GetFaction infers Player from PlayerControlledComponent", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE(psr::GetFaction(MakePlayer(registry)) == psr::Faction::Player);
}

TEST_CASE("GetFaction infers Enemy from AiComponent", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE(psr::GetFaction(MakeAi(registry)) == psr::Faction::Enemy);
}

TEST_CASE("GetFaction defaults an unmarked entity to Neutral", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE(psr::GetFaction(MakePlain(registry)) == psr::Faction::Neutral);
}

TEST_CASE("GetFaction prefers an authored FactionComponent over inference", "[Hostility]")
{
    psr::Registry registry;
    psr::Entity entity = MakeAi(registry);
    entity.Emplace<psr::FactionComponent>(psr::FactionComponent{psr::Faction::Neutral});
    REQUIRE(psr::GetFaction(entity) == psr::Faction::Neutral);
}

TEST_CASE("IsHostile: Player and Enemy are hostile", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE(psr::IsHostile(MakePlayer(registry), MakeAi(registry)));
}

TEST_CASE("IsHostile: two Player-faction entities are not hostile", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE_FALSE(psr::IsHostile(MakePlayer(registry), MakePlayer(registry)));
}

TEST_CASE("IsHostile: an unmarked (Neutral) entity is not hostile to the player", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE_FALSE(psr::IsHostile(MakePlayer(registry), MakePlain(registry)));
}

TEST_CASE("IsHostile: two Enemy-faction entities do not infight by default", "[Hostility]")
{
    psr::Registry registry;
    REQUIRE_FALSE(psr::IsHostile(MakeAi(registry), MakeAi(registry)));
}

TEST_CASE("IsHostile: an Ally-faction entity is hostile to Enemy but not Player", "[Hostility]")
{
    psr::Registry registry;
    psr::Entity ally = MakePlain(registry);
    ally.Emplace<psr::FactionComponent>(psr::FactionComponent{psr::Faction::Ally});

    REQUIRE(psr::IsHostile(ally, MakeAi(registry)));
    REQUIRE_FALSE(psr::IsHostile(ally, MakePlayer(registry)));
}
