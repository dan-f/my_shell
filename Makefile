CFLAGS=-g -pedantic -std=c99 -Wall# -O2
CC=gcc

all: run_shell_rl run_shell 

run_shell: run_shell.o util.o
	$(CC) $(CFLAGS) -o run_shell run_shell.o util.o

run_shell_rl: run_shell_rl.o util.o
	$(CC) $(CFLAGS) -o run_shell_rl run_shell_rl.o -lreadline util.o

util.o: util.c util.h
	$(CC) $(CFLAGS) -o util.o -c util.c

run_shell.o: run_shell.c run_shell.h util.h
	$(CC) $(CFLAGS) -o run_shell.o -c run_shell.c

run_shell_rl.o: run_shell.c run_shell.h util.h
	$(CC) $(CFLAGS) -o run_shell_rl.o -c run_shell_rl.c

test: run_shell_rl
	./run_shell_rl

clean:
	rm -f *.o run_shell run_shell_rl
