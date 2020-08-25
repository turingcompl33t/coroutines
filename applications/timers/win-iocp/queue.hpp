// queue.hpp
// Dead-simple concurrent queue.

#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <queue>
#include <optional>
#include <windows.h>

#include <libcoro/win/system_error.hpp>

template <typename T>
class queue
{
    // Mutex handle; mutual exclusion.
    HANDLE lock;

    // CV handle; singalled when item is inserted into queue.
    HANDLE non_empty;

    std::queue<T*> buffer;

public:
    queue()
        : lock{::CreateMutexW(nullptr, FALSE, nullptr)}
        , non_empty{::CreateEventW(nullptr, TRUE, FALSE, nullptr)}
        , buffer{}
    {
        if (NULL == lock || NULL == non_empty)
        {
            throw coro::win::system_error{};
        }
    }

    ~queue()
    {
        ::CloseHandle(non_empty);
        ::CloseHandle(lock);
    }

    // non-copyable
    queue(queue const&)            = delete;
    queue& operator=(queue const&) = delete;

    // non-movable
    queue(queue&&)            = delete;
    queue& operator=(queue&&) = delete;

    void push(T* obj, BOOL alertable)
    {
        ::WaitForSingleObjectEx(lock, INFINITE, alertable);
        
        buffer.push(obj);

        ::ReleaseMutex(lock);
        ::SetEvent(non_empty);
    }

    T* pop(BOOL alertable)
    {
        ::WaitForSingleObjectEx(lock, INFINITE, alertable);
        while (is_empty())
        {
            ::SignalObjectAndWait(lock, non_empty, INFINITE, alertable);
            ::WaitForSingleObjectEx(lock, INFINITE, alertable);
        }

        auto* popped = buffer.front();
        buffer.pop();

        ::ReleaseMutex(lock);

        return popped;
    }

    std::optional<T*> try_pop(BOOL alertable)
    {
        std::optional<T*> popped{};

        ::WaitForSingleObjectEx(lock, INFINITE, alertable);
        if (!is_empty())
        {
            popped.emplace(buffer.front());
            buffer.pop();
        }

        ::ReleaseMutex(lock);

        return popped;
    }

private:
    bool is_empty()
    {
        return buffer.empty();
    }
};

#endif // QUEUE_HPP
