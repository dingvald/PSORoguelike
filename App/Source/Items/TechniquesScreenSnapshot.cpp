#include "Items/TechniquesScreenSnapshot.h"

#include "ApplicationFilepaths.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueLibrary.h"
#include "Components/EquipmentComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"
#include "Messages/TechniquesScreenMessage.h"

#include <filesystem>

namespace psr {

namespace {

    // Absolute path if a "<id_string>.png" icon exists under
    // ApplicationFilepaths::TexturesPath/"Techniques", empty string
    // otherwise -- see TechniquesScreenSnapshot.h's own doc comment for why
    // this must be absolute (RmlUi's <img src> resolves relative to process
    // CWD, not the .rml's own folder).
    std::string ResolveIconPath(const std::string& id_string)
    {
        const std::filesystem::path candidate =
            ApplicationFilepaths::TexturesPath / "Techniques" / (id_string + ".png");
        if (!std::filesystem::exists(candidate))
            return {};
        return std::filesystem::absolute(candidate).string();
    }

} // namespace

TechniquesScreenMessage BuildTechniquesScreenMessage(Registry& registry, entt::entity player,
                                                      const TechniqueLibrary& techniques,
                                                      const PhotonArtLibrary& photon_arts)
{
    TechniquesScreenMessage message;

    if (const KnownTechniquesComponent* known = registry.TryGetComponent<KnownTechniquesComponent>(player))
    {
        message.techniques.reserve(known->known.size());
        for (const KnownTechniqueEntry& entry : known->known)
        {
            TechniquesScreenMessage::TechniqueEntry technique_entry;
            technique_entry.technique_id = entry.technique_id;
            technique_entry.tier = entry.tier;
            if (const Technique* technique = techniques.Find(entry.technique_id))
            {
                technique_entry.display_name = technique->name.empty() ? technique->id_string : technique->name;
                technique_entry.icon_path = ResolveIconPath(technique->id_string);
            }
            message.techniques.push_back(std::move(technique_entry));
        }
    }

    if (const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player);
        equipment && equipment->weapon != entt::null)
    {
        if (const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(equipment->weapon))
        {
            message.photon_arts.reserve(weapon->photon_art_ids.size());
            for (std::uint32_t photon_art_id : weapon->photon_art_ids)
            {
                TechniquesScreenMessage::PhotonArtEntry entry;
                entry.photon_art_id = photon_art_id;
                if (const PhotonArt* photon_art = photon_arts.Find(photon_art_id))
                    entry.display_name = photon_art->name.empty() ? photon_art->id_string : photon_art->name;
                message.photon_arts.push_back(std::move(entry));
            }
        }
    }

    return message;
}

} // namespace psr
