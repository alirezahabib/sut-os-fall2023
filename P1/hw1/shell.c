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


#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

int cmd_help(tok_t arg[]);

void run_file(tok_t *arg);

int calcArgC(char **argv);

process *find_process(int pid);

int cmd_cd(tok_t arg[]) {
    if (chdir(arg[0]) == 0) {
//        printf("")
    } else {
        perror("chdir");
    }
    return 0;
}

int cmd_pwd(tok_t arg[]) {
    char dir[1024];
    if (getcwd(dir, sizeof(dir)) != NULL) {
        printf("%s\n", dir);
        return 0;
    } else {
        perror("getcwd");
        return 1;
    }
}

int cmd_wait(tok_t arg[]) {
    process *p = first_process;

    while (p != NULL) {
        if (!p->completed && p->background) {
            int *status;
            waitpid(p->pid, status, 2);

            process *child = find_process(p->pid);
            if (child != NULL) {
                child->completed = TRUE;
            }
        }
        p = p->next;
    }

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
        {cmd_pwd,  "pwd",  "print the current directory"},
        {cmd_cd,   "cd",   "change the current directory"},
        {cmd_wait, "wait", "wait for all background processes to complete"}
};

int cmd_help(tok_t arg[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}


int lookup(char cmd[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
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
    /** YOUR CODE HERE */
    first_process = malloc(sizeof(process));
    first_process->pid = getpid();

}

/**
 * Add a process to our process list
 */
void add_process(process *p) {
    /** YOUR CODE HERE */
    process *last = first_process;
    while (last->next != NULL) {
        last = last->next;
    }
    last->next = p;
    p->prev = last;
}

/**
 * Creates a process given the inputString from stdin
 */

process *create_process(tok_t *inputString) {
    /** YOUR CODE HERE */
    process *p = malloc(sizeof(process));
    p->argv = inputString;
    p->argc = calcArgC(p->argv);
    p->completed = FALSE;
    p->stopped = FALSE;
    p->status = 0;

    p->stdin = STDIN_FILENO;
    p->stdout = STDOUT_FILENO;
    p->stderr = STDERR_FILENO;

    // Look for < or > in arguments
    int i;
    for (i = 0; i < p->argc - 1; i++) {
        if (strncmp(inputString[i], "<", 1) == 0) {
            p->stdin = open(inputString[i + 1], O_RDONLY);
            p->argv[i] = NULL;
            p->argv[i + 1] = NULL;
        } else if (strncmp(inputString[i], ">", 1) == 0) {
            p->stdout = open(inputString[i + 1], O_WRONLY | O_CREAT, 0644);
            p->argv[i] = NULL;
            p->argv[i + 1] = NULL;
        }
    }

    p->argc = calcArgC(p->argv);

    if (strcmp(p->argv[p->argc - 1], "&") == 0) {
        p->background = TRUE;
        p->argv[p->argc - 1] = NULL;
        p->argc--;
    } else {
        p->background = FALSE;
    }

    return p;
}

int calcArgC(char **argv) {
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    return argc;
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
        else if (calcArgC(t) > 0) {
            run_file(t);
        }
        // fprintf(stdout, "%d: ", lineNum);
    }

    return 0;
}


void run_file(tok_t argv[]) {
    process *p = create_process(argv);
    add_process(p);

    int pid;
    pid = fork();
    if (pid < 0) {
        // error
        perror("fork");
    } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        p->pid = getpid();

        printf("child id: %d", p->pid);


        launch_process(p);

    } else {
        // parent
//        p->pid = getpid();
        p->pid = pid;
        int parentPID = getpid();

        setpgid(parentPID, parentPID);
        if (!p->background) {
            tcsetpgrp(STDIN_FILENO, parentPID);
            int *status;
            waitpid(pid, status, 2);

            process *child = find_process(pid);
            if (child != NULL) {
                child->completed = TRUE;
            }
        }

    }
}

process *find_process(int pid) {
    process *p = first_process;
    while (p != NULL) {
        if (p->pid == pid) {
            return p;
        }
        p = p->next;
    }

    return NULL;
}
