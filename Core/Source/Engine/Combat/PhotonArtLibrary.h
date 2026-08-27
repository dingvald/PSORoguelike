#pragma once

#include "Engine/Combat/PhotonArt.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace psr {

// The set of authored Photon Art definitions, loaded from
// App/Assets/Data/PhotonArts. Mirrors AffixLibrary/DungeonLibrary/PieceLibrary.
class PhotonArtLibrary
{
public:
    PhotonArtLibrary() = default;
    explicit PhotonArtLibrary(std::vector<PhotonArt> photon_arts);

    const PhotonArt* Find(std::uint32_t id) const;
    const std::vector<PhotonArt>& All() const { return m_photon_arts; }
    bool Empty() const { return m_photon_arts.empty(); }

private:
    std::vector<PhotonArt> m_photon_arts;
    std::unordered_map<std::uint32_t, std::size_t> m_by_id;
};

} // namespace psr
