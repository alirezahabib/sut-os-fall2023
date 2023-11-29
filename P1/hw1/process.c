#include "process.h"
#include "shell.h"
#include "parse.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <termios.h>
#include <string.h>
#include <limits.h>

/**
 * Executes the process p.
 * If the shell is in interactive mode and the process is a foreground process,
 * then p should take control of the terminal.
 */
void launch_process(process *p) {
    dup2(p->stdin, STDIN_FILENO);
    dup2(p->stdout, STDOUT_FILENO);
    char *file = p->argv[0];

    char *path = getenv("PATH");

    // Check if file is in current directory
    if (access(file, F_OK) == 0) execv(p->argv[0], p->argv);
    else {
        // Search PATH
        char *dir = strtok(strdup(path), ":");
        char full_path[PATH_MAX];

        while (dir) {
            sprintf(full_path, "%s/%s", dir, file);

            if (access(full_path, F_OK) == 0) {
                // Can access file
                execv(full_path, p->argv);
                perror("execv");
            }
            dir = strtok(NULL, ":");
        }

        perror("PATH");
        exit(0);
    }
}

/* Put a process in the foreground. This function assumes that the shell
 * is in interactive mode. If the cont argument is true, send the process
 * group a SIGCONT signal to wake it up.
 */
void
put_process_in_foreground(process *p, int cont) {
    /** YOUR CODE HERE */
}

/* Put a process in the background. If the cont argument is true, send
 * the process group a SIGCONT signal to wake it up. */
void
put_process_in_background(process *p, int cont) {
    /** YOUR CODE HERE */
}
