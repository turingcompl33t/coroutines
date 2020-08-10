// return.cpp
// Introduction to the co_return keyword.

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

    char const* return_val();

private:
    coro_handle handle;
};

struct resumable::promise_type
{
    using coro_handle = stdcoro::coroutine_handle<promise_type>;

    char const* string;

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

    void return_value(char const* string_)
    {   
        string = string_;
    }

    void unhandled_exception()
    {
        std::terminate();
    }
};

char const* resumable::return_val()
{
    return handle.promise().string;
}

resumable foo()
{
    trace("enter");

    co_await stdcoro::suspend_always{};

    trace("exit");

    co_return "hello co_return";
}

int main()
{
    auto res = foo();
    while (res.resume());
    
    printf("%s\n", res.return_val());

    return EXIT_SUCCESS;
}
