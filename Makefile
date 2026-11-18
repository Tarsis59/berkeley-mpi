CC     = mpicc
CFLAGS = -O2 -Wall -Wextra
N     ?= 5

berkeley: berkeley.c
	$(CC) $(CFLAGS) -o $@ $< -lm

run: berkeley
	mpirun --oversubscribe -n $(N) ./berkeley

exemplo: berkeley
	mpirun --oversubscribe -n 3 ./berkeley -e -r 1

falha: berkeley
	mpirun --oversubscribe -n 6 ./berkeley -f 3

clean:
	rm -f berkeley

.PHONY: run exemplo falha clean
