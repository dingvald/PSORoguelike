#pragma once

#include <entt/entt.hpp>

namespace psr {

class Registry;
class TechniqueLibrary;
class PhotonArtLibrary;
struct TechniquesScreenMessage;

// Resolves player's KnownTechniquesComponent and equipped weapon's
// photon_art_ids into a fully-resolved TechniquesScreenMessage for HudLayer
// to render -- same "pure function, not a method on GameplayLayer or the
// state" shape CharacterScreenSnapshot.h's BuildCharacterScreenMessage
// already uses, for the same "layers/states never reference each other"
// reason. Each TechniqueEntry's icon_path is resolved to an absolute
// filesystem path under ApplicationFilepaths::TexturesPath/"Techniques"/
// "<id_string>.png", left empty if that file doesn't exist on disk (an
// unauthored icon degrades to no <img> rather than a broken reference).
TechniquesScreenMessage BuildTechniquesScreenMessage(Registry& registry, entt::entity player,
                                                      const TechniqueLibrary& techniques,
                                                      const PhotonArtLibrary& photon_arts);

} // namespace psr
