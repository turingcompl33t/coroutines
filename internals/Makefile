# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines 

COMMON = task.hpp

suspend_never: $(COMMON)
	$(CC) $(CPPFLAGS) suspend_never.cpp -o suspend_never

suspend_always: $(COMMON)
	$(CC) $(CPPFLAGS) suspend_always.cpp -o suspend_always

clean:
	rm -f *.o
	rm -f suspend_never
	rm -f suspend_always

.PHONY: clean