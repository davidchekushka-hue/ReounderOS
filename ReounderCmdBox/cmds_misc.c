#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/wait.h>

int cmd_sleep(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: sleep SECONDS\n"); return 1; }
    sleep(atoi(argv[1])); return 0;
}
int cmd_yes(int argc, char **argv){
    const char *s = argc>1 ? argv[1] : "y";
    int limit = argc>2 ? atoi(argv[2]) : 1000;
    while(limit-- > 0) puts(s);
    return 0;
}
int cmd_seq(int argc, char **argv){
    long from=1,to=1;
    if(argc==2) to=atol(argv[1]);
    else if(argc>=3){ from=atol(argv[1]); to=atol(argv[2]); }
    else { fprintf(stderr,"usage: seq [FROM] TO\n"); return 1; }
    for(long i=from;i<=to;i++) printf("%ld\n", i);
    return 0;
}
int cmd_chmod(int argc, char **argv){
    if(argc<3){ fprintf(stderr,"usage: chmod MODE FILE...\n"); return 1; }
    mode_t m = (mode_t)strtol(argv[1], NULL, 8);
    int rc=0;
    for(int i=2;i<argc;i++)
        if(chmod(argv[i], m)<0){ perror(argv[i]); rc=1; }
    return rc;
}
int cmd_sync(int argc, char **argv){ (void)argc;(void)argv; sync(); return 0; }
int cmd_reboot(int argc, char **argv){
    (void)argc;(void)argv; sync(); reboot(RB_AUTOBOOT);
    perror("reboot"); return 1;
}
int cmd_poweroff(int argc, char **argv){
    (void)argc;(void)argv; sync(); reboot(RB_POWER_OFF);
    perror("poweroff"); return 1;
}
int cmd_kill(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: kill PID\n"); return 1; }
    if(kill(atoi(argv[1]), SIGTERM)<0){ perror("kill"); return 1; }
    return 0;
}

int cmd_reoinstall(int argc, char **argv){
    (void)argc;(void)argv;
    pid_t p = fork();
    if(p < 0){ perror("reoinstall"); return 1; }
    if(p == 0){
        execl("/bin/reoinstall","reoinstall",(char*)0);
        _exit(127);
    }
    int st = 0;
    waitpid(p,&st,0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}
