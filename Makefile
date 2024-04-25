all: ggyl test

ggyl: ggyl.c
	gcc -Wall -g -o ggyl ggyl.c


test: test.c test.h
	gcc -Wall -g -std=gnu11 -o test test.c test.h

.PHONY: clean
clean:
	rm -f ggyl test
