#ifndef SHELL_H
#define SHELL_H
#include <stddef.h>

typedef int (*cmd_fn)(int argc, char **argv);
struct cmd { const char *name; const char *desc; cmd_fn fn; };

extern const struct cmd cmd_table[];
extern const int cmd_count;
int run_command(int argc, char **argv);

/* общие команды, используемые из других TU */
int cmd_env(int argc, char **argv);
int cmd_help(int argc, char **argv);
#endif
