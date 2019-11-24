all: shell.c
	gcc -Wall -g -o shell shell.c -lm
clean:
	$(RM) shell
