// io_context.hpp

#ifndef CORO_IO_CONTEXT_HPP
#define CORO_IO_CONTEXT_HPP

#include "system_error.hpp"
#include "runtime_error.hpp"
#include "ioc_awaiter_base.hpp"

#include <memory>
#include <utility>
#include <coroutine>

#include <unistd.h>
#include <sys/epoll.h>

namespace coro
{
    static void process_n_events(
        struct epoll_event* events, 
        int const           n_ready);

    enum class io_interest
    {
        read,
        write    
    };

    class io_context
    {   
        // The maximum number of event sources monitored.
        std::size_t const max_events;

        // The epoll instance file descriptor.
        int instance;

    public:
        io_context(std::size_t const max_events_)
            : max_events{max_events_}
            , instance{-1}
        {
            if (max_events < 1)
            {
                throw runtime_error{"invalid max_events count specified"};
            }   

            int const epoll_instance = ::epoll_create1(0);
            if (-1 == epoll_instance)
            {
                throw system_error{};
            }

            instance = epoll_instance;
        }

        ~io_context()
        {
            if (instance != -1)
            {
                ::close(instance);
            }
        }

        io_context(io_context const&)            = delete;
        io_context& operator=(io_context const&) = delete;

        io_context(io_context&& ioc) 
            : max_events{ioc.max_events} 
            , instance{ioc.instance}
        {
            ioc.instance = -1;
        }

        io_context& operator=(io_context&& ioc)
        {
            if (std::addressof(ioc) != this)
            {
                instance     = ioc.instance;
                ioc.instance = -1;
            }

            return *this;
        }

        // Register an OS-specific event source handle with the reactor.
        void register_handle(
            int const         fd, 
            io_interest       interest) const
        {
            uint32_t event_flags;
            struct epoll_event ev{};

            switch (interest)
            {
            case io_interest::read:
                event_flags = EPOLLIN;
                break;
            case io_interest::write:
                event_flags = EPOLLOUT;
                break;
            default:
                break;
            }

            ev.events   = event_flags;
            ev.data.ptr = nullptr;

            auto const res = ::epoll_ctl(instance, EPOLL_CTL_ADD, fd, &ev);
            if (-1 == res)
            {
                throw system_error{};
            }
        }

        // Unregister an OS-specific event source handle with the reactor.
        void unregister_handle(int fd) const
        {
            struct epoll_event ev{};
            auto const res = ::epoll_ctl(instance, EPOLL_CTL_DEL, fd, &ev);
            if (-1 == res && errno != ENOENT)
            {
                throw system_error{};
            }
        }

        void set_awaiter(int const fd, ioc_awaiter_base* awaiter) const
        {
            struct epoll_event ev{};
            ev.data.ptr = static_cast<void*>(awaiter);

            int const res = ::epoll_ctl(instance, EPOLL_CTL_MOD, fd, &ev);
            if (-1 == res)
            {
                throw system_error{};
            }
        }

        std::size_t process_events();
    };

    std::size_t io_context::process_events()
    {
        auto events = std::make_unique<struct epoll_event[]>(max_events);

        for (;;)
        {
            int const n_ready = ::epoll_wait(instance, events.get(), max_events, -1);
            if (-1 == n_ready)
            {
                throw system_error{};
            }

            process_n_events(events.get(), n_ready);
        }
    }

    static void process_n_events(
        struct epoll_event* events, 
        int const           n_ready)
    {
        for (auto i = 0; i < n_ready; ++i)
        {
            auto& event   = events[i];
            auto* awaiter = static_cast<ioc_awaiter_base*>(event.data.ptr);

            if ((event.events & EPOLLIN) || (event.events & EPOLLOUT))
            {
                // read / write ready
                ioc_awaiter_base::io_ready_callback(awaiter, 0);
            }
            else if (event.events & (EPOLLHUP | EPOLLERR))
            {
                // TODO: error codes??
                ioc_awaiter_base::io_ready_callback(awaiter, 1);
            }
        }
    }
}

#endif // CORO_IO_CONTEXT_HPP