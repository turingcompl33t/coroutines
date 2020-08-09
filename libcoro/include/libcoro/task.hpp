// task.hpp
// A lazily-computed asynchronous computation.
//
// Adapted / simplified from the implementation in CppCoro:
// https://github.com/lewissbaker/cppcoro

#ifndef CORO_TASK_HPP
#define CORO_TASK_HPP

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <utility>
#include <exception>
#include <stdexcept>
#include <coroutine>
#include <type_traits>

namespace coro
{
    template <typename T>
    class task;

    class task_promise_base
    {
        friend struct final_awaitable;

        // The awaitable type returned by final_suspend() 
        // for the task_promise type.
        struct final_awaitable
        {
            bool await_ready() const noexcept
            {
                // The final_awaitable unconditionally returns "not ready"
                // such that the logic below in await_suspend() is executed
                return false;
            }

            template <typename Promise>
            void await_suspend(std::coroutine_handle<Promise> coro_handle)
            {
                // Acquire a reference to the promise for the awaiting coroutine.
                task_promise_base& promise = coro_handle.promise();

                // Atomically update the state of the promise to `true`.
                // An atomic exchange operation returns the previously stored
                // value, so the conditional evaluates in the event that the
                // promise state was already in the `true` state.
                if (promise.state.exchange(true, std::memory_order_acq_rel))
                {
                    // Resume execution of the continuation coroutine.
                    promise.continuation_handle.resume();
                }
            }

            void await_resume() noexcept {}
        };

    public:
        task_promise_base() noexcept 
            : state{false} {}

        auto initial_suspend() noexcept
        {
            // The promise type for a task unconditionally suspends at 
            // initial_suspend() in order to perform the requested
            // computation lazily; that is, only when the task<> is eventually
            // awaited upon will the requested computation actually take place
            return std::suspend_always{};
        }

        auto final_suspend() noexcept
        {
            // See above.
            return final_awaitable{};
        }

        // Store a handle to the coroutine that should be resumed
        // upon completion of the coroutine controlled by this promise.
        bool try_set_continuation(std::coroutine_handle<> continuation_handle_)
        {
            // Set the continuation handle unconditionally.
            continuation_handle = continuation_handle_;
            
            // An atomic exchange operation atomically updates the value
            // of `state` to `true` to reflect the fact that the promise 
            // now possesses a valid continuation, and simultaneously 
            // returns the previous result that was stored in `state`. 
            //
            // If `state` was previously `false`, then prior to this call
            // the promise did not possess a valid continuation coroutine.
            auto const no_previous_continuation 
                = !state.exchange(true, std::memory_order_acq_rel);
            return no_previous_continuation;
        }

    private:
        // A handle to the coroutine that should be resumed.
        std::coroutine_handle<> continuation_handle;

        // The state of this promise; `true` if a continuation is 
        // set for the awaiting coroutine, `false` otherwise.
        std::atomic_bool state;
    };

    // The promise type for tasks that return values.
    template <typename T>
    class task_promise final 
        : public task_promise_base
    {
    public:
        task_promise() noexcept {}

        ~task_promise()
        {
            switch (result_type)
            {
            case ResultType::value:
                value.~T();
                break;
            case ResultType::exception:
                exception.~exception_ptr();
                break;
            default:
                break;
            }
        }

        task<T> get_return_object() noexcept;

        void unhandled_exception() noexcept
        {
            // Store the exception pointer for unhandled exception in the promise object.
            ::new (static_cast<void*>(std::addressof(exception))) 
                std::exception_ptr{std::current_exception()};
            
            result_type = ResultType::exception;
        }

        template <
                typename Value, 
                typename = std::enable_if<std::is_convertible_v<Value&&, T>>>
        void return_value(Value&& value)
        {
            // Store the value returned by the coroutine in the promise object.
            ::new (static_cast<void*>(std::addressof(value))) 
                T{std::forward<Value>(value)};

            result_type = ResultType::value;
        }

        // Retrieve the result of the coroutine.
        T& result()
        {
            if (ResultType::exception == result_type)
            {
                std::rethrow_exception(exception);
            }

            assert(ResultType::value == result_type);

            return value;
        }

    private:
        enum class ResultType
        {
            empty,
            value,
            exception
        };

        // Reflects whether the task resulted in a valid value (ResultType::value)
        // or encountered some exceptional condition (ResultType::exception).
        ResultType result_type{ResultType::empty};

        union 
        {
            T value;
            std::exception_ptr exception;
        };
    };

    // The promise type for tasks that do not return values.
    template <>
    class task_promise<void> final 
        : public task_promise_base
    {
    public:
        task_promise() noexcept = default;

        task<void> get_return_object() noexcept;

        void return_void() {}

        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
        }

        void result()
        {
            if (exception)
            {
                std::rethrow_exception(exception);
            }
        }

    private:
        std::exception_ptr exception;
    };

    // The task type.
    template <typename T = void>
    class task
    {
    public:
        using promise_type = task_promise<T>;
        using value_type   = T;

    private:

        // Common implementation for the task<> awaitable.
        // See operator co_await below.
        struct awaitable_base
        {
            std::coroutine_handle<promise_type> coro_handle;

            awaitable_base(std::coroutine_handle<promise_type> coro_handle_)
                : coro_handle{coro_handle_}
            {}

            bool await_ready() const noexcept
            {
                // A task that is awaited upon is ready if the handle is invalid
                // or if the couroutine to which it refers is already complete.
                return !coro_handle || coro_handle.done();
            }

            bool await_suspend(std::coroutine_handle<> awaiting_coro_handle) noexcept
            {
                // The await_suspend() method is invoked upon the awaiter for 
                // the task type the first time that co_await is used to 
                // await upon the result of the task, because the computation 
                // is lazy and thus does not suspend at initial_suspend().
                //
                // The first thing we do in await_suspend() is resume the 
                // coroutine for the awaited-upon task, which has the effect 
                // of actually executing some part (possibly all) of the lazy
                // computation that the task ultimately represents.
                coro_handle.resume();

                // Recall from above the semantics of try_set_continuation() for the
                // task_promise type: the method returns `true` if the promise did 
                // not previously possess a valid continuation, implying that the
                // coroutine referred to by the promise has not yet completed. Thus,
                // we only return control to the caller of the coroutine in the event
                // that the coroutine is not yet complete.
                auto const should_return_to_caller 
                    = coro_handle.promise().try_set_continuation(awaiting_coro_handle);

                return should_return_to_caller;
            }
        };

    public:
        task() noexcept
            : coro_handle{nullptr}
        {}

        explicit task(std::coroutine_handle<promise_type> coro_handle_)
            : coro_handle{coro_handle_}
        {}

        ~task()
        {
            if (coro_handle)
            {
                coro_handle.destroy();
            }
        }

        task(task const&)           = delete;
        task& operator=(task const&) = delete;

        task(task&& t) noexcept
            : coro_handle{t.coro_handle}
        {
            t.coro_handle = nullptr;
        }

        task& operator=(task&& t)
        {
            if (std::addressof(t) != this)
            {
                if (coro_handle)
                {
                    coro_handle.destroy();
                }

                coro_handle   = t.coro_handle;
                t.coro_handle = nullptr;
            }

            return *this;
        }

        bool is_ready() const noexcept
        {
            return !coro_handle || coro_handle.done();
        }

        // Get the awaiter for the task type.
        auto operator co_await() const noexcept
        {
            struct awaitable : awaitable_base
            {
                using awaitable_base::awaitable_base;

                decltype(auto) await_resume()
                {
                    if (!this->coro_handle)
                    {
                        throw std::runtime_error{"broken promise"};
                    }

                    // Return the result of the computation.
                    return this->coro_handle.promise().result();
                }
            };

            return awaitable{ coro_handle };
        }

        bool resume()
        {
            if (!coro_handle.done())
            {
                coro_handle.resume();
            }

            return !coro_handle.done();
        }

        std::coroutine_handle<promise_type> handle() const noexcept
        {
            return coro_handle;
        }

    private:
        std::coroutine_handle<promise_type> coro_handle;
    };

    template <typename T>
    task<T> task_promise<T>::get_return_object() noexcept
    {
        return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
    }

    task<void> task_promise<void>::get_return_object() noexcept
    {
        return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
    }
}

#endif // CORO_TASK_HPP