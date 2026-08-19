#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Layer.h"

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

// The engine scaffold's first real layer: loads a font + a hello-world RmlUi
// document, and creates one ECS entity as a smoke test proving Registry is
// wired up end-to-end (not just linked-but-unused). Replaced by real menu/
// game layers once gameplay milestones land.
class HelloWorldLayer : public Layer
{
public:
    HelloWorldLayer();

    void OnAttach() override;
    void OnDetach() override;

private:
    Registry m_registry;
    Rml::ElementDocument* m_document = nullptr;
};

} // namespace psr
