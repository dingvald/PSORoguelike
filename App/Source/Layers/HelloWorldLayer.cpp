#include "Layers/HelloWorldLayer.h"

#include <ApplicationFilepaths.h>

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>

namespace psr {

namespace {
    std::filesystem::path FontPath = ApplicationFilepaths::FontsPath / "PixelCode-Regular.ttf";
    std::filesystem::path DocumentPath = ApplicationFilepaths::RmlDocumentsPath / "hello.rml";

    // Tag component with no data -- exists purely so Registry::Emplace/Each
    // have something concrete to exercise below.
    struct GreetingTagComponent
    {
    };
} // namespace

HelloWorldLayer::HelloWorldLayer() : Layer("HelloWorldLayer") {}

void HelloWorldLayer::OnAttach()
{
    if (!Rml::LoadFontFace(FontPath.string().c_str()))
        SDL_Log("Warning: HelloWorldLayer failed to load font '%s'", FontPath.string().c_str());

    GuiContext::LockedAccess gui_context = GetLockedGuiContext();
    m_document = gui_context->LoadDocument(DocumentPath.string().c_str());
    if (!m_document)
    {
        SDL_Log("Warning: HelloWorldLayer has no document to show");
        return;
    }
    m_document->Show();

    // ECS smoke test: proves Registry is wired up end-to-end, not just
    // linked. Remove once a real gameplay system exercises it instead.
    entt::entity entity = m_registry.CreateEntity();
    m_registry.Emplace<GreetingTagComponent>(entity);
    m_registry.Each<GreetingTagComponent>([](entt::entity) { SDL_Log("HelloWorldLayer: ECS entity alive"); });
}

void HelloWorldLayer::OnDetach()
{
    if (m_document)
    {
        m_document->Close();
        m_document = nullptr;
    }
}

} // namespace psr
