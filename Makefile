AS = gcc
FLAGS = -Wall

all: get_wrr_info_test

get_wrr_info_test: get_wrr_info_test.c
	$(AS) $(FLAGS) $< -o $@

clean:
	rm get_wrr_info_test
