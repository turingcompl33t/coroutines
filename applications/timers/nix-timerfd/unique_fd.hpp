// unique_fd.hpp

#ifndef CORO_UNIQUE_FD_HPP
#define CORO_UNIQUE_FD_HPP

#include <unistd.h>
#include <utility>

class unique_fd
{
    int fd;
public:
    unique_fd() : fd{-1} {}

    explicit unique_fd(int const fd_) 
        : fd{fd_} {}

    unique_fd(unique_fd const&)            = delete;
    unique_fd& operator=(unique_fd const&) = delete;

    unique_fd(unique_fd&& ufd)
        : fd{ufd.fd}
    {
        ufd.fd = -1;
    }

    unique_fd& operator=(unique_fd&& ufd)
    {
        if (std::addressof(ufd) != this)
        {
            close();

            fd     = ufd.fd;
            ufd.fd = -1;
        }

        return *this;
    }

    ~unique_fd()
    {
        close();
    }

    int get() const noexcept
    {
        return fd;
    }

    int* put() noexcept
    {
        return &fd;
    }

    int release() noexcept
    {
        int const tmp = fd;
        fd = -1;
        return tmp;
    }

    explicit operator bool()
    {
        return fd != -1;
    }

private:
    void close()
    {
        if (fd != -1)
        {
            ::close(fd);
        }
    }
};

#endif // CORO_UNIQUE_FD_HPP