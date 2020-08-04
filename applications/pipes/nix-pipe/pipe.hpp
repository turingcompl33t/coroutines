// pipe.hpp

#ifndef CORO_PIPE_HPP
#define CORO_PIPE_HPP

#include "readable_pipe.hpp"
#include "writeable_pipe.hpp"

#include <tuple>

#include <fcntl.h>
#include <unistd.h>

namespace coro
{
    std::pair<readable_pipe, writeable_pipe>
    make_pipe(io_context const& ioc)
    {
        int pipefds[2];
        
        // create pipes in nonblocking mode
        int const res = ::pipe2(pipefds, O_NONBLOCK);
        if (-1 == res)
        {
            throw system_error{};
        }

        return std::make_pair(
            readable_pipe{pipefds[0], ioc}, 
            writeable_pipe{pipefds[1], ioc});
    }
}



#endif // CORO_PIPE_HPP