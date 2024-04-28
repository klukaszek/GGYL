all: ggyl test

ggyl: ggyl.c
	gcc -Wall -g -o ggyl ggyl.c


test: test2.c test3.h
	gcc -Wall -g -std=gnu11 -o test test2.c test3.h

.PHONY: clean
clean:
	rm -f ggyl test
