// queue.hpp
//
// A simple, internally synchronized queue.

#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <mutex>
#include <queue>
#include <memory>
#include <condition_variable>

class queue
{
    // The internal queue.
    std::queue<uintptr_t> buffer;

    // Synchronization for the internal queue.
    std::mutex              lock;
    std::condition_variable non_empty_cv;

public:
    queue() 
        : buffer{}
        , lock{}
        , non_empty_cv{}
    {}

    ~queue() = default;

    // non-copyable
    queue(queue const&)            = delete;
    queue& operator=(queue const&) = delete;

    // non-movable
    queue(queue&&)            = delete;
    queue& operator=(queue&&) = delete;

    auto push(uintptr_t const value) -> void
    {
        {
            auto guard = std::scoped_lock{lock};
            buffer.push(value);
        }

        non_empty_cv.notify_one();
    }

    template <typename Container>
    auto push_batch(Container const& values)
    {
        {
            auto guard = std::scoped_lock{lock};
            for (auto const& v : values)
            {
                buffer.push(v);
            }
        }

        for (auto i = 0u; i < values.size(); ++i)
        {
            non_empty_cv.notify_one();
        }
    }

    auto pop() -> uintptr_t
    {
        auto guard = std::unique_lock{lock};
        non_empty_cv.wait(guard, [&](){ return is_nonempty_unsafe(); });

        auto const popped = buffer.front();
        buffer.pop();

        return popped;
    }

private:
    bool is_nonempty_unsafe()
    {
        return !buffer.empty();
    }
};

#endif // QUEUE_HPP