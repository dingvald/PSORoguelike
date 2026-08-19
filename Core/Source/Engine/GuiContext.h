#pragma once

#include <mutex>

namespace Rml {
class Context;
} // namespace Rml

namespace psr {

// Thread-safe wrapper around the Rml::Context* Application owns. RmlUi's
// Context is not internally synchronized, so any code touching it from a
// thread other than the main loop (see Layer::HandleQueuedMessages, which
// layers may call from a thread they own) must go through Lock() to
// serialize access with the main thread's Update()/Render() calls.
class GuiContext
{
public:
    GuiContext() = default;

    GuiContext(const GuiContext&) = delete;
    GuiContext& operator=(const GuiContext&) = delete;
    GuiContext(GuiContext&&) = delete;
    GuiContext& operator=(GuiContext&&) = delete;

    // RAII handle granting exclusive access to the wrapped pointer for its
    // lifetime. Keep it as short-lived as possible (e.g. a single
    // statement) to avoid serializing unrelated work behind the lock.
    class LockedAccess
    {
    public:
        Rml::Context* operator->() const { return m_context; }
        Rml::Context& operator*() const { return *m_context; }
        operator Rml::Context*() const { return m_context; }
        explicit operator bool() const { return m_context != nullptr; }

        LockedAccess(const LockedAccess&) = delete;
        LockedAccess& operator=(const LockedAccess&) = delete;
        LockedAccess(LockedAccess&&) = default;
        LockedAccess& operator=(LockedAccess&&) = default;

    private:
        friend class GuiContext;
        LockedAccess(Rml::Context* context, std::unique_lock<std::mutex>&& lock)
            : m_context(context), m_lock(std::move(lock))
        {
        }

        Rml::Context* m_context;
        std::unique_lock<std::mutex> m_lock;
    };

    // Locks the mutex, then returns a handle wrapping the current pointer;
    // the mutex is released when the handle goes out of scope.
    LockedAccess Lock()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return LockedAccess(m_context, std::move(lock));
    }

    // Replaces the wrapped pointer, e.g. once Application creates the
    // context after this member is default-constructed, or clears it back
    // to nullptr during shutdown.
    void Reset(Rml::Context* context)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_context = context;
    }

private:
    Rml::Context* m_context = nullptr;
    std::mutex m_mutex;
};

} // namespace psr
