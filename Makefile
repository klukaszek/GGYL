all: ggyl test

ggyl: ggyl.c ggyl.h
	gcc -Wall -g -std=gnu11 -o ggyl ggyl.c ggyl.h


test: test.c ggyl.h
	gcc -Wall -g -std=gnu11 -o test test.c ggyl.h

.PHONY: clean
clean:
	rm -f ggyl test
