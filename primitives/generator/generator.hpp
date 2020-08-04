// generator.hpp
// A simple integer generator.

#ifndef GENERATOR_H
#define GENERATOR_H

#include <coroutine>
#include <stdexcept>

struct generator_promise;

class generator
{
public:
    using coro_handle = std::coroutine_handle<generator_promise>;

    generator(coro_handle handle_)
        : handle{handle_}
    {}

    ~generator()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    // non-copyable
    generator(generator const&)            = delete;
    generator& operator=(generator const&) = delete;

    generator(generator&& g) noexcept
        : handle{g.handle}
    {
        g.handle = nullptr;
    }

    generator& operator=(generator&& g) noexcept
    {
        if (&g != this)
        {
            handle   = g.handle;
            g.handle = nullptr;
        }

        return *this;
    }

    bool next();
    int  value();

private:
    // a handle to the associated coroutine
    coro_handle handle;
};

struct generator_promise
{
    using coro_handle = generator::coro_handle;

    generator_promise() 
        : current_value{0} {}

    generator get_return_object()
    {
        return generator{coro_handle::from_promise(*this)};
    }

    auto initial_suspend()
    {
        return std::suspend_never{};
    }

    auto final_suspend()
    {
        return std::suspend_never{};
    }

    void return_void() {}

    auto yield_value(int val)
    {
        current_value = val;
        return std::suspend_always{};
    }

    void unhandled_exception()
    {
        std::terminate();
    }

    friend class generator;

private:
    int current_value;
};

bool generator::next()
{
    handle.resume();
    return !handle.done();
}

int generator::value()
{
    auto const v = handle.promise().current_value;
    return v;
}

template <>
struct std::coroutine_traits<generator>
{
    using promise_type = generator_promise;
};

#endif // GENERATOR_H