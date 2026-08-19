// Entry point for the roguelike. SDL_MAIN_HANDLED keeps our own standard main()
// as the entry point (we manage the SDL lifecycle ourselves in psr::Application).
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "ApplicationFilepaths.h"
#include "Engine/Application.h"
#include "Layers/HelloWorldLayer.h"

int main(int /*argc*/, char** /*argv*/)
{
    SDL_SetMainReady();

    psr::ApplicationInitContext context = {
        .window_title = "PSORoguelike", .initial_width = 1280, .initial_height = 720};

    psr::Application app;
    if (!app.Initialize(context))
        return 1;

    app.PushLayer<psr::HelloWorldLayer>();

    return app.Run();
}
