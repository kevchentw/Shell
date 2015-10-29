#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"
#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

//color
#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

struct command_segment {
    char **args;   // arguments array
    struct command_segment *next;
    pid_t pid;   // process ID
    pid_t pgid;   // process group ID
};

struct command {
    struct command_segment *root;   // a linked list
    int mode;   // BACKGROUND_EXECUTION or FOREGROUND_EXECUTION
};


int mysh_cd(char *path) {
    /* Implement cd command */
    if (chdir(path)!=0){
        printf("mysh : errno(%d)\n", errno);
    }
    return 1;
}

int mysh_fg(pid_t pid) {
    int status;
    printf("fg gpid: %d\n", pid);
    if (kill (pid, SIGCONT) < 0){
        perror ("kill (SIGCONT)");
    }
    tcsetpgrp(0, pid);
    waitpid(pid, &status, WUNTRACED);
    tcsetpgrp(0, getpid());
}

int mysh_bg(pid_t pid) {
    /* Implement bg command */
    printf("bg gpid: %d\n", pid);
    if (kill (pid, SIGCONT) < 0){
        perror ("kill (SIGCONT)");
    }
}

int mysh_kill(pid_t pid) {
    printf("kill: process pgid=%d\n", pid);
    kill(pid, SIGKILL);
}

int mysh_exit() {
    /* Release all the child processes */
    printf("Goodbye! QQ\n");
    exit(0);
}

int mysh_execute_builtin_command(struct command_segment *segment) {
    if(strcmp(segment->args[0], "cd")==0){
        mysh_cd(segment->args[1]);
        return 1;
    }
    else if(strcmp(segment->args[0], "fg")==0){
        printf("fgfgfg\n");
        mysh_fg(atoi(segment->args[1]));
        return 1;
    }
    else if(strcmp(segment->args[0], "bg")==0){
        mysh_bg(atoi(segment->args[1]));
        return 1;
    }
    else if(strcmp(segment->args[0], "kill")==0){
        mysh_kill(atoi(segment->args[1]));
        return 1;
    }
    else if(strcmp(segment->args[0], "exit")==0){
        mysh_exit();
        return 1;
    }
    else{
        return 0;
    }
}
int mysh_execute_command_segment(struct command_segment *segment, int in_fd, int out_fd, int mode, int pgid, int counter, int total) {
    if (mysh_execute_builtin_command(segment)) {
        return 0;
    }
    int pid, status, exec_status;
    pid = fork();
    struct command_segment *i;
    if(pid==0){
        if(counter==0 && counter!=total-1){
            close(in_fd);
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        else if(counter!=total-1){
            dup2(in_fd, STDIN_FILENO);
            dup2(out_fd, STDOUT_FILENO);
            close(in_fd);
            close(out_fd);
        }
        else{
            close(out_fd);
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        exec_status = execvp(segment->args[0], segment->args);
        if(exec_status==-1){
            printf("command not found\n");
            exit(0);
        }
    }
    else if(pid>0){
        printf("Command executed by pid=%d\n", pid);
        close(in_fd);
        close(out_fd);
        segment->pid = pid;
        if(pgid==0){
            segment->pgid = pid;
        }
        else{
            segment->pgid = pgid;
        }
        setpgid(pid, segment->pgid);
        if(mode==BACKGROUND_EXECUTION){
            mysh_bg(pid);
        }
    }
    else{
        printf("fork error QQ\n");
    }
}

int mysh_execute_command(struct command *command) {
    struct command_segment *cur_segment;

    // Iterate the linked list of command segment
    // If this is not a pipeline command, there is only a root segment.
    int pipe_fd[1000][2];
    int p_gid = 0;
    int counter = 0;
    int total = 0;
    for (cur_segment = command->root; cur_segment != NULL; cur_segment = cur_segment->next) {
        total++;
    }
    pipe(pipe_fd[0]);
    close(pipe_fd[0][1]);
    for (cur_segment = command->root; cur_segment != NULL; cur_segment = cur_segment->next) {
        if (pipe(pipe_fd[counter+1])==-1){
            printf("pipe error");
        }
        mysh_execute_command_segment(cur_segment, pipe_fd[counter][0], pipe_fd[counter+1][1], command->mode, p_gid, counter, total);
        p_gid = cur_segment->pgid;
        if(counter==0) {
            tcsetpgrp(0, cur_segment->pid);
        }
        counter++;
    }
    close(pipe_fd[counter][0]);
    int status;
    for (int i = 0; i < counter && command->mode == FOREGROUND_EXECUTION; ++i) {
        waitpid(-command->root->pgid, &status, WUNTRACED);
    }
    tcsetpgrp(0, getpid());
    /* Return status */
    return 0;
}

struct command_segment* mysh_parse_command_segment(char *segment) {
    struct command_segment *c = (struct command_segment*)malloc(sizeof(struct command_segment));
    char** args = (char**)malloc(100*sizeof(char *));
    int args_counter = 0;
    char* arg = strtok(segment, " \r\n");
    while(arg != NULL){
        args[args_counter++] = arg;
        arg = strtok(NULL, " \r\n");
    }
    c->args = args;
    c->next = NULL;
    return c;
}

struct command* mysh_parse_command(char *line) {
    /* Parse line as command structure */
    int mode = FOREGROUND_EXECUTION;
    for (int j = strlen(line)-1; j >= 0; j--) {
        if(line[j]=='&'){
            mode = BACKGROUND_EXECUTION;
            line[j]=' ';
        }
        if(line[j]!=' '){
            break;
        }
    }
    struct command *cmd = (struct command*)malloc(sizeof(struct command));
    char* pipeline = strtok(line, "|");
    char** pipeline_args = (char**)malloc(100*sizeof(char *));
    int pipeline_counter = 0;
    while(pipeline != NULL){
        pipeline_args[pipeline_counter++] = pipeline;
        pipeline = strtok(NULL, "|");
    }
    cmd->root = mysh_parse_command_segment(pipeline_args[0]);
    struct command_segment *next_command = cmd->root;
    for (int i = 1; i < pipeline_counter; i++) {
        next_command->next = mysh_parse_command_segment(pipeline_args[i]);
        next_command = next_command->next;
    }
    next_command = NULL;
    cmd->mode = mode;
    return cmd;
}

char* mysh_read_line() {
    int bufsize = COMMAND_BUFSIZE;
    int position = 0;
    char *buffer = (char *)malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "-mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {    // read just one line per time
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;
        if (position >= bufsize) {   // handle overflow case
            bufsize += COMMAND_BUFSIZE;
            buffer = (char *)realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "-mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void mysh_print_promt() {
    /* Print "<username> in <current working directory>" */
    char *username;
    char *pwd;
    username = (char *)malloc(20 * sizeof(char));
    pwd = (char *)malloc(100 * sizeof(char));
    getlogin_r(username, 20);
    getcwd(pwd, 100);
    printf("\n");
    printf(LIGHT_BLUE"┌─[");
    printf(GREEN"%s", username);
    printf(LIGHT_BLUE"]");
    printf(WHITE" - ");
    printf(LIGHT_BLUE"[");
    printf(WHITE"%s", pwd);
    printf(LIGHT_BLUE"]\n");
    printf(LIGHT_BLUE"└─[");
    printf(WHITE"mysh");
    printf(LIGHT_BLUE"]>"NONE);
}

void mysh_print_welcome() {
    printf(LIGHT_GRAY"Welcome to mysh by 0316317!\n");
}

void mysh_loop() {
    char *line;
    struct command *command;
    int status = 1;

    do {   // an infinite loop to handle commands
        mysh_print_promt();
        line = mysh_read_line();   // read one line from terminal
        if (strlen(line) == 0) {
            continue;
        }
        command = mysh_parse_command(line);   // parse line as command structure
        status = mysh_execute_command(command);   // execute the command
        free(line);
    } while (status >= 0);
}

void handle_signal(int signal) {
    const char *signal_name;
    sigset_t pending;

    // Find out which signal we're handling
    switch (signal) {
        case SIGINT:
            signal_name = "SIGINT";
//            mysh_print_promt();
        case SIGTSTP:
            signal_name = "SIGTSTP";
            break;
        case SIGCHLD:
            signal_name = "SIGCHLD";
            int pid, status;
            while ((pid=waitpid(-1, &status, WNOHANG)) > 0) {
                printf("done killing child pid=%d\n", pid);
            }
            break;
        default:
            fprintf(stderr, "Caught wrong signal: %d\n", signal);
            return;
    }
}

void mysh_init() {
    /* Do any initializations here. You may need to set handlers for signals */
    struct sigaction sa;
    sa.sa_handler = &handle_signal;

    // Restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;

    // Block every signal during the handler
    sigfillset(&sa.sa_mask);

    struct sigaction sa_ign;
    sa_ign.sa_handler = SIG_IGN;

    // Restart the system call, if at all possible
    sa_ign.sa_flags = SA_RESETHAND;
    // Block every signal during the handler
    sigfillset(&sa_ign.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT"); // Should not happen
    }
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGTSTP"); // Should not happen
    }
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGCHLD"); // Should not happen
    }
    if (sigaction(SIGTTOU, &sa_ign, NULL) == -1) {
        perror("Error: cannot handle SIGTTOU"); // Should not happen
    }
    if (sigaction(SIGTTIN, &sa_ign, NULL) == -1) {
        perror("Error: cannot handle SIGTTIN"); // Should not happen
    }
}

int main(int argc, char **argv) {
    printf("Shell pid = %d\n", getpid());
    mysh_init();
    mysh_print_welcome();
    mysh_loop();
    return EXIT_SUCCESS;
}