# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines -fmax-errors=1

co_await:
	$(CC) $(CPPFLAGS) co_await.cpp -o co_await

co_return:
	$(CC) $(CPPFLAGS) co_return.cpp -o co_return

co_yield:
	$(CC) $(CPPFLAGS) co_yield.cpp -o co_yield

nested:
	$(CC) $(CPPFLAGS) nested.cpp -o nested

# counter:
# 	$(CC) $(CPPFLAGS) counter.cpp -o counter

clean:
	rm -f *.o
	rm -f co_await
	rm -f co_return
	rm -f co_yield
	rm -f nested

.PHONY: clean