# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines -fmax-errors=1 -ggdb

DEPS = future.hpp

vanilla: vanilla.cpp
	$(CC) $(CPPFLAGS) vanilla.cpp -o vanilla

awaitable: awaitable.cpp
	$(CC) $(CPPFLAGS) awaitable.cpp -o awaitable

clean:
	rm -f *.o
	rm -f vanilla
	rm -f awaitable

.PHONY: clean