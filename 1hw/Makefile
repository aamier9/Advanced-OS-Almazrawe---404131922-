CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude

all: test_run

test_run: tests/hello_local.o src/apager/apager.o src/hpager/hpager.o src/dpager/dpager.o
	$(CC) $(CFLAGS) $^ -o test_runner

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*/*.o tests/*.o test_runner
