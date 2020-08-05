# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines -ggdb

all: example0 example1 example2

example0: task_v0.hpp
	$(CC) $(CPPFLAGS) example0.cpp -o example0

example1: task_v0.hpp
	$(CC) $(CPPFLAGS) example1.cpp -o example1

example2: task_v1.hpp
	$(CC) $(CPPFLAGS) example2.cpp -o example2

clean:
	rm -f *.o
	rm -f example0
	rm -f example1
	rm -f example2

.PHONY: clean