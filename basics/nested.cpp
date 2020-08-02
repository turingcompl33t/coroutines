// nested1.cpp

#include <cstdio>
#include <cstdlib>
#include <coroutine>
#include <stdexcept>

#define trace(s) fprintf(stderr, "[%s] %s\n", __func__, s)

template <typename T>
struct task_promise;

template <typename T>
struct task_awaiter;

template <typename T>
class task
{
public:    
    using promise_type = task_promise<T>;
    using coro_handle = std::coroutine_handle<promise_type>;

    task(coro_handle handle_)
        : handle {handle_}
    {}

    ~task()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task(task&& t) 
        : handle{t.handle}
    {
        t.handle = nullptr;
    }

    task& operator=(task&& t)
    {
        if (&t != this)
        {
            handle   = t.handle;
            t.handle = nullptr;
        }

        return *this;
    }

    task_awaiter<T> operator co_await()
    {
        return task_awaiter<T>{handle};
    }

    bool run()
    {
        if (handle)
        {
            handle.resume();
        }

        return !handle.done();
    }

private:
    coro_handle handle;
};

template <typename T>
struct task_promise
{
    using coro_handle = task<T>::coro_handle;

    task<T> get_return_object()
    {
        return task{coro_handle::from_promise(*this)};
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

    void return_value(T value_)
    {
        value = std::move(value_);
    }

    void unhandled_exception()
    {
        std::terminate();
    }

    friend struct task_awaiter<T>;

private:
    T value;
};

template <typename T>
struct task_awaiter
{
    using coro_handle = task<T>::coro_handle;

    task_awaiter(coro_handle handle_)
        : handle{handle_}
    {}

    bool await_ready()
    {
        return false;
    }

    bool await_suspend(coro_handle h)
    {
        return true;
    }

    T await_resume()
    {
        return handle.promise().value;
    }

private:
    coro_handle handle;
};

task<int> baz()
{
    trace("enter");

    co_await std::suspend_always{};

    trace("exit");

    co_return 2;
}

task<int> bar()
{
    trace("enter");

    co_await std::suspend_always{};

    trace("exit");

    co_return 1;
}

task<int> foo()
{
    trace("enter");

    auto const r1 = co_await bar();
    auto const r2 = co_await baz();

    auto const sum = r1 + r2;

    fprintf(stdout, "[%s] result: %d\n", __func__, sum);

    trace("leave");

    co_return sum;
}

int main()
{
    trace("enter");

    auto t = foo();
    while (t.run());

    trace("leave");

    return EXIT_SUCCESS;
}