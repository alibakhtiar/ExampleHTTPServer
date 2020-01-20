# Usage:
# make        # compile all binary
# make clean  # remove ALL binaries and objects
# ./example.out <port>

CC      := g++
CCFLAGS := -std=c++11 -pthread -O3 -Wall -Wshadow -Wtype-limits -Wunused -Wextra -Werror
INCLUDE := -I./

all: example.o
	$(CC) example.o $(INCLUDE) $(CCFLAGS) -o example.out

clean:
	rm -rvf *.o *.out
