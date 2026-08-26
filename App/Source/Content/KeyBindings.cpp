#include "Content/KeyBindings.h"

#include "Actions/MoveAction.h"
#include "Actions/WaitAction.h"

#include <SDL3/SDL_keycode.h>

#include <memory>

namespace psr {

ActionMap<int> CreateDefaultKeyBindings(Grid& grid)
{
    ActionMap<int> map;

    map.Bind(SDLK_UP, std::make_unique<MoveAction>(grid, Vec2{0, -1}));
    map.Bind(SDLK_DOWN, std::make_unique<MoveAction>(grid, Vec2{0, 1}));
    map.Bind(SDLK_LEFT, std::make_unique<MoveAction>(grid, Vec2{-1, 0}));
    map.Bind(SDLK_RIGHT, std::make_unique<MoveAction>(grid, Vec2{1, 0}));
    map.Bind(SDLK_SPACE, std::make_unique<WaitAction>());

    return map;
}

} // namespace psr
