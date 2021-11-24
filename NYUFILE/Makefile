CC=gcc 
CFLAGS=-g -std=c99 -l crypto

.PHONY: all
all: nyufile

nyufile: nyufile.o 

nyufile.o: nyufile.c


.PHONY: clean
clean:
	rm -f *.o nyufile