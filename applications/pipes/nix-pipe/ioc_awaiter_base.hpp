// ioc_awaiter_base.hpp

#ifndef CORO_IOC_AWAITER_BASE_HPP
#define CORO_IOC_AWAITER_BASE_HPP

#include <coroutine>

namespace coro
{
    struct ioc_awaiter_base
    {
        int                     error_code;
        std::coroutine_handle<> awaiting_coro;
        
        static void io_ready_callback(void* awaiter_base, int const error_code)
        {
            auto* me = static_cast<ioc_awaiter_base*>(awaiter_base);
            me->error_code = error_code;
            me->awaiting_coro.resume();
        }
    };
}

#endif // CORO_IOC_AWAITER_BASE_HPP