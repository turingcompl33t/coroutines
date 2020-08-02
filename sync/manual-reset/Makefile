# Makefile

CC = /usr/bin/g++-10
CPPFLAGS = -Wall -Werror -std=c++20 -fcoroutines 

driver: manual_reset_event.hpp
	$(CC) $(CPPFLAGS) driver.cpp -o driver

clean:
	rm -f *.o
	rm -f driver

.PHONY: clean