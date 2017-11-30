CC=mpicc
SRC=main.c aux.c
OBJ = main.o aux.o
CFLAGS=-g -O3 -std=c99
target: decide_mpi
main.o: main.c projet.h
aux.o: aux.c projet.h





decide_mpi: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f *.o decide_mpi

