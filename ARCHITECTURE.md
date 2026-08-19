# Architecture

Runtime design of the engine (`Core`) and how the game (`App`) builds on it. For the
directory layout and build instructions, see [README.md](README.md).

All engine code lives in `namespace psr`.

## Application

[`Application`](Core/Source/Engine/Application.h) is the single owner of the SDL window,
the SDL renderer, and RmlUi's system/render interfaces and context. It exists to sequence
three phases whose ordering is load-bearing:

1. **`Initialize(ApplicationInitContext)`** — `SDL_Init` → `SDL_CreateWindow` →
   `SDL_CreateRenderer` (plain, driver auto-selected — no custom GPU device yet; that lands
   with a future tile-rendering milestone) → construct the RmlUi SDL system/render interfaces
   → `Rml::Initialise()` → `Rml::CreateContext()`, wrapped into the `GuiContext` member. If
   RmlUi fails to initialize, SDL is left usable and layers simply get no document (see
   `Layer::GetDocument()`); the rest of `Initialize()` still returns `true`.
2. **`Run()`** — the main loop (below).
3. **`Shutdown()`** (called from `Run()`'s return path and from `~Application()`) — tears
   down in the *reverse* of a naive RAII order because RmlUi's shutdown has its own
   constraints: layers must release their documents before `m_gui_context.Reset(nullptr)`,
   and `Rml::Shutdown()` must run before the render interface is deleted (it releases
   geometry/textures through it) but after the layers no longer reference the context. The
   RmlUi system/render interfaces are therefore raw owning pointers (`new`/`delete` in
   `Application::Impl`), not `unique_ptr`s — their lifetime is pinned to `Rml::Shutdown()`
   ordering, not to member-declaration order. This is the one exception CLAUDE.md's
   ownership rules carve out, and it's commented at the declaration site.

`Application` also owns the `MessageBus` and `LayerStack`, and wires every pushed layer to
both (see [Layers](#layers-and-the-layer-stack) below) before the layer's `OnAttach()` runs.

### The main loop

Each iteration of `Run()`:

1. Drains the SDL event queue. For each `SDL_Event`:
   - **RmlUi gets first refusal**: `RmlSDL::InputEventHandler()` is tried first (through the
     locked `GuiContext`); if it consumes the event, no layer sees it.
   - **Native pass**: otherwise every layer's `OnNativeEvent(const SDL_Event&)` is called,
     topmost (last-pushed overlay) first, stopping as soon as one returns `false`. This
     exists for layers wrapping a backend that needs full platform fidelity (text
     composition, mouse wheel deltas, touch) that the semantic `Event` hierarchy below
     doesn't model.
   - **Semantic translation**: a small allow-list of SDL event types
     (`SDL_EVENT_QUIT`, `SDL_EVENT_WINDOW_RESIZED`, `SDL_EVENT_KEY_DOWN`) is converted into
     an `Event` subclass and run through `Application::OnEvent()`.
2. `Application::OnEvent()` first tries its own handlers (`OnWindowClose`,
   `OnWindowResize`, `OnKeyPressed` — e.g. Escape sets `running = false`) via
   `EventDispatcher`, then walks the layer stack topmost-first calling `Layer::OnEvent()`,
   stopping once `event.handled` is set.
3. Every layer's `OnUpdate(delta_time)` runs front-to-back, then the shared `Rml::Context`
   updates.
4. Every layer's `OnRender()` runs front-to-back, then the RmlUi render interface's
   begin/render/end frame, then `SDL_RenderPresent`.

So a frame has two independent event passes — the raw SDL native pass (RmlUi-or-layers) and
the semantic `Event` pass (Application-or-layers) — because they serve different consumers
and neither alone is sufficient for both RmlUi and gameplay input.

## Layers and the layer stack

[`Layer`](Core/Source/Engine/Layer.h) is the extension point: game-specific behavior (e.g.
[`HelloWorldLayer`](App/Source/Layers/HelloWorldLayer.h)) is a `Layer` subclass pushed onto
the `Application`. A layer gets, via `Application::PushLayer`/`PushOverlay`, a `MessageBus*`
and `GuiContext*` before `OnAttach()` runs, so `OnAttach()` is the right place to load fonts
and call `LoadDocument()`-equivalent work (through `GetLockedGuiContext()`).

**Layers never hold references to each other.** The only inter-layer channel is the message
bus (below); this keeps layers independently attachable/detachable and testable.

[`LayerStack`](Core/Source/Engine/LayerStack.h) keeps regular layers at the front of its
vector, in push order, and overlays always appended at the back. That ordering gives:
regular layers update/render before overlays (overlays draw on top), while overlays receive
events first (topmost-first iteration for events). `Clear()` detaches top-to-bottom
explicitly rather than relying on `~LayerStack()` — `Application::Shutdown()` calls it before
releasing the SDL renderer/RmlUi context those layers may depend on during `OnDetach()`.

## Events

[`Event`](Core/Source/Engine/Events/Event.h) is the semantic event base: an `EventType` enum,
an `EventCategory` bitmask, and a `handled` flag. `PSR_EVENT_CLASS_TYPE`/`PSR_EVENT_CLASS_CATEGORY`
macros generate the boilerplate per concrete event
([`ApplicationEvent.h`](Core/Source/Engine/Events/ApplicationEvent.h),
[`KeyEvent.h`](Core/Source/Engine/Events/KeyEvent.h)). `EventDispatcher` matches an `Event&`
against a concrete type and invokes a handler, OR-ing its `bool` return into `handled` — this
is how `Application::OnEvent` and any `Layer::OnEvent` override should consume events.

This hierarchy is deliberately separate from the raw `SDL_Event` passed to
`Layer::OnNativeEvent` — semantic events are engine-level and small in number (added as
gameplay needs them); native events are the platform's full event, unfiltered.

## Cross-layer messaging

Because layers can't reference each other, all cross-layer communication goes through
[`MessageBus`](Core/Source/Engine/Messages/MessageBus.h) +
[`MessageQueue`](Core/Source/Engine/Messages/MessageQueue.h), exposed to a `Layer` subclass
via the protected `Subscribe<TMessage>(handler, this)` / `Publish<TMessage>(message)` helpers.

- `MessageBus` is a thread-safe, type-indexed routing table (`type_index → MessageQueue*`).
  It holds **non-owning** pointers — every `MessageQueue` is owned by the `Layer` that reads
  it, and `Layer::~Layer()` calls `MessageBus::UnsubscribeAll()` so a destroyed layer can't
  be published to.
- `Publish<TMessage>()` looks up current subscribers under lock, then (outside the lock)
  pushes a `shared_ptr<const TMessage>` into each subscriber's queue via `Enqueue()`.
  Delivery into the queue is immediate; **handler invocation is deferred**.
- Each layer decides when to process its inbox by calling
  `Layer::HandleQueuedMessages()` — typically from its own `OnUpdate()`, but a layer that
  owns a worker thread may call it from there instead.

This is a "fire, queue, and collect later" model, not a synchronous callback: `Publish()`
never runs a subscriber's handler inline.

## GUI integration (GuiContext)

[`GuiContext`](Core/Source/Engine/GuiContext.h) wraps the single `Rml::Context*` that
`Application` creates in `Initialize()`. RmlUi's `Context` is not internally synchronized, so
any access from a thread other than the main loop must go through `GuiContext::Lock()`, which
returns an RAII `LockedAccess` (a `unique_lock` plus the pointer) that serializes against the
main thread's `Update()`/`Render()`. Callers are expected to keep the locked scope as short as
possible — a single statement — since it otherwise serializes unrelated work.

`Layer` exposes this to subclasses as `GetLockedGuiContext()`; `Application` is the only
class allowed to call the private `AttachToGuiContext()` that wires it up.

## ECS (Registry)

[`Registry`](Core/Source/Engine/ECS/Registry.h) is a thin wrapper around `entt::registry` so
call sites never touch entt's API directly: `CreateEntity`/`DestroyEntity`/`IsValid`, plus
templated `Emplace`/`GetComponent`/`TryGetComponent`/`HasComponent`/`Remove`/`Each`. Currently
deliberately minimal — no prefabs, no reflection, no save/load path. The sibling
UnnamedRoguelike project's `Registry` shows the fuller shape this is expected to grow into
(a second prefab registry loaded from JSON, `entt::meta`-based reflection, cereal-backed chunk
persistence) once those systems have an actual consumer here.

## Backends (`Core/Source/Backends`)

`RmlUi_Platform_SDL` / `RmlUi_Renderer_SDL` are vendored verbatim from RmlUi's own
"copy into your project" SDL backend (they aren't part of the installed RmlUi library),
built against SDL3 (`RMLUI_SDL_VERSION_MAJOR=3`). They have their own `.clang-tidy` that
exempts them from the project's naming/guideline checks since the code isn't ours to
restyle.

## Adding a new layer

1. Subclass `Layer`, override `OnAttach()` to register fonts / load a document via
   `GetLockedGuiContext()`, and override `OnUpdate()`/`OnRender()`/`OnEvent()` as needed.
2. In the constructor, call `Subscribe<TMessage>(&MyLayer::OnMessage, this)` for each message
   type the layer cares about; call `Publish<TMessage>(...)` wherever it needs to notify
   other layers.
3. Call `HandleQueuedMessages()` from `OnUpdate()` (or from a thread the layer owns).
4. Push it from `main.cpp` (or wherever composes the app) with
   `app.PushLayer<MyLayer>(ctorArgs...)` or `PushOverlay` if it should render on top and
   receive events first.
