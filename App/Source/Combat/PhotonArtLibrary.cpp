#include "Combat/PhotonArtLibrary.h"

namespace psr {

PhotonArtLibrary::PhotonArtLibrary(std::vector<PhotonArt> photon_arts) : m_photon_arts(std::move(photon_arts))
{
    for (std::size_t i = 0; i < m_photon_arts.size(); ++i)
        m_by_id.emplace(m_photon_arts[i].id, i);
}

const PhotonArt* PhotonArtLibrary::Find(std::uint32_t id) const
{
    auto it = m_by_id.find(id);
    return it == m_by_id.end() ? nullptr : &m_photon_arts[it->second];
}

} // namespace psr
