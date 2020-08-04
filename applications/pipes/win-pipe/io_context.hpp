// io_context.hpp

#ifndef CORO_IO_CONTEXT_HPP
#define CORO_IO_CONTEXT_HPP

#include "win32_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <windows.h>

#include <thread>
#include <utility>

namespace coro
{
    class io_context
    {
        // The IOCP instance.
        HANDLE port;

        // Monotonically increasing counter.
        mutable volatile LONG64 key;        

    public:
        io_context(unsigned long const max_threads 
            = std::thread::hardware_concurrency())
            : port{::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, max_threads)}
            , key{0}
        {
            if (NULL == port)
            {
                throw win32_error{};
            }
        }

        ~io_context()
        {
            ::CloseHandle(port);
        }

        io_context(io_context const&)            = delete;
        io_context& operator=(io_context const&) = delete; 

        io_context(io_context&& ioc)
            : port{ioc.port}
        {
            ioc.port = NULL;
        }

        io_context& operator=(io_context&& ioc)
        {
            if (std::addressof(ioc) != this)
            {
                close();

                port     = ioc.port;
                ioc.port = NULL;
            }

            return *this;
        }

        void register_handle(HANDLE handle) const
        {
            auto const res = ::CreateIoCompletionPort(
                handle, 
                port, 
                InterlockedIncrement64(&key), 
                0);

            if (NULL == res)
            {
                throw win32_error{};
            }
        }

        std::size_t process_events() const;

    private:
        void close()
        {
            ::CloseHandle(port);
        }
    };  

    std::size_t io_context::process_events() const
    {
        LPOVERLAPPED pov;
        ULONG_PTR completion_key = 0;
        unsigned long bytes_xfer = 0;

        for (;;)
        {
            auto const status = ::GetQueuedCompletionStatus(
                port, 
                &bytes_xfer, 
                &completion_key, 
                &pov, 
                INFINITE);

            if (status)
            {
                printf("got completion\n");
                //ioc_awaiter_base::on_io_complete(pov, bytes_xfer);
            }
        }
    }
}

#endif // CORO_IO_CONTEXT_HPP