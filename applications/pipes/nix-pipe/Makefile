# Makefile

LIB = ../../lib/include/

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -std=c++20 -fcoroutines -fmax-errors=1 -I$(LIB) -ggdb

LIB_DEPS = $(LIB)/libcoro/task.hpp $(LIB)/libcoro/task_queue.hpp
LOCAL_DEPS = io_context.hpp pipe.hpp readable_pipe.hpp writeable_pipe.hpp runtime_error.hpp system_error.hpp

driver1: $(LIB_DEPS) $(LOCAL_DEPS)
	$(CC) $(CPPFLAGS) driver1.cpp -o driver1 -pthread

driver2: $(LIB_DEPS) $(LOCAL_DEPS)
	$(CC) $(CPPFLAGS) driver2.cpp -o driver2 -pthread

test:
	$(CC) $(CPPFLAGS) test.cpp -o test -pthread

clean:
	rm -f *.o
	rm -f driver1
	rm -f driver2
	rm -f test

.PHONY: clean