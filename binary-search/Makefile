# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines 

driver: coro_infra.hpp coroutine.hpp rng.hpp state_machine.hpp vanilla.hpp
	$(CC) $(CPPFLAGS) driver.cpp -o driver

clean:
	rm -f *.o
	rm -f driver

.PHONY: clean