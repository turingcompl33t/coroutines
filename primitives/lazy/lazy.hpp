// lazy.hpp
// A lazily-evaluated result.

#ifndef LAZY_H
#define LAZY_H

#include <optional>
#include <stdexcept>
#include <coroutine>

template <typename T>
class lazy_promise;

template <typename T>
class lazy
{

public:
    using coro_handle = std::coroutine_handle<lazy_promise<T>>;

    lazy(coro_handle handle_)
        : value{}, handle{handle_}
    {}

    ~lazy()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    T get()
    {
        if (handle)
        {
            // resume the coroutine to perform the computation
            handle.resume();

            // the coroutine is now complete, suspending at co_return;
            // move the result of the computation from the promise 
            // into the return object so it survives destruction of
            // the coroutine frame hosting the computation
            value.emplace(std::move(handle.promise().value));

            // and destroy the frame
            handle.destroy();
            handle = {};
        }

        return value.value();
    }

    friend class lazy_promise<T>;

private:    
    // the lazily-computed value
    std::optional<T> value;

    // a handle to the associated coroutine
    coro_handle handle;
};

template <typename T>
class lazy_promise
{
public:
    using coro_handle = lazy<T>::coro_handle;

    lazy<T> get_return_object()
    {
        return lazy<T>{coro_handle::from_promise(*this)};
    }

    void return_value(T val)
    {
        value = std::move(val);
    }

    auto initial_suspend()
    {
        // suspension of the coroutine at initial_suspend() is
        // the crux of performing the lazy computation
        return std::suspend_always{};
    }

    auto final_suspend()
    {
        return std::suspend_never{};
    }

    void unhandled_exception()
    {
        std::terminate();
    }

    // The lazily-computed value.
    T value;
};

template <typename T>
struct std::coroutine_traits<lazy<T>>
{
    using promise_type = lazy_promise<T>;
};

#endif // LAZY_H