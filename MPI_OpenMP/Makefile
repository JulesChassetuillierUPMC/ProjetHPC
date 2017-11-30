CC=mpicc
SRC=main.c aux.c
OBJ = main.o aux.o
CFLAGS=-g -fopenmp -O3 -std=c99
target: decide_mpi_openmp
main.o: main.c projet.h
aux.o: aux.c projet.h





decide_mpi_openmp: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f *.o decide_mpi_openmp

