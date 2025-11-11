#include "shell.h"

/* Helper to trim whitespace */
static void trim_whitespace(char* s) {
    if (!s) return;
    /* left trim */
    char* p = s;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    /* right trim */
    int len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n')) {
        s[len-1] = '\0';
        len--;
    }
}

/* ----------------------------
   handle_if_else - parse and run a single-line if ... then ... [else ...] fi
   Returns 1 if handled; 0 if not an if-statement
   Limitations: condition is executed as a single command (no complex pipeline support inside condition).
   ---------------------------- */
int handle_if_else(char* cmdline) {
    if (!cmdline) return 0;
    char* p = cmdline;
    /* must start with "if " */
    while (*p == ' ') p++;
    if (strncmp(p, "if ", 3) != 0) return 0;

    char* then_ptr = strstr(p, " then ");
    char* fi_ptr = strstr(p, " fi");
    if (!then_ptr || !fi_ptr) return 0;

    char* else_ptr = strstr(p, " else ");
    char condbuf[MAX_LEN] = {0};
    char thenbuf[MAX_LEN] = {0};
    char elsebuf[MAX_LEN] = {0};

    /* condition between "if " and " then " */
    int cond_len = then_ptr - (p + 3);
    if (cond_len >= (int)sizeof(condbuf)) cond_len = (int)sizeof(condbuf) - 1;
    strncpy(condbuf, p + 3, cond_len);
    condbuf[cond_len] = '\0';
    trim_whitespace(condbuf);

    if (else_ptr && else_ptr < fi_ptr) {
        int then_len = else_ptr - (then_ptr + 6);
        if (then_len >= (int)sizeof(thenbuf)) then_len = (int)sizeof(thenbuf) - 1;
        strncpy(thenbuf, then_ptr + 6, then_len);
        thenbuf[then_len] = '\0';

        int else_len = fi_ptr - (else_ptr + 6);
        if (else_len >= (int)sizeof(elsebuf)) else_len = (int)sizeof(elsebuf) - 1;
        strncpy(elsebuf, else_ptr + 6, else_len);
        elsebuf[else_len] = '\0';
    } else {
        int then_len = fi_ptr - (then_ptr + 6);
        if (then_len >= (int)sizeof(thenbuf)) then_len = (int)sizeof(thenbuf) - 1;
        strncpy(thenbuf, then_ptr + 6, then_len);
        thenbuf[then_len] = '\0';
    }

    trim_whitespace(thenbuf);
    trim_whitespace(elsebuf);

    /* Execute condition: tokenize and fork/exec */
    char** cond_args = tokenize(condbuf);
    if (!cond_args) return 0; /* nothing to run */

    pid_t cpid = fork();
    if (cpid < 0) {
        perror("fork failed");
        /* free cond_args */
        for (int i = 0; cond_args[i]; i++) free(cond_args[i]);
        free(cond_args);
        return 1;
    } else if (cpid == 0) {
        execvp(cond_args[0], cond_args);
        perror("Condition exec failed");
        exit(127);
    } else {
        int wstatus;
        waitpid(cpid, &wstatus, 0);
        int cond_ok = WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;

        /* free cond args */
        for (int i = 0; cond_args[i]; i++) free(cond_args[i]);
        free(cond_args);

        if (cond_ok) {
            /* run then part using tokenization and execute() */
            char** then_args = tokenize(thenbuf);
            if (then_args) {
                /* handle assignment in then or builtin inside execute */
                if (!handle_builtin(then_args)) execute(then_args);
                for (int i = 0; then_args[i]; i++) free(then_args[i]);
                free(then_args);
            }
        } else {
            if (elsebuf[0] != '\0') {
                char** else_args = tokenize(elsebuf);
                if (else_args) {
                    if (!handle_builtin(else_args)) execute(else_args);
                    for (int i = 0; else_args[i]; i++) free(else_args[i]);
                    free(else_args);
                }
            }
        }
    }

    return 1;
}

/* ----------------------------
   execute: supports I/O redirection (<, >, >>), background (&), and pipes (|).
   arglist: token array (NULL-terminated) already with variable expansion.
   ---------------------------- */
int execute(char* arglist[]) {
    if (!arglist || !arglist[0]) return 0;

    /* first, detect and handle simple 'if' lines - but main should call handle_if_else before tokenizing; keep here as fallback */
    /* (we don't have full original command string here, so skip) */

    /* Check for background (&) token anywhere */
    int background = 0;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "&") == 0) {
            background = 1;
            arglist[i] = NULL;
            break;
        }
    }

    /* Handle pipes: count and split into commands */
    int pipe_count = 0;
    for (int i = 0; arglist[i] != NULL; i++) if (strcmp(arglist[i], "|") == 0) pipe_count++;

    if (pipe_count == 0) {
        /* Single command: handle redirection and exec */
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            return 1;
        } else if (pid == 0) {
            /* Child: handle redirection tokens */
            for (int i = 0; arglist[i] != NULL; i++) {
                if (strcmp(arglist[i], "<") == 0 && arglist[i+1]) {
                    int fd = open(arglist[i+1], O_RDONLY);
                    if (fd < 0) { perror("Input file"); exit(1); }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                    arglist[i] = NULL;
                } else if (strcmp(arglist[i], ">") == 0 && arglist[i+1]) {
                    int fd = open(arglist[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { perror("Output file"); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    arglist[i] = NULL;
                } else if (strcmp(arglist[i], ">>") == 0 && arglist[i+1]) {
                    int fd = open(arglist[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { perror("Append file"); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    arglist[i] = NULL;
                }
            }
            execvp(arglist[0], arglist);
            perror("Command not found");
            exit(127);
        } else {
            if (!background) {
                int status;
                waitpid(pid, &status, 0);
            } else {
                printf("[Running in background: PID %d]\n", pid);
            }
        }
        return 0;
    }

    /* ---------- Pipeline execution ----------
       Build an array of command token arrays pointing into arglist.
       We'll create small arrays of char* that point to arglist entries.
    */
    int num_cmds = pipe_count + 1;
    char** cmds[num_cmds];
    int cmd = 0;
    int i = 0;
    cmds[cmd] = &arglist[0];
    while (arglist[i] != NULL) {
        if (strcmp(arglist[i], "|") == 0) {
            arglist[i] = NULL; /* terminate previous command */
            cmd++;
            cmds[cmd] = &arglist[i+1];
        }
        i++;
    }

    int pipefds[2*(num_cmds-1)];
    for (int j = 0; j < num_cmds - 1; j++) {
        if (pipe(pipefds + j*2) < 0) {
            perror("pipe failed");
            return 1;
        }
    }

    for (int j = 0; j < num_cmds; j++) {
        pid_t p = fork();
        if (p < 0) {
            perror("fork failed");
            return 1;
        } else if (p == 0) {
            /* child */
            if (j > 0) {
                /* read end of previous pipe */
                dup2(pipefds[(j - 1) * 2], STDIN_FILENO);
            }
            if (j < num_cmds - 1) {
                /* write end of this pipe */
                dup2(pipefds[j*2 + 1], STDOUT_FILENO);
            }
            /* close all pipe fds */
            for (int k = 0; k < 2*(num_cmds - 1); k++) close(pipefds[k]);

            /* Handle redirections inside this command (if any) */
            char** cur = cmds[j];
            for (int x = 0; cur[x] != NULL; x++) {
                if (strcmp(cur[x], "<") == 0 && cur[x+1]) {
                    int fd = open(cur[x+1], O_RDONLY);
                    if (fd < 0) { perror("Input file"); exit(1); }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                    cur[x] = NULL;
                } else if (strcmp(cur[x], ">") == 0 && cur[x+1]) {
                    int fd = open(cur[x+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { perror("Output file"); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    cur[x] = NULL;
                } else if (strcmp(cur[x], ">>") == 0 && cur[x+1]) {
                    int fd = open(cur[x+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { perror("Append file"); exit(1); }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    cur[x] = NULL;
                }
            }

            execvp(cmds[j][0], cmds[j]);
            perror("exec failed");
            exit(127);
        }
        /* parent continues to next command fork */
    }

    /* parent: close pipes */
    for (int k = 0; k < 2*(num_cmds - 1); k++) close(pipefds[k]);

    if (!background) {
        for (int j = 0; j < num_cmds; j++) wait(NULL);
    } else {
        printf("[Pipeline running in background]\n");
    }

    return 0;
}
