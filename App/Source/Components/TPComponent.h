#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <entt/entt.hpp>

namespace psr {

// Current/max Technique Points -- the single resource pool both Technique
// (see TechniqueAction) and PhotonArt (see PhotonArtAction) spend. PP and TP
// were originally separate pools (Hunter/Ranger PP vs. Force TP) but were
// collapsed into one per docs/GDD.md's "PP vs. TP (revised -- collapsed to one pool)" section --
// same shape as HealthComponent: no regen mechanic this round, no fixed
// class-to-pool enforcement.
struct TPComponent
{
    int current_tp = 0;
    int max_tp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<TPComponent>("tp")
            .Data<&TPComponent::current_tp>("current_tp")
            .Data<&TPComponent::max_tp>("max_tp");
    }

    // Contributes this entity's current_tp to
    // BeforeTechniqueCastEvent/BeforePhotonArtCastEvent, dispatched to this
    // same entity by TechniqueAction/PhotonArtAction -- see TPComponent.cpp.
    // Wired via Registry::BindComponentEvents<TPComponent>() in
    // RegisterComponents.cpp.
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
