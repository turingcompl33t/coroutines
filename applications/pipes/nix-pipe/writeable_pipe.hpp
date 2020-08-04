// writeable_pipe.hpp

#ifndef CORO_WRITEABLE_PIPE_HPP
#define CORO_WRITEABLE_PIPE_HPP

#include "io_context.hpp"
#include "system_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <memory>
#include <utility>
#include <coroutine>

namespace coro
{
    class writeable_pipe
    {
        // The underlying file descriptor for the writeable pipe.
        int fd;

        // A reference to the IO context with which this pipe is associated.
        io_context const* ioc;

        std::unique_ptr<ioc_awaiter_base> awaiter_base;

    public:
        writeable_pipe(
            int fd_, 
            io_context const& ioc_)
            : fd{fd_}
            , ioc{std::addressof(ioc_)}
            , awaiter_base{std::make_unique<ioc_awaiter_base>()}
        {
            ioc->register_handle(fd, io_interest::write);
        }

        ~writeable_pipe()
        {
            if (fd != -1)
            {
                close();
            }
        }

        writeable_pipe(writeable_pipe&& wp) 
            : fd{wp.fd}
            , ioc{wp.ioc}
        {
            wp.fd = -1;
        }

        writeable_pipe& operator=(writeable_pipe&& wp)
        {
            if (std::addressof(wp) != this)
            {
                if (fd != -1)
                {
                    close();
                }

                fd    = wp.fd;
                ioc   = wp.ioc;
                wp.fd = -1;
            }

            return *this;
        }

        auto write_some(void* buffer, std::size_t len)
        {
            struct awaiter : ioc_awaiter_base
            {
                io_context const*  ioc;
                writeable_pipe*    me;
                void*              buffer;
                std::size_t        len;

                std::size_t        bytes_xfer;

                awaiter(io_context const* ioc_, writeable_pipe* me_, void* buffer_, std::size_t len_)
                    : ioc{ioc_}, me{me_}, buffer{buffer_}, len{len} {}

                bool await_ready() 
                {
                    return false;
                }

                bool await_suspend(std::coroutine_handle<> awaiting_coro)
                {
                    this->awaiting_coro = awaiting_coro;
                    this->ioc->set_awaiter(this->me->fd, static_cast<ioc_awaiter_base*>(this));

                    // attempt the read operation
                    auto const n_bytes = ::write(this->me->fd, this->buffer, this->len);
                    if (-1 == n_bytes)
                    {
                        if (errno == EAGAIN) return true;
                        else throw system_error{};
                    }

                    // synchronous completion
                    this->bytes_xfer = n_bytes;
                    return false;
                }

                std::size_t await_resume()
                {
                    // check for synchronous completion
                    if (this->bytes_xfer > 0)
                    {
                        return this->bytes_xfer;
                    }

                    // the reactor notification ensures us this call will not block now
                    auto const n_bytes = ::write(this->me->fd, this->buffer, this->len);
                    if (-1 == n_bytes)
                    {
                        throw system_error{};
                    }

                    return n_bytes;
                };
            };

            return awaiter{ioc, this, buffer, len};
        }

    private:
        void close()
        {
            ioc->unregister_handle(fd);
            ::close(fd);
        }
    };
}

#endif // CORO_WRITEABLE_PIPE_HPP