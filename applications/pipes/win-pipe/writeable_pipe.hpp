// writeable_pipe.hpp

#ifndef CORO_WRITEABLE_PIPE_HPP
#define CORO_WRITEABLE_PIPE_HPP

#include "io_context.hpp"
#include "win32_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <windows.h>

namespace coro
{
    class writeable_pipe
    {
        // The IO context with which this pipe instance is associated.
        io_context const& ioc;

        // The underlying pipe handle.
        HANDLE handle;
    public:
        writeable_pipe(io_context const& ioc_, HANDLE handle_)
            : ioc{ioc_}, handle{handle_}
        {
            ioc.register_handle(handle);
        }

        ~writeable_pipe()
        {
            close();
        }

        writeable_pipe(writeable_pipe const&)            = delete;
        writeable_pipe& operator=(writeable_pipe const&) = delete;

        writeable_pipe(writeable_pipe&& rp)
            : ioc{rp.ioc}, handle{rp.handle}
        {
            rp.handle = INVALID_HANDLE_VALUE;
        }

        writeable_pipe& operator=(writeable_pipe&&) = delete;

        auto write_some(void* buffer, std::size_t len);

    private:
        void close()
        {
            ::CloseHandle(handle);
        }
    };

    auto writeable_pipe::write_some(void* buffer, std::size_t len)
    {
        struct awaiter : ioc_awaiter_base
        {   
            writeable_pipe* me;
            void*          buffer;
            std::size_t    len;

            awaiter(writeable_pipe* me_, void* buffer_, std::size_t len_)
                : me{me_}, buffer{buffer_}, len{len_}
            {}

            bool await_ready()
            {
                return false;
            }

            bool await_suspend(
                std::experimental::coroutine_handle<> awaiting_coro)
            {
                unsigned long bytes_written{};

                auto const status = ::WriteFile(
                    this->me->handle,
                    this->buffer,
                    this->len,
                    &bytes_written,
                    reinterpret_cast<LPOVERLAPPED>(&this->Internal));

                if (ERROR_IO_PENDING == status) 
                {
                    return true;
                }
                else if (status != ERROR_SUCCESS)
                {
                    throw win32_error{};
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

#endif // CORO_WRITEABLE_PIPE_HPP