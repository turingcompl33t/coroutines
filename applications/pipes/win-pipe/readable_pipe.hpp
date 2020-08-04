// readable_pipe.hpp

#ifndef CORO_READABLE_PIPE_HPP
#define CORO_READABLE_PIPE_HPP

#include "io_context.hpp"
#include "win32_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <windows.h>

#include <utility>

namespace coro
{
    class readable_pipe
    {
        // The IO context with which this pipe instance is associated.
        io_context const& ioc;

        // The underlying pipe handle.
        HANDLE handle;

    public:
        readable_pipe(io_context const& ioc_, HANDLE handle_)
            : ioc{ioc_}, handle{handle_}
        {
            ioc.register_handle(handle);
        }

        ~readable_pipe()
        {
            close();
        }

        readable_pipe(readable_pipe const&)            = delete;
        readable_pipe& operator=(readable_pipe const&) = delete;

        readable_pipe(readable_pipe&& rp)
            : ioc{rp.ioc}, handle{rp.handle}
        {
            rp.handle = INVALID_HANDLE_VALUE;
        }

        readable_pipe& operator=(readable_pipe&&) = delete;

        void read(void* buffer, std::size_t len, LPOVERLAPPED ov);

        auto read_some(void* buffer, std::size_t len);

    private:
        void close()
        {
            if (INVALID_HANDLE_VALUE != handle)
            {
                ::CloseHandle(handle);
            }
        }
    };

    void readable_pipe::read(void* buffer, std::size_t len, LPOVERLAPPED ov)
    {
        printf("handle = %p\n", handle);
        unsigned long n_read{};
        auto const status = ::ReadFile(
            handle, 
            buffer,
            len,
            &n_read,
            ov);

        auto const err = ::GetLastError();
        if (!status && ERROR_IO_PENDING == err)
        {
            printf("pending\n");
        }
        else
        {
            printf("err = %u\n", err);
        }
    }

    auto readable_pipe::read_some(void* buffer, std::size_t len)
    {
        struct awaiter : ioc_awaiter_base
        {   
            readable_pipe* me;
            void*          buffer;
            std::size_t    len;
            unsigned long  error;

            awaiter(readable_pipe* me_, void* buffer_, std::size_t len_)
                : me{me_}, buffer{buffer_}, len{len_}, error{0}
            {}

            bool await_ready()
            {
                return false;
            }

            bool await_suspend(
                std::experimental::coroutine_handle<> awaiting_coro)
            {
                this->awaiting_coro = awaiting_coro;

                auto const status = ::ReadFile(
                    this->me->handle,
                    this->buffer,
                    static_cast<unsigned long>(this->len),
                    &this->bytes_xfer,
                    reinterpret_cast<LPOVERLAPPED>(this));

                auto const err = ::GetLastError();
                if (!status && ERROR_IO_PENDING == err) 
                {
                    // await completion
                    return true;
                }
                else
                {
                    printf("err = %u\n", err);
                    this->error = err;
                }

                // synchronous completion
                return false;
            }

            std::size_t await_resume()
            {
                return this->bytes_xfer;
            }
        };

        return awaiter{this, buffer, len};
    }
}

#endif // CORO_READABLE_PIPE_HPP