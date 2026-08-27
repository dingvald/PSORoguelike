#include "Content/KeyBindings.h"

#include "Actions/MoveAction.h"
#include "Actions/WaitAction.h"

#include <SDL3/SDL_keycode.h>

#include <memory>

namespace psr {

ActionMap<int> CreateDefaultKeyBindings(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng)
{
    ActionMap<int> map;

    map.Bind(SDLK_UP, std::make_unique<MoveAction>(grid, affixes, Vec2{0, -1}, rng));
    map.Bind(SDLK_DOWN, std::make_unique<MoveAction>(grid, affixes, Vec2{0, 1}, rng));
    map.Bind(SDLK_LEFT, std::make_unique<MoveAction>(grid, affixes, Vec2{-1, 0}, rng));
    map.Bind(SDLK_RIGHT, std::make_unique<MoveAction>(grid, affixes, Vec2{1, 0}, rng));
    map.Bind(SDLK_SPACE, std::make_unique<WaitAction>());

    return map;
}

} // namespace psr
