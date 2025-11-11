#include "shell.h"

/* ----------------------------
   Internal history
   ---------------------------- */
char internal_history[HISTORY_SIZE][MAX_LEN];
int internal_history_count = 0;

/* ----------------------------
   Internal variables
   ---------------------------- */
static char var_names[MAX_VARS][ARGLEN];
static char var_values[MAX_VARS][ARGLEN];
static int var_count = 0;

void set_var(const char* name, const char* value) {
    if (!name) return;
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            strncpy(var_values[i], value ? value : "", ARGLEN - 1);
            return;
        }
    }
    if (var_count < MAX_VARS) {
        strncpy(var_names[var_count], name, ARGLEN - 1);
        strncpy(var_values[var_count], value ? value : "", ARGLEN - 1);
        var_count++;
    }
}

const char* get_var(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0)
            return var_values[i];
    }
    return getenv(name); /* fallback to environment */
}

/* Expand single token: replace occurrences of $NAME with value.
   dest must have size ARGLEN or larger. */
void expand_token_inplace(char* dest, const char* src) {
    if (!src || !dest) return;
    const char* p = src;
    char buf[ARGLEN];
    int bi = 0;
    while (*p && bi < ARGLEN - 1) {
        if (*p == '$') {
            p++;
            if (*p == '\0') { /* trailing $ */
                buf[bi++] = '$';
                break;
            }
            /* collect variable name */
            char name[ARGLEN];
            int ni = 0;
            if (*p == '{') { /* support ${NAME} */
                p++;
                while (*p && *p != '}' && ni < ARGLEN - 1) name[ni++] = *p++;
                if (*p == '}') p++;
            } else {
                while ((*p == '_' || isalnum((unsigned char)*p)) && ni < ARGLEN - 1) name[ni++] = *p++;
            }
            name[ni] = '\0';
            const char* val = get_var(name);
            if (val) {
                for (int k = 0; val[k] && bi < ARGLEN - 1; k++)
                    buf[bi++] = val[k];
            }
        } else {
            buf[bi++] = *p++;
        }
    }
    buf[bi] = '\0';
    strncpy(dest, buf, ARGLEN - 1);
    dest[ARGLEN - 1] = '\0';
}

/* ----------------------------
   read_cmd - wrapper for readline()
   ---------------------------- */
char* read_cmd(char* prompt) {
    char* line = readline(prompt);
    if (line == NULL) return NULL;
    if (strlen(line) > 0) {
        add_history(line); /* readline history */
        internal_history_add(line); /* internal history */
    }
    return line;
}

/* ----------------------------
   Tokenize with variable expansion
   Caller must free returned array and each string.
   ---------------------------- */
char** tokenize(char* cmdline) {
    if (cmdline == NULL) return NULL;

    /* Make a working copy because strtok modifies string */
    char tmp[MAX_LEN];
    strncpy(tmp, cmdline, MAX_LEN - 1);
    tmp[MAX_LEN - 1] = '\0';

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int idx = 0;

    char* p = tmp;
    char* token;

    /* simple tokenization on spaces and tabs; preserves quoting is not implemented */
    token = strtok(p, " \t");
    while (token != NULL && idx < MAXARGS) {
        /* check for variable assignment like NAME=VALUE (no spaces) */
        char* eq = strchr(token, '=');
        if (eq != NULL && token[0] != '=' ) {
            /* treat as assignment token; we still return it so caller can catch assignment
               but many shells allow assignment as separate command; here we let main handle it */
        }
        /* expand variables into buffer */
        char expanded[ARGLEN];
        expand_token_inplace(expanded, token);

        arglist[idx] = malloc(strlen(expanded) + 1);
        strcpy(arglist[idx], expanded);

        idx++;
        token = strtok(NULL, " \t");
    }

    arglist[idx] = NULL;
    if (idx == 0) {
        free(arglist);
        return NULL;
    }
    return arglist;
}

/* ----------------------------
   Built-in command handler
   Return 1 if handled, 0 otherwise
   ---------------------------- */
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting myshell...\n");
        exit(0);
    } else if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) fprintf(stderr, "cd: expected argument\n");
        else if (chdir(arglist[1]) != 0) perror("cd failed");
        return 1;
    } else if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   - Change directory\n");
        printf("  exit       - Exit the shell\n");
        printf("  help       - Show this help message\n");
        printf("  history    - Show command history\n");
        printf("  VAR=val    - Set variable\n");
        printf("  echo $VAR  - Expand variable\n");
        return 1;
    } else if (strcmp(arglist[0], "history") == 0) {
        internal_history_show();
        return 1;
    }
    return 0;
}

/* ----------------------------
   Internal history functions
   ---------------------------- */
void internal_history_add(const char* cmdline) {
    if (!cmdline || !*cmdline) return;
    if (internal_history_count < HISTORY_SIZE) {
        strncpy(internal_history[internal_history_count++], cmdline, MAX_LEN - 1);
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++)
            strncpy(internal_history[i - 1], internal_history[i], MAX_LEN);
        strncpy(internal_history[HISTORY_SIZE - 1], cmdline, MAX_LEN - 1);
    }
}

void internal_history_show() {
    for (int i = 0; i < internal_history_count; i++)
        printf("%d %s\n", i + 1, internal_history[i]);
}

char* internal_history_get(int index) {
    if (index < 1 || index > internal_history_count) return NULL;
    return internal_history[index - 1];
}
