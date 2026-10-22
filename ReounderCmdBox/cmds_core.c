#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>

int cmd_echo(int argc, char **argv){
    for(int i=1;i<argc;i++){ if(i>1) putchar(' '); fputs(argv[i], stdout); }
    putchar('\n'); return 0;
}
int cmd_pwd(int argc, char **argv){
    (void)argc;(void)argv; char b[4096];
    if(getcwd(b,sizeof(b))) puts(b); else perror("pwd");
    return 0;
}
int cmd_cd(int argc, char **argv){
    const char *p = argc>1 ? argv[1] : "/";
    if(chdir(p)<0){ perror("cd"); return 1; } return 0;
}
int cmd_true(int argc, char **argv){ (void)argc;(void)argv; return 0; }
int cmd_false(int argc, char **argv){ (void)argc;(void)argv; return 1; }
int cmd_clear(int argc, char **argv){
    (void)argc;(void)argv; fputs("\033[2J\033[H", stdout); fflush(stdout); return 0;
}
int cmd_exit(int argc, char **argv){
    int c = argc>1 ? atoi(argv[1]) : 0;
    if(getpid()==1){ sync(); reboot(RB_POWER_OFF); }
    exit(c);
}
int cmd_help(int argc, char **argv){
    (void)argc;(void)argv;
    printf("ReounderOS CmdBox — %d commands\n\n", cmd_count);
    for(int i=0;i<cmd_count;i++)
        printf("  \033[36m%-10s\033[0m %s\n", cmd_table[i].name, cmd_table[i].desc);
    return 0;
}
