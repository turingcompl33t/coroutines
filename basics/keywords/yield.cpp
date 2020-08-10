// yield.cpp
// Introduction to the co_yield keyword.

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <stdcoro/coroutine.hpp>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

class resumable
{
public:
    struct promise_type;
    using coro_handle = stdcoro::coroutine_handle<promise_type>;
    
    explicit resumable(coro_handle handle_) 
        : handle{handle_} {}
    
    ~resumable()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    resumable(resumable const&)            = delete;
    resumable& operator=(resumable const&) = delete;

    resumable(resumable&& r) 
        : handle{r.handle}
    {
        r.handle = nullptr;
    }

    resumable& operator=(resumable&& r)
    {
        if (std::addressof(r) != this)
        {
            if (handle)
            {
                handle.destroy();
            }

            handle   = r.handle;
            r.handle = nullptr;
        }

        return *this;
    }

    bool resume()
    {
        if (!handle.done())
        {
            handle.resume();
        }
        return !handle.done();
    }

    const char* recent_val();

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = stdcoro::coroutine_handle<promise_type>;

    char const* string = nullptr;

    resumable get_return_object()
    {
        return resumable{coro_handle::from_promise(*this)};
    }

    auto initial_suspend()
    {
        return stdcoro::suspend_always{};
    }

    auto final_suspend()
    {
        return stdcoro::suspend_always{};
    }

    auto yield_value(char const* string_)
    {
        string = string_;
        return stdcoro::suspend_always{};
    }

    void unhandled_exception()
    {
        std::terminate();
    }
};

const char* resumable::recent_val()
{
    return handle.promise().string;
}

resumable foo()
{
    trace("enter");

    for (;;)
    {
        co_yield "Hello";
        co_yield "co_yield";
    }

    // never reached
    trace("exit");
}

int main()
{
    auto res = foo();
    for (size_t i = 0; i < 10; ++i)
    {
        res.resume();
        printf("%s\n", res.recent_val());
    }

    return EXIT_SUCCESS;
}
