extern int run_cd(char **args);
extern int run_exit(char **args);
extern int run_echo(char **args);
extern int run_ls(char **args);
extern int run_pwd(char **args);
extern int run_pinfo(char **args);
extern int run_interrupt(char **args);
extern int run_nightswatch(char **args);	

#define TOK_BUFSIZE 64
#define COM_DELIM ";"
#define TOK_DELIM " \t\r\n\a"
#define IN_DELIM "<"
#define OUT_DELIM ">"

#define RESET "\x1b[0m"
#define BRIGHT "\x1b[1m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"

char HOME[256];

struct termios orig_termios;

char *builtins[] = {
	"cd",
	"exit",
	"echo",
	"ls",
	"pwd",
	"pinfo",
	"interrupt",
	"nightswatch"
};

int (*commands[])(char **) = {
	&run_cd,
	&run_exit,
	&run_echo,
	&run_ls,
	&run_pwd,
	&run_pinfo,
	&run_interrupt,
	&run_nightswatch
};

int *checklsflags(char **args, char allowedflags[]){
	int i,j,k;

	int *returnarray = malloc(3*sizeof(int));
	returnarray[0] = 0;
	returnarray[1] = 0;

	for(i=1; args[i]!=NULL; i++){
		if(args[i][0]=='-'){
			for(j=1; j<strlen(args[i]); j++){
				for(k=0; k < strlen(allowedflags)/sizeof(char); k++){
					if(args[i][j] != allowedflags[k] && k == strlen(allowedflags)/sizeof(char) - 1){
						fprintf(stderr, "ls: invalid option -- '%c'", args[i][j]);
						returnarray[0] = 0;
						returnarray[1] = 0;
						return returnarray;
					}
					else if(k==0 && returnarray[0]==0 && args[i][j] == allowedflags[k]){
						returnarray[0] = 1;
						break;
					}
					else if(k==1 && returnarray[0]==0 && args[i][j] == allowedflags[k]){
						returnarray[0] = 2;
						break;
					}
					else if((k==0 && returnarray[0] == 1 && args[i][j] != allowedflags[k])||(k==1 && returnarray[0] == 2 && args[i][j] != allowedflags[k])){
						returnarray[0] = 3;
						break;
					}
				}
				if(returnarray[0] == 3){
					break;
				}
			}
		}
		else{
			returnarray[1] += 1;
		}
	}
	return returnarray;
}

char checkreadperms(struct stat *filestat, char *person){
	if(strcmp(person,"user")==0 && (filestat->st_mode & S_IRUSR)) return 'r';
	else if(strcmp(person,"group")==0 && (filestat->st_mode & S_IRGRP)) return 'r';
	else if(strcmp(person,"others")==0 && (filestat->st_mode & S_IROTH)) return 'r';
	return '-';
}

char checkwriteperms(struct stat *filestat, char *person){
	if(strcmp(person,"user")==0 && (filestat->st_mode & S_IWUSR)) return 'w';
	else if(strcmp(person,"group")==0 && (filestat->st_mode & S_IWGRP)) return 'w';
	else if(strcmp(person,"others")==0 && (filestat->st_mode & S_IWOTH)) return 'w';
	return '-';
}

char checkexecperms(struct stat *filestat, char *person){
	if(strcmp(person,"user")==0 && (filestat->st_mode & S_IXUSR)) return 'x';
	else if(strcmp(person,"group")==0 && (filestat->st_mode & S_IXGRP)) return 'x';
	else if(strcmp(person,"others")==0 && (filestat->st_mode & S_IXOTH)) return 'x';
	return '-';
}

char setfiletype(struct stat *filestat){
	if(S_ISDIR(filestat->st_mode)==1) return 'd';
	else if(S_ISCHR(filestat->st_mode)==1) return 'c';
	else if(S_ISBLK(filestat->st_mode)==1) return 'b';
	else if(S_ISFIFO(filestat->st_mode)==1) return 'p';
	else if(S_ISLNK(filestat->st_mode)==1) return 'l';
	else if(S_ISSOCK(filestat->st_mode)==1) return 's';
	return '-';
}

void printlslong(char *args, struct dirent *myfile)
{
	char filename[256];
	char fileperms[11] = "----------";
	char datetime[64];
	struct stat *file;
	struct passwd *user;
	struct group *grp;
	file = malloc(sizeof(struct stat));


	sprintf(filename, "%s/%s", args, myfile->d_name);
	stat(filename, file);
	strftime(datetime, 64, "%h %e %H:%M", localtime(&(file->st_mtime)));

	user = getpwuid(file->st_uid);
	grp = getgrgid(file->st_gid);

	fileperms[0] = setfiletype(file);
	fileperms[1] = checkreadperms(file, "user");
	fileperms[2] = checkwriteperms(file, "user");
	fileperms[3] = checkexecperms(file, "user");
	fileperms[4] = checkreadperms(file, "group");
	fileperms[5] = checkwriteperms(file, "group");
	fileperms[6] = checkexecperms(file, "group");
	fileperms[7] = checkreadperms(file, "others");
	fileperms[8] = checkwriteperms(file, "others");
	fileperms[9] = checkexecperms(file, "others");

	printf("%s ", fileperms);
	printf("%u ", (unsigned)file->st_nlink);
	printf("%s ", user->pw_name);
	printf("%s ", grp->gr_name);
	printf("%7d ", (int)file->st_size);
	printf("%s ", datetime);
	if(fileperms[0] == 'd'){
		printf(BRIGHT BLUE "%s " RESET, myfile->d_name);	
	}
	else if(fileperms[0] == 'l'){
		printf(BRIGHT CYAN "%s " RESET, myfile->d_name);
	}
	else if(fileperms[0] == '-'){
		printf("%s ", myfile->d_name);
	}
}

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

int run_cd(char **args){
	if (args[1] == NULL){
		chdir(HOME);
	}
	else{
		if (chdir(args[1]) != 0){
			perror("MYSHELL");
		}
	}
	return 1;
}

int run_exit(char **args){
	return 0;
}

int run_echo(char **args){
	int i;
	for(i=1; args[i]!=NULL; i++)
	{
		printf("%s ", args[i]);
	}
	printf("\n");
	return 1;
}


int run_ls(char **args){
	char allowedflags[] ={'l','a'};

	char filename[256];

	char callfolder[256];

	struct stat *filestat;
	filestat = malloc(sizeof(struct stat));

	int *checkedargs = checklsflags(args, allowedflags);

	int flag = checkedargs[0];
	int numfiles = checkedargs[1];

	int i;
	
	getcwd(callfolder, 256);

	DIR *mydir;

	struct dirent *myfile1;
	struct dirent **myfile2;

	if(numfiles > 0){
		for (i = 1; args[i]!=NULL; i++){
			if(args[i][0] != '-'){
				mydir = opendir(args[i]);

				if(mydir == NULL){
					sprintf(filename, "ls: cannot access %s", args[i]);
					perror(filename);
					continue;
				}
				else if(numfiles > 1){
					printf("%s:\n", args[i]);
				}

				int total = 0;

				while((myfile1 = readdir(mydir)) != NULL){
					sprintf(filename, "%s/%s", args[i], myfile1->d_name);
					stat(filename,filestat);
					if (fmod((float)filestat->st_size/(float)filestat->st_blksize, 1.0) != 0){
						total += ((int)filestat->st_size/(int)filestat->st_blksize + 1)*4;
					}
					else{
							total += ((int)filestat->st_size/(int)filestat->st_blksize) * 4;
						}
				}

				if(flag == 1 || flag == 3)printf("total %d\n",total);
				
				closedir(mydir);

				int n = scandir(args[i], &myfile2, NULL, alphasort);
				int j = 0;

				while(j < n)
				{	
					if(flag == 0 && myfile2[j]->d_name[0] != '.'){
						printf("%s  ", myfile2[j]->d_name);
					}
					else if(flag == 2)
					{
						printf("%s  ", myfile2[j]->d_name);
					}
					else if(flag == 1 && myfile2[j]->d_name[0] != '.'){
						printlslong(args[i], myfile2[j]);
						printf("\n");
					}
					else if(flag == 3){
						printlslong(args[i], myfile2[j]);
						printf("\n");
					}
					free(myfile2[j]);
					++j;
				}
				free(myfile2);
				if(flag == 2 || flag == 0) printf("\n");
				if(args[i+1] != NULL) printf("\n");
			}
		}
	}
	else{
		mydir = opendir(callfolder);

		if(mydir == NULL){
			sprintf(filename, "ls: cannot access %s", callfolder);
			perror(filename);
			return 1;
		}

		int total = 0;

		while((myfile1 = readdir(mydir)) != NULL){
			sprintf(filename, "%s/%s", callfolder, myfile1->d_name);
			stat(filename,filestat);
			if (fmod((float)filestat->st_size/(float)filestat->st_blksize, 1.0) != 0){
				total += ((int)filestat->st_size/(int)filestat->st_blksize + 1)*4;
			}
			else{
				total += ((int)filestat->st_size/(int)filestat->st_blksize) * 4;
			}
		}

		if(flag == 1 || flag == 3)printf("total %d\n",total);

		closedir(mydir);

		int n = scandir(callfolder, &myfile2, NULL, alphasort);
		int j = 0;

		while(j < n)
		{	
			if(flag == 0 && myfile2[j]->d_name[0] != '.'){
				printf("%s  ", myfile2[j]->d_name);
			}
			else if(flag == 2)
			{
				printf("%s  ", myfile2[j]->d_name);
			}
			else if(flag == 1 && myfile2[j]->d_name[0] != '.'){
				printlslong(callfolder, myfile2[j]);
				printf("\n");
			}
			else if(flag == 3){
				printlslong(callfolder, myfile2[j]);
				printf("\n");
			}
			free(myfile2[j]);
			++j;
		}
		free(myfile2);
		if(flag == 2 || flag == 0) printf("\n");
	}
	return 1;
}

int run_pwd(char **args){
	char dir[256];
	getcwd(dir, 256);
	if(strstr(dir, HOME) == NULL){
		printf("%s\n", dir);
	}
	else{
		char *p = dir;
		p = p + strlen(HOME);
		sprintf(dir, "~%s", p);
		printf("%s\n", dir);
	}
	return 1;
}

int run_pinfo(char **args){

	int pid;

	if(args[1]==NULL){
		pid = getpid();
	}
	else{
		pid = atoi(args[1]);
	}

	char file[64];
	char filename[64];
	char path[PATH_MAX];
	char *detail;
	char line[256];

	sprintf(file,"/proc/%d/status",pid);
	sprintf(filename,"/proc/%d/exe",pid);

	FILE *fd = fopen(file, "r");

	if(fd == NULL) {
		fprintf(stderr, "pinfo: No process with process id %d found\n", pid);
		return 1;
	}

	ssize_t len = readlink(filename, path, sizeof(path)-1);
	path[len] = '\0';

	while(fgets(line, sizeof(line), fd)) {
		detail = strtok(line, TOK_DELIM);
		if(strcmp(detail, "State:")==0){
			printf("Process Status -- %s\n",strtok(NULL, TOK_DELIM));
		}
		else if(strcmp(detail, "VmSize:")==0){
			printf("Memory -- %s\n", strtok(NULL, TOK_DELIM));
			break;
		}
	}
	fclose(fd);
	printf("Executable Path -- %s\n", path);
	return 1;
}

int run_interrupt(char **args){
	FILE *fd = fopen("/proc/interrupts", "r");
	char line[256];
	char *detail;
	int numcpus = 0;
	int i = 0;

	fgets(line, sizeof(line), fd);

	detail = strtok(line, TOK_DELIM);
	numcpus++;

	while(detail!=NULL){
		detail = strtok(NULL, TOK_DELIM);
		numcpus++;
	}
	numcpus--;

	while(i<numcpus){
		fprintf(stderr,"CPU%d\t", i);
		i++;
	}

	fprintf(stderr,"\n\r");

	while(fgets(line, sizeof(line), fd)) {
		detail = strtok(line, TOK_DELIM);
		if(strcmp(detail, "1:") == 0){
			i = 0;
			while(i<numcpus){
				printf("%4s\t",strtok(NULL, TOK_DELIM));
				i++;
			}
			printf("\n\r");
		}
	}
	return 1;
}

int run_nightswatch(char **args){
	char c = 'z';
	float n = 0;
	int i;


	clock_t start, end;
	double cpu_time_used;

	start = clock();

	if(strcmp(args[1],"-n")){
		fprintf(stderr,"nightswatch: invalid argument %s", args[1]);
		return 1;
	}
	else if((int)args[2] != args[2]){
		fprintf(stderr, "nightswatch: argument 2 should be of type 'int'\n");
		return 1;
	}
	else if (strcmp(args[3],"interrupt")!=0){
		fprintf(stderr, "nightswatch: Sorry,%s is not supported as an argument currently\n", args[3]);
		return 1;
	}
	else{

		for(i=strlen(args[2])-1; i>=0; i--){
			n += ((int)args[2][i-strlen(args[2])+1] - 48) * pow(10,i);
		}

		set_conio_terminal_mode();
		while(c!='q'){
			while (!kbhit()) {
				end = clock();
				cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
				if(fmod(cpu_time_used, n) == 0){
					run_interrupt(args);
					fflush(stdout);
				}
        	}
        	c = getch();
        }
        reset_terminal_mode();
        run_exit(args);
	}
	return 1;
}