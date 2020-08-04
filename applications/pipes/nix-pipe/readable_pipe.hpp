// readable_pipe.hpp

#ifndef CORO_READABLE_PIPE_HPP
#define CORO_READABLE_PIPE_HPP

#include "io_context.hpp"
#include "system_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <cstdio>
#include <memory>
#include <utility>
#include <coroutine>

namespace coro
{
    class readable_pipe
    {
        // The underlying file descriptor for the readble pipe.
        int fd;

        // A reference to the IO context with which this pipe is associated.
        io_context const* ioc;

    public:
        friend struct awaiter;

        readable_pipe(
            int fd_, 
            io_context const& ioc_)
            : fd{fd_}
            , ioc{std::addressof(ioc_)}
        {
            ioc->register_handle(fd, io_interest::read);
        }

        ~readable_pipe()
        {
            if (fd != -1)
            {
                close();
            }
        }       

        // non-copyable
        readable_pipe(readable_pipe const&)            = delete;
        readable_pipe& operator=(readable_pipe const&) = delete;

        readable_pipe(readable_pipe&& rp) 
            : fd{rp.fd}
            , ioc{rp.ioc}
        {
            rp.fd = -1;
        }

        readable_pipe& operator=(readable_pipe&& rp)
        {
            if (std::addressof(rp) != this)
            {
                if (fd != -1)
                {
                    close();
                }

                fd    = rp.fd;
                ioc   = rp.ioc;
                rp.fd = -1;
            }

            return *this;
        }

        auto read_some(void* buffer, std::size_t len)
        {
            struct awaiter : ioc_awaiter_base
            {
                io_context const* ioc;
                readable_pipe*    me;
                void*             buffer;
                std::size_t       len;

                std::size_t       bytes_xfer;

                awaiter(io_context const* ioc_, readable_pipe* me_, void* buffer_, std::size_t len_)
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
                    // auto const n_bytes = ::read(this->me->fd, this->buffer, this->len);
                    // if (-1 == n_bytes)
                    // {
                    //     if (errno == EAGAIN) return true;
                    //     else throw system_error{};
                    // }

                    // // synchronous completion
                    // this->bytes_xfer = n_bytes;
                    return true;
                }

                std::size_t await_resume()
                {
                    // check for synchronous completion
                    if (this->bytes_xfer > 0)
                    {
                        return this->bytes_xfer;
                    }

                    // the reactor notification ensures us this call will not block now
                    auto const n_bytes = ::read(this->me->fd, this->buffer, this->len);
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
            fd = -1;
        }
    };
}

#endif // CORO_READABLE_PIPE_HPP