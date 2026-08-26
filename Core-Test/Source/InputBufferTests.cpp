#include "Engine/Input/InputBuffer.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("InputBuffer Press fires immediately", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(/*initial_delay_seconds=*/0.4f, /*repeat_interval_seconds=*/0.1f);

    buffer.Press(1);

    REQUIRE(buffer.Pop() == std::optional<int>(1));
}

TEST_CASE("InputBuffer Pop only returns a fire once", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);

    REQUIRE(buffer.Pop().has_value());
    REQUIRE_FALSE(buffer.Pop().has_value());
}

TEST_CASE("InputBuffer does not repeat before the initial delay elapses", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);
    buffer.Pop();

    buffer.Update(0.2f);

    REQUIRE_FALSE(buffer.Pop().has_value());
}

TEST_CASE("InputBuffer repeats the held key after the initial delay", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);
    buffer.Pop();

    buffer.Update(0.4f);

    REQUIRE(buffer.Pop() == std::optional<int>(1));
}

TEST_CASE("InputBuffer repeats at the faster interval after the first repeat", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);
    buffer.Pop();
    buffer.Update(0.4f);
    buffer.Pop();

    buffer.Update(0.1f);

    REQUIRE(buffer.Pop() == std::optional<int>(1));
}

TEST_CASE("InputBuffer Release stops the repeat", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);
    buffer.Pop();
    buffer.Release(1);

    buffer.Update(1.0f);

    REQUIRE_FALSE(buffer.Pop().has_value());
}

TEST_CASE("InputBuffer Release of the active key hands the repeat back to the previously held key", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);
    buffer.Pop();
    buffer.Press(2);
    buffer.Pop();

    buffer.Release(2);
    buffer.Update(0.4f);

    REQUIRE(buffer.Pop() == std::optional<int>(1));
}

TEST_CASE("InputBuffer Clear drops held keys and any buffered fire", "[InputBuffer]")
{
    psr::InputBuffer<int> buffer(0.4f, 0.1f);
    buffer.Press(1);

    buffer.Clear();

    REQUIRE_FALSE(buffer.Pop().has_value());
    buffer.Update(1.0f);
    REQUIRE_FALSE(buffer.Pop().has_value());
}
