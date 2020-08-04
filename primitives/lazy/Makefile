# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines -ggdb

lazy: $(COMMON)
	$(CC) $(CPPFLAGS) lazy.cpp -o lazy

clean:
	rm -f *.o
	rm -f lazy

.PHONY: clean