## C++ Coroutines

Exploring behavior and applications of C++ coroutines.

### Requirements

All programs in this repository that do not make use of platform-specific IO APIs build successfully under the following OS / compiler combinations:

- `g++ 10.1` on Ubuntu 18.04
- `clang++ 11.0` on MacOS Catalina 10.15
- `msvc 19.26` on Windows 10 2004

The `coroutine.hpp` header file in the `config/` directory, along with the CMake compiler feature detection in `cmake/set_coro_options.cmake`, attempt to act as a shim layer such that all three major compilers are supported. That said, I would not be at all surprised if I have neglected to account for some (or many) potential build environments where this shim layer fails to behave appropriately.

Some of the programs in this repository, typically those in the `applications/` directory, make use of operating-specific APIs in order to interact with various IO services such as pipes, sockets, timers, etc. In most cases, no attempt is made to make a single program cross-platform. However, in many cases multiple implementations are included such that one may compare the way in which coroutines are integrated with the IO system when different operating system APIs are available.

### Contents

An approximate outline of the contents of this repository is as follows:

- [applications](./applications) Assorted applications of C++ coroutines including asynchronous pipes, timers, and sockets.
- [basics](./basics) Your first programs using C++ coroutines.
- [internals](./internals) Digging into the nitty-gritty details of how coroutines are implemented under the covers.
- [primitives](./primitives) Assorted primitive programming constructs that enable greater productivity when working with coroutines.