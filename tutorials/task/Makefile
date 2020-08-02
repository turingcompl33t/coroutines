# async-latch/Makefile

LIB = ../../lib/include/

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines -fmax-errors=1 -I$(LIB)

DEPS = $(LIB)/libcoro/task.hpp $(LIB)/libcoro/task_queue.hpp

driver: $(DEPS)
	$(CC) $(CPPFLAGS) driver.cpp -o driver

clean:
	rm -f *.o
	rm -f driver

.PHONY: clean