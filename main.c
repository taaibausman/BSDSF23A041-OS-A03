#include "shell.h"
#include <ctype.h>

/* Signal handlers */
void sigint_handler(int signo) {
    /* print prompt on new line, keep shell alive */
    write(STDOUT_FILENO, "\nmyshell> ", 10);
    fflush(stdout);
}

void sigchld_handler(int signo) {
    /* reap finished background processes */
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

/* helper: check if token is assignment NAME=VALUE (no spaces) */
static int is_assignment_token(const char* tok) {
    if (!tok) return 0;
    const char* eq = strchr(tok, '=');
    if (!eq) return 0;
    /* ensure name part is non-empty and valid identifier start */
    if (eq == tok) return 0;
    /* name should not contain spaces; already ensured by tokenization */
    return 1;
}

int main() {
    /* install signal handlers */
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);

    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT)) != NULL) {
        if (cmdline[0] == '\0') { free(cmdline); continue; }

        /* history recall !n */
        if (cmdline[0] == '!' && strlen(cmdline) > 1) {
            int idx = atoi(cmdline + 1);
            char* recalled = internal_history_get(idx);
            if (recalled) {
                printf("%s\n", recalled);
                free(cmdline);
                cmdline = strdup(recalled);
            } else {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }
        }

        /* If the line is an if/then/else/fi structure, handle it before tokenizing */
        if (handle_if_else(cmdline)) {
            free(cmdline);
            continue;
        }

        /* Tokenize (this performs variable expansion) */
        arglist = tokenize(cmdline);
        if (arglist == NULL) { free(cmdline); continue; }

        /* Handle simple variable assignment: NAME=VALUE */
        if (is_assignment_token(arglist[0]) && arglist[1] == NULL) {
            char* eq = strchr(arglist[0], '=');
            size_t name_len = eq - arglist[0];
            char name[ARGLEN]; char value[ARGLEN];
            strncpy(name, arglist[0], name_len);
            name[name_len] = '\0';
            strncpy(value, eq + 1, ARGLEN - 1);
            value[ARGLEN - 1] = '\0';
            set_var(name, value);

            /* cleanup */
            for (int i = 0; arglist[i]; i++) free(arglist[i]);
            free(arglist);
            free(cmdline);
            continue;
        }

        /* Built-ins or external execution */
        if (!handle_builtin(arglist)) {
            execute(arglist);
        }

        /* cleanup */
        for (int i = 0; arglist[i]; i++) free(arglist[i]);
        free(arglist);
        free(cmdline);
    }

    printf("\nmyshell exited.\n");
    return 0;
}
