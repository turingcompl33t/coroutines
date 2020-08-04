// ioc_awaiter_base.hpp

#ifndef CORO_IOC_AWAITER_BASE_HPP
#define CORO_IOC_AWAITER_BASE_HPP

#include <experimental/coroutine>

#include <windows.h>

namespace coro
{
    struct ioc_awaiter_base : OVERLAPPED
    {   
        template <typename T>
        using coroutine_handle = std::experimental::coroutine_handle<T>;

        // The handle to the awaiting coroutine.
        coroutine_handle<void> awaiting_coro;

        unsigned long bytes_xfer;

        static void on_io_complete(void* ctx, unsigned long bytes_xfer)
        {
            auto* me = reinterpret_cast<ioc_awaiter_base*>(ctx);
            me->bytes_xfer = bytes_xfer;
            me->awaiting_coro.resume();
        }
    };
}

#endif // CORO_IOC_AWAITER_BASE_HPP