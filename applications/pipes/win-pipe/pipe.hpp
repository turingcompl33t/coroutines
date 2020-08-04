// pipe.hpp

#ifndef CORO_PIPE_HPP
#define CORO_PIPE_HPP

#include "io_context.hpp"
#include "win32_error.hpp"
#include "readable_pipe.hpp"
#include "writeable_pipe.hpp"

#include <tuple>
#include <cstdio>

#include <windows.h>

static volatile unsigned long PIPE_SERIAL_NO = 0;

namespace coro
{
    namespace detail
    {
        bool create_pipe_ex(
            HANDLE* read_pipe, 
            HANDLE* write_pipe, 
            LPSECURITY_ATTRIBUTES attributes, 
            unsigned long size, 
            unsigned long read_mode, 
            unsigned long write_mode)
        {
            char pipe_name_buffer[MAX_PATH];

            _snprintf_s(
                pipe_name_buffer, 
                MAX_PATH, 
                "\\\\.\\Pipe\\LOCAL.%08X.%08X",
                ::GetCurrentProcessId(),
                InterlockedIncrement(&PIPE_SERIAL_NO));

            auto read_handle = ::CreateNamedPipeA(
                pipe_name_buffer,
                PIPE_ACCESS_INBOUND | read_mode,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | 
                PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                1,
                size,
                size,
                0,
                attributes);

            if (INVALID_HANDLE_VALUE == read_handle)
            {
                return false;
            }

            auto write_handle = ::CreateFileA(
                pipe_name_buffer,
                GENERIC_WRITE,
                0,
                attributes,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | write_mode,
                NULL);
            
            if (INVALID_HANDLE_VALUE == write_handle)
            {
                ::CloseHandle(read_handle);
                return false;
            }

            *read_pipe  = read_handle;
            *write_pipe = write_handle;

            return true;
        }
    }

    std::pair<readable_pipe, writeable_pipe>
    pipe(io_context const& ioc)
    {
        HANDLE reader;
        HANDLE writer;

        if (!detail::create_pipe_ex(
            &reader, 
            &writer, 
            NULL, 
            0, 
            FILE_FLAG_OVERLAPPED, 
            FILE_FLAG_OVERLAPPED))
        {
            throw win32_error{};
        }

        return std::make_pair(
            readable_pipe{ioc, reader}, 
            writeable_pipe{ioc, writer});
    }
}

#endif // CORO_PIPE_HPP