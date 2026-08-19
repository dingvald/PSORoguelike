#include "Engine/Application.h"

#include "Backends/RmlUi_Platform_SDL.h"
#include "Backends/RmlUi_Renderer_SDL.h"
#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Layer.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include <SDL3/SDL.h>

namespace psr {

struct Application::Impl
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // Raw owning pointers: these are RmlUi interfaces whose lifetime must be
    // tied to Rml::Shutdown() ordering (see Shutdown()), not to RAII smart
    // pointer destruction order.
    SystemInterface_SDL* gui_system_interface = nullptr;
    RenderInterface_SDL* gui_render_interface = nullptr;

    bool sdl_initialized = false;
    bool rml_initialized = false;
    bool running = false;
};

Application::Application() : m_impl(std::make_unique<Impl>()) {}

Application::~Application() { Shutdown(); }

bool Application::Initialize(ApplicationInitContext context)
{
    Impl& s = *m_impl;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    s.sdl_initialized = true;

    s.window =
        SDL_CreateWindow(context.window_title, context.initial_width, context.initial_height, SDL_WINDOW_RESIZABLE);

    if (!s.window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // Plain SDL_Renderer, driver auto-selected -- this scaffold has no
    // custom GPU tile pipeline yet, so there's no need to create our own
    // SDL_GPUDevice/Vulkan renderer (see UnnamedRoguelike's Application for
    // that fuller shape once a tile-rendering milestone lands here).
    s.renderer = SDL_CreateRenderer(s.window, nullptr);
    if (!s.renderer)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    // The system interface creates SDL system cursors in its constructor, so
    // it must be created only after SDL is initialized.
    s.gui_system_interface = new SystemInterface_SDL();
    s.gui_system_interface->SetWindow(s.window);
    s.gui_render_interface = new RenderInterface_SDL(s.renderer);

    Rml::SetSystemInterface(s.gui_system_interface);
    Rml::SetRenderInterface(s.gui_render_interface);

    if (!Rml::Initialise())
    {
        // SDL is still usable without RmlUi; degrade rather than fail
        // Initialize() outright. Layers simply won't get a document (see
        // Layer::LoadDocument()).
        SDL_Log("Rml::Initialise failed");
        return true;
    }
    s.rml_initialized = true;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(s.window, &width, &height);

    Rml::Context* rml_context = Rml::CreateContext("main", Rml::Vector2i(width, height));
    if (!rml_context)
        SDL_Log("Rml::CreateContext failed");
    else
        Rml::Debugger::Initialise(rml_context);
    m_gui_context.Reset(rml_context);

    return true;
}

void Application::PushLayer(std::unique_ptr<Layer> layer)
{
    layer->AttachToBus(m_message_bus);
    layer->AttachToGuiContext(m_gui_context);
    layer->AttachToApplication(*this);
    m_layer_stack.PushLayer(std::move(layer));
}

void Application::PushOverlay(std::unique_ptr<Layer> overlay)
{
    overlay->AttachToBus(m_message_bus);
    overlay->AttachToGuiContext(m_gui_context);
    overlay->AttachToApplication(*this);
    m_layer_stack.PushOverlay(std::move(overlay));
}

void Application::RequestQuit() { m_impl->running = false; }

void Application::EnqueueLayerStackChange(std::move_only_function<void()> change)
{
    m_pending_layer_stack_changes.push_back(std::move(change));
}

void Application::RemoveLayer(Layer& layer)
{
    Layer* target = &layer;
    EnqueueLayerStackChange([this, target] { m_layer_stack.RemoveLayer(target); });
}

void Application::OnEvent(Event& event)
{
    // Topmost layer (overlays first) gets the event first; stop once handled.
    for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it)
    {
        if (event.handled)
            break;
        (*it)->OnEvent(event);
    }

    if (event.handled)
        return;

    // Application's own default handling (e.g. Escape quits) only applies
    // once every layer has passed on the event.
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { return OnWindowClose(e); });
    dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return OnKeyPressed(e); });
}

bool Application::OnWindowClose(WindowCloseEvent& /*event*/)
{
    RequestQuit();
    return true;
}

bool Application::OnWindowResize(WindowResizeEvent& event)
{
    if (auto context = m_gui_context.Lock())
        context->SetDimensions(Rml::Vector2i(event.GetWidth(), event.GetHeight()));
    return false;
}

bool Application::OnKeyPressed(KeyPressedEvent& event)
{
    if (event.GetKeyCode() == SDLK_ESCAPE)
    {
        RequestQuit();
        return true;
    }
    if (event.GetKeyCode() == SDLK_F8)
    {
        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        return true;
    }
    return false;
}

int Application::Run()
{
    Impl& s = *m_impl;
    if (!s.window || !s.renderer)
    {
        SDL_Log("Application::Run() called without a successful Initialize()");
        return 1;
    }

    s.running = true;

    Uint64 previous_ticks = SDL_GetPerformanceCounter();
    const double tick_frequency = double(SDL_GetPerformanceFrequency());

    while (s.running)
    {
        // The one point in the frame no layer is mid-call and Run()'s own
        // iteration below hasn't started -- safe to mutate m_layer_stack.
        // See TransitionTo()/EnqueueLayerStackChange(). Index-based, re-reading
        // size() each pass: a change's OnAttach() may itself enqueue another
        // change here, and a cached range-for end() iterator would go stale
        // across that push_back -- silently dropping the new entry when
        // clear() below runs.
        for (std::size_t i = 0; i < m_pending_layer_stack_changes.size(); ++i)
            m_pending_layer_stack_changes[i]();
        m_pending_layer_stack_changes.clear();

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            bool consumed_by_gui = false;
            if (auto context = m_gui_context.Lock())
            {
                // RmlSDL::InputEventHandler requires a mutable reference even
                // though it does not modify the event.
                SDL_Event mutable_event = ev;
                consumed_by_gui = !RmlSDL::InputEventHandler(context, s.window, mutable_event);
            }

            if (!consumed_by_gui)
            {
                // Native pass: topmost layer first, e.g. a layer wrapping a
                // native input backend that needs full-fidelity events (text
                // input, mouse wheel, touch).
                for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it)
                {
                    if (!(*it)->OnNativeEvent(ev))
                        break;
                }
            }

            switch (ev.type)
            {
            case SDL_EVENT_QUIT:
            {
                WindowCloseEvent close_event;
                OnEvent(close_event);
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            {
                WindowResizeEvent resize_event(ev.window.data1, ev.window.data2);
                OnEvent(resize_event);
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            {
                KeyPressedEvent key_event(ev.key.key, ev.key.repeat);
                OnEvent(key_event);
                break;
            }
            case SDL_EVENT_KEY_UP:
            {
                KeyReleasedEvent key_event(ev.key.key);
                OnEvent(key_event);
                break;
            }
            default:
                break;
            }
        }

        const Uint64 current_ticks = SDL_GetPerformanceCounter();
        const float delta_time = float(double(current_ticks - previous_ticks) / tick_frequency);
        previous_ticks = current_ticks;

        for (const std::unique_ptr<Layer>& layer : m_layer_stack)
            layer->OnUpdate(delta_time);

        if (auto context = m_gui_context.Lock())
            context->Update();

        SDL_SetRenderDrawColor(s.renderer, 18, 18, 26, 255);
        SDL_RenderClear(s.renderer);

        for (const std::unique_ptr<Layer>& layer : m_layer_stack)
            layer->OnRender(s.renderer);

        if (auto context = m_gui_context.Lock())
        {
            s.gui_render_interface->BeginFrame();
            context->Render();
            s.gui_render_interface->EndFrame();
        }

        SDL_RenderPresent(s.renderer);
    }

    Shutdown();
    return 0;
}

void Application::Shutdown()
{
    Impl& s = *m_impl;

    // Layers must release their RmlUi document(s) (see ~Layer()) before
    // Rml::Shutdown() runs below.
    m_layer_stack.Clear();
    m_gui_context.Reset(nullptr);

    // Rml::Shutdown() releases geometry/textures through the render
    // interface, so it must run while the render interface is still alive.
    if (s.rml_initialized)
    {
        Rml::Shutdown();
        s.rml_initialized = false;
    }
    delete s.gui_render_interface;
    s.gui_render_interface = nullptr;
    delete s.gui_system_interface;
    s.gui_system_interface = nullptr;

    if (s.renderer)
    {
        SDL_DestroyRenderer(s.renderer);
        s.renderer = nullptr;
    }
    if (s.window)
    {
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
    }

    if (s.sdl_initialized)
    {
        SDL_Quit();
        s.sdl_initialized = false;
    }
}

} // namespace psr
