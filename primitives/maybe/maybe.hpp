// maybe.hpp
// A barebones maybe monadic typic via coroutines.

#ifndef MAYBE_H
#define MAYBE_H

#include <optional>
#include <coroutine>
#include <stdexcept>

template <typename T>
struct maybe_promise;

template <typename T>
struct maybe_awaiter;

template <typename T>
class maybe
{
public:
    using coro_handle = std::coroutine_handle<maybe_promise<T>>;

    maybe(coro_handle handle_)
        : handle{handle_}
        , value{}
    {}

    maybe(maybe const&)            = delete;
    maybe& operator=(maybe const&) = delete;

    maybe(maybe&& m) 
        : handle{m.handle}
        , value{std::move(m.value)}
    {
        m.handle = nullptr;
    }

    maybe& operator=(maybe&& m)
    {
        if (&m != this)
        {
            handle   = m.handle;
            m.handle = nullptr;

            value = std::move(m.value);
        }

        return *this;
    }

    ~maybe()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    bool has_value() const noexcept
    {
        return value.has_value();
    }

    T& value()
    {
        return value.value();
    }

    maybe_awaiter operator co_await()
    {
        return maybe_awaiter{*this};
    }

private:
    // Handle to the coroutine.
    coro_handle handle;

    // Possibly-present value.
    std::optional<T> value;
};

template <typename T>
struct maybe_promise
{
    using coro_handle = maybe::coro_handle;

    maybe<T> get_return_object()
    {
        return maybe<T>{coro_handle::from_promise(*this)};
    }

    void initial_suspend()
    {
        return std::suspend_never{};
    }

    void final_suspend()
    {
        return std::suspend_never{};
    }

    void return_value(T value)
    {

    }
};

template <typename T>
struct maybe_awaiter
{
    using coro_handle = maybe::coro_handle;

    maybe_awaiter(maybe const& m_)
        : m{m_}
    {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(coro_handle handle)
    {
        
    }

    void await_resume()
    {

    }

private:
    maybe const& m;
};

#endif // MAYBE_H