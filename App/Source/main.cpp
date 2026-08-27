// Entry point for the roguelike. SDL_MAIN_HANDLED keeps our own standard main()
// as the entry point (we manage the SDL lifecycle ourselves in psr::Application).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ApplicationFilepaths.h"
#include "Engine/Application.h"
#include "Layers/GameplayLayer.h"

int main(int /*argc*/, char** /*argv*/)
{
    SDL_SetMainReady();

    psr::ApplicationInitContext context = {
        .window_title = "PSORoguelike", .initial_width = 1280, .initial_height = 720};

    psr::Application app;
    if (!app.Initialize(context))
        return 1;

    // GameplayLayer::OnAttach() lets content-load/generation failures
    // propagate as exceptions (a missing/malformed file is a build-input bug,
    // not something to hide behind a black screen) -- this is the one place
    // that turns an uncaught one into a logged, clean exit instead of an OS
    // crash dialog.
    try
    {
        app.PushLayer<psr::GameplayLayer>();
        return app.Run();
    }
    catch (const std::exception& error)
    {
        SDL_Log("PSORoguelike: fatal error: %s", error.what());
        return 1;
    }
}
