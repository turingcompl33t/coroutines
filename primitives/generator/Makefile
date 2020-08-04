# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines

generator: $(COMMON)
	$(CC) $(CPPFLAGS) generator.cpp -o generator

clean:
	rm -f *.o
	rm -f generator

.PHONY: clean