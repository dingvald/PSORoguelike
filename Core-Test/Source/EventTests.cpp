#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("EventDispatcher only invokes the handler matching the event's static type", "[Event]")
{
    psr::WindowResizeEvent event(800, 600);
    psr::EventDispatcher dispatcher(event);

    bool resizeCalled = false;
    bool closeCalled = false;

    bool resizeDispatched = dispatcher.Dispatch<psr::WindowResizeEvent>(
        [&resizeCalled](psr::WindowResizeEvent&) {
            resizeCalled = true;
            return true;
        });
    bool closeDispatched = dispatcher.Dispatch<psr::WindowCloseEvent>(
        [&closeCalled](psr::WindowCloseEvent&) {
            closeCalled = true;
            return true;
        });

    REQUIRE(resizeDispatched);
    REQUIRE(resizeCalled);
    REQUIRE_FALSE(closeDispatched);
    REQUIRE_FALSE(closeCalled);
}

TEST_CASE("EventDispatcher sets handled when the matched handler returns true", "[Event]")
{
    psr::WindowCloseEvent event;
    psr::EventDispatcher dispatcher(event);

    dispatcher.Dispatch<psr::WindowCloseEvent>([](psr::WindowCloseEvent&) { return true; });

    REQUIRE(event.handled);
}

TEST_CASE("EventDispatcher leaves handled false when the matched handler returns false", "[Event]")
{
    psr::WindowCloseEvent event;
    psr::EventDispatcher dispatcher(event);

    dispatcher.Dispatch<psr::WindowCloseEvent>([](psr::WindowCloseEvent&) { return false; });

    REQUIRE_FALSE(event.handled);
}

TEST_CASE("WindowResizeEvent reports category and dimensions", "[Event]")
{
    psr::WindowResizeEvent event(1280, 720);

    REQUIRE(event.GetWidth() == 1280);
    REQUIRE(event.GetHeight() == 720);
    REQUIRE(event.IsInCategory(psr::EventCategoryApplication));
    REQUIRE_FALSE(event.IsInCategory(psr::EventCategoryInput));
}

TEST_CASE("KeyPressedEvent and KeyReleasedEvent report key code and input categories", "[Event]")
{
    psr::KeyPressedEvent pressed(65, true);
    psr::KeyReleasedEvent released(65);

    REQUIRE(pressed.GetKeyCode() == 65);
    REQUIRE(pressed.IsRepeat());
    REQUIRE(pressed.IsInCategory(psr::EventCategoryKeyboard));
    REQUIRE(pressed.IsInCategory(psr::EventCategoryInput));

    REQUIRE(released.GetKeyCode() == 65);
    REQUIRE(released.GetEventType() == psr::EventType::KeyReleased);
}
