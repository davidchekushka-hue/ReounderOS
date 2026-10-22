#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "shell.h"

#define PROMPT "\033[1;32mroot@reounderos\033[0m\033[1;35m#>\033[0m "
#define MAX_ARGS 64

static int split_args(char *line, char **argv, int max){
    int argc=0; char *p=line;
    while(*p && argc<max-1){
        while(*p==' '||*p=='\t'||*p=='\n') p++;
        if(!*p) break;
        if(*p=='"'||*p=='\''){
            char q=*p++; argv[argc++]=p;
            while(*p && *p!=q) p++;
            if(*p) *p++=0;
        }else{
            argv[argc++]=p;
            while(*p && *p!=' ' && *p!='\t' && *p!='\n') p++;
            if(*p) *p++=0;
        }
    }
    argv[argc]=NULL; return argc;
}

int main(void){
    char line[4096]; char *argv[MAX_ARGS];
    signal(SIGINT, SIG_IGN);
    if(isatty(0)){
        printf("\033[35mReounderOS CmdBox\033[0m — type 'help' for commands\n\n");
    }
    for(;;){
        write(1, PROMPT, sizeof(PROMPT)-1);
        if(!fgets(line, sizeof(line), stdin)){ putchar('\n'); break; }
        int argc = split_args(line, argv, MAX_ARGS);
        if(argc) run_command(argc, argv);
    }
    return 0;
}
