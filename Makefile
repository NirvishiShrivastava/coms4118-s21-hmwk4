AS = gcc
FLAGS = -Wall

all: test

test: test_sched.c
	$(AS) $(FLAGS) $< -o $@

clean:
	rm test
