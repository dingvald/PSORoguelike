#include "Engine/Actions/ActionExecutor.h"

#include "Engine/Actions/IAction.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

// Records which steps ran (by name) and either succeeds outright or hands
// back a fallback -- lets tests assert ResolveAction follows a multi-step
// chain and stops at the first action that returns no fallback.
class StubAction : public psr::IAction
{
public:
    StubAction(std::string name, int cost, std::vector<std::string>& log,
               std::unique_ptr<psr::IAction> fallback = nullptr)
        : m_name(std::move(name)), m_cost(cost), m_log(log), m_fallback(std::move(fallback))
    {
    }

    psr::ActionResult Perform(psr::Entity /*actor*/) override
    {
        m_log.push_back(m_name);
        return psr::ActionResult(m_cost, std::move(m_fallback));
    }

private:
    std::string m_name;
    int m_cost;
    std::vector<std::string>& m_log;
    std::unique_ptr<psr::IAction> m_fallback;
};

} // namespace

TEST_CASE("ResolveAction returns the only action's result when it has no fallback", "[ActionExecutor]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());

    std::vector<std::string> log;
    auto action = std::make_unique<StubAction>("only", 100, log);
    psr::ActionResult result = psr::ResolveAction(*action, actor);

    REQUIRE(result.cost == 100);
    REQUIRE_FALSE(result.fallback);
    REQUIRE(log == std::vector<std::string>{"only"});
}

TEST_CASE("ResolveAction follows a multi-step fallback chain and returns only the final cost", "[ActionExecutor]")
{
    psr::Registry registry;
    psr::Entity actor(registry, registry.CreateEntity());

    std::vector<std::string> log;
    auto third = std::make_unique<StubAction>("third", 50, log);
    auto second = std::make_unique<StubAction>("second", 0, log, std::move(third));
    auto first = std::make_unique<StubAction>("first", 0, log, std::move(second));

    psr::ActionResult result = psr::ResolveAction(*first, actor);

    REQUIRE(result.cost == 50);
    REQUIRE_FALSE(result.fallback);
    REQUIRE(log == std::vector<std::string>{"first", "second", "third"});
}
