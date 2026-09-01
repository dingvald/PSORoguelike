#include "Content/KeyBindings.h"

#include "Actions/MoveAction.h"
#include "Actions/PickupAction.h"
#include "Actions/WaitAction.h"

#include <SDL3/SDL_keycode.h>

#include <memory>

namespace psr {

ActionMap<int> CreateDefaultKeyBindings(Grid& grid, const AffixLibrary& affixes, std::mt19937& rng)
{
    ActionMap<int> map;

    // Arrow keys for movement, space for wait.
    map.Bind(SDLK_UP, std::make_unique<MoveAction>(grid, affixes, Vec2{0, -1}, rng));
    map.Bind(SDLK_DOWN, std::make_unique<MoveAction>(grid, affixes, Vec2{0, 1}, rng));
    map.Bind(SDLK_LEFT, std::make_unique<MoveAction>(grid, affixes, Vec2{-1, 0}, rng));
    map.Bind(SDLK_RIGHT, std::make_unique<MoveAction>(grid, affixes, Vec2{1, 0}, rng));
    map.Bind(SDLK_SPACE, std::make_unique<WaitAction>());
    map.Bind(SDLK_G, std::make_unique<PickupAction>(grid));

    // Numpad keys for movement, numpad 5 for wait.
    map.Bind(SDLK_KP_8, std::make_unique<MoveAction>(grid, affixes, Vec2{0, -1}, rng));
    map.Bind(SDLK_KP_9, std::make_unique<MoveAction>(grid, affixes, Vec2{1, -1}, rng));
    map.Bind(SDLK_KP_2, std::make_unique<MoveAction>(grid, affixes, Vec2{0, 1}, rng));
    map.Bind(SDLK_KP_1, std::make_unique<MoveAction>(grid, affixes, Vec2{-1, 1}, rng));
    map.Bind(SDLK_KP_7, std::make_unique<MoveAction>(grid, affixes, Vec2{-1, -1}, rng));
    map.Bind(SDLK_KP_4, std::make_unique<MoveAction>(grid, affixes, Vec2{-1, 0}, rng));
    map.Bind(SDLK_KP_6, std::make_unique<MoveAction>(grid, affixes, Vec2{1, 0}, rng));
    map.Bind(SDLK_KP_3, std::make_unique<MoveAction>(grid, affixes, Vec2{1, 1}, rng));
    map.Bind(SDLK_KP_5, std::make_unique<WaitAction>());

    return map;
}

} // namespace psr
