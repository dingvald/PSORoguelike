#include "Engine/Input/ActionMap.h"

#include "Engine/Actions/ActionResult.h"
#include "Engine/ECS/Entity.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace {

// A minimal IAction that reports which instance ran -- lets tests assert on
// identity (Resolve() must hand back the exact bound instance, not a copy)
// without depending on any concrete game action.
class StubAction : public psr::IAction
{
public:
    explicit StubAction(int id) : m_id(id) {}

    psr::ActionResult Perform(psr::Entity /*actor*/) override { return psr::ActionResult(m_id); }

private:
    int m_id;
};

} // namespace

TEST_CASE("ActionMap resolves a bound key to its action", "[ActionMap]")
{
    psr::ActionMap<int> map;
    map.Bind(1, std::make_unique<StubAction>(100));

    psr::IAction* action = map.Resolve(1);
    REQUIRE(action != nullptr);
    REQUIRE(action->Perform(psr::Entity()).cost == 100);
}

TEST_CASE("ActionMap resolves an unbound key to nullptr", "[ActionMap]")
{
    psr::ActionMap<int> map;
    map.Bind(1, std::make_unique<StubAction>(100));

    REQUIRE(map.Resolve(2) == nullptr);
}

TEST_CASE("ActionMap Bind overwrites a previous binding for the same key", "[ActionMap]")
{
    psr::ActionMap<int> map;
    map.Bind(1, std::make_unique<StubAction>(100));
    map.Bind(1, std::make_unique<StubAction>(200));

    psr::IAction* action = map.Resolve(1);
    REQUIRE(action != nullptr);
    REQUIRE(action->Perform(psr::Entity()).cost == 200);
}

TEST_CASE("ActionMap supports multiple keys mapping to equivalent actions", "[ActionMap]")
{
    psr::ActionMap<int> map;
    map.Bind(1, std::make_unique<StubAction>(100));
    map.Bind(2, std::make_unique<StubAction>(100));

    REQUIRE(map.Resolve(1)->Perform(psr::Entity()).cost == 100);
    REQUIRE(map.Resolve(2)->Perform(psr::Entity()).cost == 100);
}

TEST_CASE("ActionMap Resolve hands back the exact bound instance, not a copy", "[ActionMap]")
{
    psr::ActionMap<int> map;
    auto stub = std::make_unique<StubAction>(100);
    psr::IAction* bound = stub.get();
    map.Bind(1, std::move(stub));

    REQUIRE(map.Resolve(1) == bound);
}
