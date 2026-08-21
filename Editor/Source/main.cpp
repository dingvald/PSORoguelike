// Entry point for the Editor -- a second GUI executable that reuses Core's
// Application/layer stack, opening to a menu that will hand off to
// sub-editors as they're built (none exist yet -- see EditorMenuLayer).
// SDL_MAIN_HANDLED keeps our own standard main() as the entry point (we
// manage the SDL lifecycle ourselves in psr::Application).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Engine/Application.h"
#include "Layers/EditorMenuLayer.h"

int main(int /*argc*/, char** /*argv*/)
{
    SDL_SetMainReady();

    psr::ApplicationInitContext context = {
        .window_title = "Editor", .initial_width = 1280, .initial_height = 720};

    psr::Application app;
    if (!app.Initialize(context))
        return 1;

    app.PushLayer<psr::EditorMenuLayer>();

    return app.Run();
}
