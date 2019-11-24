#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pwd.h>
#include <stdbool.h>
#include <grp.h>
#include <termios.h>
#include "commands.h"

int nchild = 0;
char *children[64];

char *getdistro(){
	char *distro = malloc(256*sizeof(char));

	FILE *fd = NULL;
	fd = fopen("/etc/issue", "r");

	if(fd == NULL){
		fprintf(stderr, "MYSHELL: Cannot find linux distribution name, using LINUX as default.\n");
	}
	else{
		fscanf(fd, "%s", distro);
	}
	fclose(fd);
	return distro;
}

char *getterminal(){
	char *terminal = malloc(256*(sizeof(char)));
	char *distro = malloc(256*(sizeof(char)));
	char *username = malloc(256*(sizeof(char)));
	char currdir[256];

	strcpy(terminal, "<");
	cuserid(username, 256);
	gethostname(distro, 256);

	strcat(strcat(strcat(strcat(terminal,username),"@"), distro),":");

	getcwd(currdir, 256);
	if(strstr(currdir, HOME) == NULL){
		strcat(strcat(terminal,currdir),"> ");
	}
	else{
		char *p = currdir;
		p = p + strlen(HOME);
		strcat(strcat(strcat(terminal,"~"),p),"> ");
	}
	free(distro);
	return terminal;
}

char *takeinput(){
	char *line = NULL;
	size_t buff = 0;
	getline(&line, &buff, stdin);
	return line;
}

char **tokenize(char *line){
	int bufsize = TOK_BUFSIZE;
	int position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	if(!tokens){
		fprintf(stderr, "MYSHELL: Memory allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, TOK_DELIM);

	while(token!=NULL){
		tokens[position] = token;
		position++;

		if(position >= bufsize){
			bufsize += TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if(!tokens){
				fprintf(stderr, "MYSHELL: Memory allocation error");
				exit(EXIT_FAILURE);
			}
		}
		token = strtok(NULL, TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}

char **lsplit(char *line){
	int bufsize = TOK_BUFSIZE;
	int position = 0;
	char **commands = malloc(bufsize * sizeof(char*));
	char *command;

	if(!commands){
		fprintf(stderr, "MYSHELL: Memory allocation error\n");
		exit(EXIT_FAILURE);
	}

	command = strtok(line, COM_DELIM);

	while(command!=NULL){
		commands[position] = command;
		position++;

		if(position >= bufsize){
			bufsize += TOK_BUFSIZE;
			commands = realloc(commands, bufsize * sizeof(char*));
			if(!commands){
				fprintf(stderr, "MYSHELL: Memory allocation error");
				exit(EXIT_FAILURE);
			}
		}
		command = strtok(NULL, COM_DELIM);
	}
	commands[position] = NULL;
	return commands;
}

void checkchildbg()
{
	union wait wstat;
	pid_t pid;

	while(true) {
		pid = wait3 (&wstat, WNOHANG, (struct rusage *)NULL );
		if (pid == 0 || pid == -1)
			return;
		else{
			printf ("%s with pid %d exited with exit status %d\n", children[nchild-1],pid, wstat.w_retcode);
			nchild --;
		}
	}
}

void intHandler(int sig) {
	signal(sig, SIG_IGN);
}

int launch(char **args, bool bg)
{
	pid_t pid;
	int status;

	signal(SIGCHLD, checkchildbg);

	pid = fork();

	if(pid == 0){
		if(bg == true){
			if(execvp(args[0], args) == -1){
				fprintf(stderr, "%s: command not found\n", args[0]);
			}
			exit(EXIT_FAILURE);
		}
		else{
			if(execvp(args[0], args) == -1){
				fprintf(stderr, "%s: command not found\n", args[0]);
			}
		}
	}
	else if(pid < 0){
		perror("MYSHELL");
	}
	else{
		if(bg == true){

    		children[nchild] = args[0];

			printf("[%d] %d\n", 1, pid);
			nchild++;
			signal(SIGINT, intHandler);
		}
		else{
			if (nchild == 0)signal(SIGINT, SIG_DFL);
			do{
				waitpid(pid, &status, WUNTRACED);
			}while (!WIFEXITED(status) && !WIFSIGNALED(status));
		}
	}
	return 1;
}
int execute(char **args){
	int i;

	if (args[0] == NULL){
		return 1;
	}

	bool bg = false;

	for(i=1; args[i]!=NULL; i++){
		if(strcmp(args[i],"&") == 0){
			bg = true;
			args[i] = NULL;
		}
	}

	for(i=0; i<sizeof(builtins)/sizeof(char*); i++){
		if(strcmp(args[0], builtins[i]) == 0){
			return (*commands[i])(args);
		}
	}

	return launch(args, bg);
}
void mainloop(void)
{
	char *line;
	char **args;
	char **commands;
	int status = 0;
	char *terminal;

	do{
		terminal = getterminal();
		printf(RED "%s" RESET, terminal);
		line = takeinput();
		int i = 0;
		commands = lsplit(line);
		while(commands[i]!=NULL){
			args = tokenize(commands[i]);
			status = execute(args);
			free(args);
			i++;
		}
		free(commands);
		free(terminal);
	}while(status);
}

int main(int argc, char **argv)
{
	getcwd(HOME, 256);
	mainloop();
	return 0;
}