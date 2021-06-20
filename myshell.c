#include "myshell.h"
#include <errno.h>
#define MAXARGS 128

void eval(char *cmdline);
int parse_line(char *buf, char **argv);
int builtin(char **argv, int argc);
int is_pipe(char **argv);
void do_pipe(char **argv, int loc, int bg);
void do_pipe_twice(char ** argv, int loc);
void do_bg(char ** argv);
int int_in_str(char * dir_name);
void sig_hand(int sig_no);


#define READ 0
#define WRITE 1

int loc_of_pipe[MAXARGS];
int cid = -1;
char tmp_cmd[MAXLINE];

struct proc{
    int id;
    char status; //R: running S:Stopped(when ctrl+z called) T: Terminated 
    char name[MAXLINE];
}proc;

struct proc bprocs[256];
int bgp_top = 0;


int main()
{
    for (int i=0 ; i< 256 ; i++){
        bprocs[i].id = -1;
    }    
    char cmdline[MAXLINE];
    pid_t pid;
    signal(SIGTSTP, sig_hand);
    //signal(SIGCHLD, sig_hand);
    
    do
    {
        memset(tmp_cmd, '\0', sizeof(char)*MAXLINE);
        printf("CSE4100:P4-myshell>");
        if(fgets(cmdline, MAXLINE, stdin)){
            if (feof(stdin))  exit(0);
            eval(cmdline);
        }
    }while (1);
}

void eval(char *cmdline)
{
    char *argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;
    char prefix[32] = "/bin/";
    strcpy(tmp_cmd, cmdline);
    for (int i=0; i< MAXARGS; i++)
        loc_of_pipe[i] = -1;
    
    //printf("in eval, print cmdline: %s",cmdline);
    strcpy(buf, cmdline);
    bg = parse_line(buf, argv);
    
    //printf("%d\n",bg);
    if (argv[0] == NULL)
        return;
    int argc;
    for(argc=0; argv[argc];){
        argc++;
    }
    //printf("%d\n",argc);
    if(!is_pipe(argv)){
        if(!builtin(argv, argc)){
            if (strchr(argv[argc-1], '&') != NULL){
                    char * ptr = strtok(argv[argc-1],"&");
                    strcpy(argv[argc-1], ptr);
                    bg = 1;
                }
            if ((pid = Fork()) == 0){
                
                strcat(prefix, argv[0]);
                //for (int i=0; argv[i]!= NULL; i++)  printf("    %s  \n",argv[i]);
                Execve(prefix, argv, environ);
                //printf("child : %d\n",pid);
                //execvp(argv[0], argv);
            }
            if (!bg){

                    int status;
                    if (waitpid(pid, &status, WUNTRACED)<0)
                    unix_error("!wait error:");   

            }
            else{
                bprocs[bgp_top].id = pid;
                strcpy(bprocs[bgp_top].name,cmdline);
                bprocs[bgp_top++].status = 'R';
                printf("%d %s",pid, cmdline);
            }
        }
    }
    else{
        
        if (strchr(argv[argc-1], '&') != NULL){
            char * ptr = strtok(argv[argc-1],"&");
            strcpy(argv[argc-1], ptr);
            bg = 1;
        }
        //printf("&? :%d\n", bg); //bg test approb
        int loc = 0, pipe_cnt = 0;
        for (int i=0;argv[i]; i++){
            if(!strcmp(argv[i], "|"))   loc_of_pipe[loc++] = i;
        }    
        for( int i=0; loc_of_pipe[i] > 0; i++){
            //printf("%d\n",loc_of_pipe[i]);
            pipe_cnt++;
        }
        //printf("%d\n",pipe_cnt);
        
            switch(pipe_cnt){
                case 1:
                    do_pipe(argv, 0, bg);
                    break;
                case 2:
                    do_pipe_twice(argv, 0);
                    break;
            }
        
        
            
        
    }
}

int builtin(char **argv , int argc){
    if (!strcmp(argv[0], "exit") ||!strcmp(argv[0], "quit"))   exit(0);
    if (!strcmp(argv[0], "&"))      return 1;
    if (!strcmp(argv[0], "cd")){
        int pid;
        if ((pid = fork()) == 0){ //FORK!!! Why????
            printf("child PID:%d\n",getpid());
            if (chdir(argv[1])<0)   unix_error("No directoryt Or File");
                return 1;
        }
        //printf("parent PID: %d\n", getpid());
        int status;
        waitpid(pid, &status, 0);
        return 1;
    }
    if (!strcmp(argv[0], "cd..")){
        if(chdir("..")<0)       unix_error("No upper dir");
        return 1;
    }
    if (!strcmp(argv[0], "debug")){ //for debug. what you want to check
        for(int i=0;environ[i]; i++){
            printf("%s\n", environ[i]);
        }
    }
    if(!strcmp(argv[0], "jobs")){ //List the running and background jobs

        DIR *dir;
        struct dirent * proc;  
        char status[256];
        char cmds[256];
        char* dummy;
        int prefix = 0;
        for(int i=0; i< 256; i++){
            if(bprocs[i].id != -1){
                FILE * fp;
                prefix++;
                char proc_dir[128];
                sprintf(proc_dir, "/proc/%d/status",bprocs[i].id);
                fp = fopen(proc_dir, "r");
                if (fp == NULL)  continue;
                dummy = fgets(status, 256, fp);
                //printf("%s", dummy);
                if(fgets(status, 256, fp)){
                    fclose(fp);
                    sprintf(proc_dir, "/proc/%d/cmdline",bprocs[i].id);
                    fp = fopen(proc_dir, "r");
                    if( fp == NULL) continue;
                    dummy = fgets(cmds, 256, fp);
                    switch (status[7]){
                        case 'R':
                            printf("[%d]  running       %s\n",prefix, bprocs[i].name);
                            break;
                        case 'T':
                            printf("[%d]  Stopped       %s\n",prefix, bprocs[i].name);
                            break;
                        case 'S':
                            printf("[%d]  Sleeping      %s\n",prefix, bprocs[i].name);
                            break;


                    }
                }
                fclose(fp);
                
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "bg")){
        //background
        int pid_;
        pid_ = atoi(argv[1]);
        //printf("%d\n", pid_);
        for(int i=0; i<bgp_top; i++){
            if(bprocs[i].id == pid_){
                kill(pid_, SIGCONT);
                int status;
                //waitpid(pid_, &status, WUNTRACED);
            }
        }

        return 1;
    }
    if(!strcmp(argv[0], "fg")){
        int pid_;
        pid_ = atoi(argv[1]);
        //printf("%d\n", pid_);
        for(int i=0; i<bgp_top; i++){
            if(bprocs[i].id == pid_){
                kill(pid_, SIGCONT);
                int status;
                waitpid(pid_, &status, WUNTRACED);
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "kill")){
        int pid_;
        pid_ = atoi(argv[1]);
        for (int i=0 ; i<bgp_top; i++){
            if(bprocs[i].id == pid_){
                kill(pid_, SIGTERM);
                int status;
                char stats[128];
                char dir[256];
                char * dummy;
                FILE *fp;

                sprintf(dir, "/proc/%d/status", pid_);
                fp = fopen(dir, "r");
                if(!fp) break;
                dummy = fgets(stats, 128, fp);
                dummy = fgets(stats, 128, fp);
                if(stats[7] != 'T') 
                    waitpid(pid_, &status, WUNTRACED); // Zombie killer
            }
        }
        return 1;
    }
    return 0;
}

int parse_line(char *buf, char **argv){
    char *delim;
    int argc;
    int bg;
    //printf("in parse, print cmdline(a.k.a buf: %s",buf);
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    argc = 0;
    while((delim = strchr(buf,' '))){
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while(*buf && (*buf == ' '))    buf++;
    }
    //printf("argc: %d\n", argc);
    argv[argc]=NULL;

    if (argc == 0)  return 1;

    if ((bg = (*argv[argc-1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}

pid_t Fork(void)
{
    pid_t pid;
    pid = fork();
    cid = pid;
    if (pid < 0)    unix_error("fork error");
    return pid;
}

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0){
        printf("%s: Command not found.\n", argv[0]);
        exit(0);
    }    
}

void unix_error(char *msg){
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

int is_pipe(char **argv){
    for(int i=0; argv[i]; i++){
        if(!strcmp(argv[i],"|"))    return 1; //it's pipe
    }
    return 0; //no pipe
}

void do_pipe(char **argv, int cur, int bg){ //initial pipe
    if (is_pipe(argv) ==0 ){
        exit(0);
    }
    int pid ,pid2;
    int fd[2]; //fd[0] = write fd[1] = read
    int location_of_pipe = loc_of_pipe[cur] + 1;
    //printf("pipe: %d, Loc: %d\n", cur, loc_of_pipe[cur]);

        if (pipe(fd) < 0)   unix_error("pipe error");
        if((pid = fork()) < 0) unix_error("fork error");
        else if (pid == 0){ //child
            //printf("child\n");
            //printf("Child PID: %d, Parent's PID:%d\n", getpid(), getppid());
            char * arg_[MAXARGS];
            int argc = 0;
            int y;
            if( cur == 0 )  y = 0;
            else            y = loc_of_pipe[cur - 1];
            for (; strcmp(argv[y],"|") != 0 ; y++){
                arg_[argc++] = argv[y];
                //printf("%s\t", argv[y]);
            }
            //printf("it's child\n");

            arg_[argc] = NULL;
            dup2(fd[1], 1);
            close(fd[0]);
            execvp(arg_[0], arg_);
            unix_error("pipe error");
        }
        else{ /*parents*/
            // split argv by |
            //printf("parents\n");
            //printf("parents PID:%d child's PID : %d\n", getpid(), pid);
            char * arg_[MAXARGS];
            int argc = 0;
            for (int y = location_of_pipe; argv[y]; y++){
                char *ptr = strchr(argv[y], '\"');
                if (ptr){
                    char tmp[NAME_MAX];
                    strncpy(tmp, ptr + 1, strlen(ptr) - 2);
                    arg_[argc++] = tmp;
                }
                else{
                    arg_[argc++] = argv[y];
                }
                //printf("%s\t", argv[y]);
            }
            //printf("it's parent\n");
            arg_[argc] = NULL;
            //printf("%d\n",cur);
            if((pid2 = fork()) == 0){
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                execvp(arg_[0], arg_);
                unix_error("Command Error");
            }
            close(fd[1]);
            //close(fd[0]);
            int status, status2;
             //if defunct this line, it for bg
            if(!bg){
            waitpid(pid, &status, WUNTRACED);
            waitpid(pid2, &status2, WUNTRACED);
            }
            if(bg){
                bprocs[bgp_top].id = pid;
                strcpy(bprocs[bgp_top++].name,tmp_cmd);
            }
            return;
        }
    return;
}

void do_pipe_twice(char ** argv, int cur){
    if (is_pipe(argv) ==0 ){
        exit(0);
    }
    int pid ,pid2, pid3;
    int fd[2]; //fd[0] = write fd[1] = read
    int fd2[2];
    int location_of_pipe = loc_of_pipe[cur] + 1;
    //printf("pipe: %d, Loc: %d\n", cur, loc_of_pipe[cur]);

        if (pipe(fd) < 0)   unix_error("pipe error");
        if(pipe(fd2) < 0)   unix_error("pipe error");
        if((pid = fork()) < 0) unix_error("fork error");
        else if (pid == 0){ //child
            //printf("child\n");
            //printf("Child PID: %d, Parent's PID:%d\n", getpid(), getppid());
            pid2 = fork();
            if (pid2 == 0){
                char * arg_[MAXARGS];
                int argc = 0;
                int y;
                if( cur == 0 )  y = 0;
                else            y = loc_of_pipe[cur - 1];
                for (; strcmp(argv[y],"|") != 0 ; y++){
                    arg_[argc++] = argv[y];
                //printf("%s\t", argv[y]);
                }
                //printf("it's child\n");

                arg_[argc] = NULL;
                dup2(fd[1], 1);
                close(fd[0]);
                execvp(arg_[0], arg_);
                unix_error("pipe error");
            }
            else{
                char * arg_[MAXARGS];
                int argc = 0;
                for (int y = location_of_pipe; strcmp(argv[y], "|") != 0; y++){
                    char *ptr = strchr(argv[y], '\"');
                    if (ptr){
                        char tmp[NAME_MAX];
                        strncpy(tmp, ptr + 1, strlen(ptr) - 2);
                        arg_[argc++] = tmp;
                    }
                    else{
                        arg_[argc++] = argv[y];
                    }
                //printf("%s\t", argv[y]);
                }
            //printf("it's parent\n");
                arg_[argc] = NULL;
            //printf("%d\n",cur);
                if((pid2 = fork()) == 0){
                    dup2(fd[0], STDIN_FILENO);
                    //dup2(fd2[1], STDOUT_FILENO);
                    close(fd[1]);
                    close(fd2[0]);
                    execvp(arg_[0], arg_);
                    unix_error("Command Error");
                }
                close(fd[1]);
                
               
            }
        }
        /*
        else{ /*parents*//*
            char * arg_[MAXARGS];
            int argc= 0 ;
            for (int y = loc_of_pipe[1] + 1; argv[y]; y++){
                char *ptr = strchr(argv[y], '\"');
                if (ptr){
                    char tmp[NAME_MAX];
                    strncpy(tmp, ptr + 1, strlen(ptr) - 2);
                    arg_[argc++] = tmp;
                }
                else{
                    arg_[argc++] = argv[y];
                }
                //printf("%s\t", argv[y]);
            }
            //printf("it's parent\n");
            arg_[argc] = NULL;

            dup2(fd2[0], STDIN_FILENO);
            close(fd2[1]);
            execvp(arg_[0], arg_);
        }
        close(fd2[1]);
        */
        int status;
        int status2;
        waitpid(pid2, &status2, WUNTRACED);
        waitpid(pid, &status, WUNTRACED);
    return;
}
    

int int_in_str(char * str){
    int ret_val = (strchr(str, '1') == NULL) && (strchr(str, '2') == NULL) &&
                    (strchr(str, '3') == NULL) && (strchr(str, '4') == NULL) &&
                    (strchr(str, '5') == NULL) && (strchr(str,'6') == NULL) &&
                    (strchr(str, '7') == NULL) && (strchr(str, '8') == NULL) &&
                    (strchr(str,'9') == NULL) && (strchr(str,'0') == NULL);
    return  ret_val; //1 if ther is no int
}

void sig_hand(int sig_no){
    if(cid > 0){
        int pid = 0;
        switch(sig_no){
            case SIGTSTP:    
                printf("\nSIGNO: %d     %d\n",sig_no,   cid);
                bprocs[bgp_top].id = cid;
                strcpy(bprocs[bgp_top++].name, tmp_cmd);
                kill(0, SIGCHLD);
                break;
            case SIGCHLD:
                while(pid != -1)
                    pid = wait(NULL);
                break;

        }
    }
}