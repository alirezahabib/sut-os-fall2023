#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int size_of(char **argv) {
    if (!argv) return 0;
    int argc = 0;
    while (argv[argc]) argc++;
    return argc;
}

int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

int cmd_help(tok_t arg[]);

void run_external_cmd(tok_t arg[]);

process *get_process(int pid);

int cmd_pwd(tok_t arg[]) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);
    else perror("getcwd");
    return 1;
}

int cmd_cd(tok_t arg[]) {
    if (!arg[0]) fprintf(stderr, "cd: missing argument\n");
    else if (chdir(arg[0]) != 0) perror("cd");
    return 1;
}

int cmd_wait(tok_t arg[]) {
    int *status;
    while (waitpid(-1, status, 0) > 0);
    return 1;
}

/* Command Lookup table */
typedef int cmd_fun_t(tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
        {cmd_help, "?",    "show this help menu"},
        {cmd_quit, "quit", "quit the command shell"},
        {cmd_cd,   "cd",   "change the current working directory"},
        {cmd_pwd,  "pwd",  "print the current working directory"},
        {cmd_wait, "wait", "wait for all background processes"}
};

int cmd_help(tok_t arg[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}


int lookup(char cmd[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    return -1;
}

void init_shell() {
    /* Check if we are running interactively */
    shell_terminal = STDIN_FILENO;

    /** Note that we cannot take control of the terminal if the shell
        is not interactive */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {

        /* force into foreground */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        /* Put shell in its own process group */
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
    }
    first_process = malloc(sizeof(process));
    first_process->pid = getpid();

}

/**
 * Add a process to our process list
 */
void add_process(process *p) {
    process *last_process = first_process;

    // Forward to the last process
    while (last_process->next) last_process = last_process->next;

    // Ex-
    last_process->next = p;
    p->prev = last_process;
}


/**
 * Creates a process given the inputString from stdin
 */
process *create_process(tok_t *inputString) {
    process *p = malloc(sizeof(process));

    p->argv = inputString;
    p->argc = size_of(p->argv);
    p->completed = FALSE;
    p->background = FALSE;
    p->stopped = FALSE;
    p->status = 0;

    p->stdin = STDIN_FILENO;
    p->stdout = STDOUT_FILENO;
    p->stderr = STDERR_FILENO;
    printf("##### debug 1, %d\n", p->argc);

    for (int i = 0; i < p->argc - 1; i++) {
        if (strncmp(inputString[i], "<", 1) == 0) {
            int file = open(inputString[i + 1], O_RDONLY);
            if (file >= 0) {
                p->stdin = file;
                printf("debug 1.5\n");
            }
            else {
                perror("open");
                exit(0);
            }
            p->argv[i] = NULL;
            p->argv[i + 1] = NULL;
        } else if (strncmp(inputString[i], ">", 1) == 0) {
            int file = open(p->argv[i + 1], O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
            if (file >= 0) p->stdout = file;
            else {
                perror("open");
                exit(0);
            }
            p->argv[i] = NULL;
            p->argv[i + 1] = NULL;
        }
    }

    p->argc = size_of(p->argv);

    printf("##### debug 2, %d\n", p->argc);


//    if (strcmp(p->argv[p->argc - 1], "&") == 0) {
//        p->background = TRUE;
//        p->argv[p->argc - 1] = NULL;
//        p->argc--;
//    }

    printf("##### debug 3\n");
    return p;
}


int shell(int argc, char *argv[]) {
    char *s = malloc(INPUT_STRING_SIZE + 1);            /* user input string */
    tok_t *t;            /* tokens parsed from input */
    int lineNum = 0;
    int fundex = -1;
    pid_t pid = getpid();        /* get current processes PID */
    pid_t ppid = getppid();    /* get parents PID */
    pid_t cpid, tcpid, cpgid;

    init_shell();

    // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

    lineNum = 0;
    // fprintf(stdout, "%d: ", lineNum);
    while ((s = freadln(stdin))) {
        t = getToks(s); /* break the line into tokens */
        fundex = lookup(t[0]); /* Is first token a shell literal */
        if (fundex >= 0) cmd_table[fundex].fun(&t[1]);
        else if (t[0]) run_external_cmd(t);
        // fprintf(stdout, "%d: ", lineNum);
    }

    return 0;
}


void run_external_cmd(tok_t argv[]) {
    process *p = create_process(argv);
    add_process(p);

    printf("##### debug 4\n");
    int pid = fork();
    printf("##### debug 5\n");

    if (pid == 0) {
        // child
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        p->pid = getpid();
        launch_process(p);
    } else if (pid > 0) {
        // parent
        p->pid = pid;
        printf("debug 9.5\n");
        int parentPID = getpid();
        printf("debug 10\n");
        setpgid(parentPID, parentPID);
        if (!p->background) {
            printf("debug 11\n");
            tcsetpgrp(STDIN_FILENO, parentPID);
            printf("debug 12\n");
            int *status;
            waitpid(pid, status, 2);

            process *child = get_process(pid);
            if (child) child->completed = TRUE;
        }
    } else perror("fork");
}

process *get_process(int pid) {
    process *p = first_process;

    do if (p->pid == pid) return p;
    while ((p = p->next));

    return NULL;
}
