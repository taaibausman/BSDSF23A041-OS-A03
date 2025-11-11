#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

/* Readline headers */
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 1024
#define MAXARGS 100
#define ARGLEN 256
#define PROMPT "myshell> "
#define HISTORY_SIZE 200
#define MAX_VARS 200

/* Input */
char* read_cmd(char* prompt);

/* Parsing & execution */
char** tokenize(char* cmdline);
int execute(char* arglist[]);
int handle_builtin(char** arglist);

/* If-else handling */
int handle_if_else(char* cmdline);

/* Internal history */
void internal_history_add(const char* cmdline);
void internal_history_show();
char* internal_history_get(int index);

/* Variables (session) */
void set_var(const char* name, const char* value);
const char* get_var(const char* name);
void expand_token_inplace(char* dest, const char* src);

#endif
