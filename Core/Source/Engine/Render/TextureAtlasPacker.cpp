#include "Engine/Render/TextureAtlasPacker.h"

#include <algorithm>

namespace psr {

PackedAtlasLayout PackTextureAtlas(std::vector<PackerInputRect> rects, int max_width)
{
    if (rects.empty())
        return PackedAtlasLayout{};

    int effective_max_width = max_width;
    for (const PackerInputRect& rect : rects)
        effective_max_width = std::max(effective_max_width, rect.width);

    std::stable_sort(rects.begin(), rects.end(),
                     [](const PackerInputRect& lhs, const PackerInputRect& rhs) { return lhs.height > rhs.height; });

    PackedAtlasLayout layout;
    layout.atlas_width = effective_max_width;
    layout.placements.reserve(rects.size());

    int shelf_x = 0;
    int shelf_y = 0;
    int shelf_height = 0;

    for (const PackerInputRect& rect : rects)
    {
        if (shelf_height != 0 && shelf_x + rect.width > effective_max_width)
        {
            shelf_y += shelf_height;
            shelf_x = 0;
            shelf_height = 0;
        }

        layout.placements.push_back(PackerPlacement{rect.id, shelf_x, shelf_y, rect.width, rect.height});
        shelf_x += rect.width;
        shelf_height = std::max(shelf_height, rect.height);
    }

    layout.atlas_height = shelf_y + shelf_height;
    return layout;
}

} // namespace psr
