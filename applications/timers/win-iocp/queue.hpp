// queue.hpp

#ifndef QUEUE_HPP
#define QUEUE_HPP

#include "win32_error.hpp"

#include <queue>
#include <optional>
#include <windows.h>

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
            throw win32_error{};
        }
    }

    ~queue()
    {
        ::CloseHandle(non_empty);
        ::CloseHandle(lock);
    }

    queue(queue const&)            = delete;
    queue& operator=(queue const&) = delete;

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
