#define _GNU_SOURCE
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/klog.h>

extern char **environ;

int cmd_uname(int argc, char **argv){
    struct utsname u;
    if(uname(&u)<0){ perror("uname"); return 1; }
    if(argc>1 && !strcmp(argv[1],"-a"))
        printf("%s %s %s %s %s\n",u.sysname,u.nodename,u.release,u.version,u.machine);
    else if(argc>1 && !strcmp(argv[1],"-m")) puts(u.machine);
    else if(argc>1 && !strcmp(argv[1],"-r")) puts(u.release);
    else puts(u.sysname);
    return 0;
}
int cmd_whoami(int argc, char **argv){
    (void)argc;(void)argv;
    struct passwd *pw = getpwuid(getuid());
    if(pw) puts(pw->pw_name); else printf("%d\n",(int)getuid());
    return 0;
}
int cmd_hostname(int argc, char **argv){
    (void)argc;(void)argv;
    char b[256];
    if(gethostname(b,sizeof(b))<0){ perror("hostname"); return 1; }
    puts(b); return 0;
}
int cmd_id(int argc, char **argv){
    (void)argc;(void)argv;
    printf("uid=%d gid=%d euid=%d egid=%d\n",
           (int)getuid(),(int)getgid(),(int)geteuid(),(int)getegid());
    return 0;
}
int cmd_date(int argc, char **argv){
    (void)argc;(void)argv;
    time_t t=time(NULL); char b[64];
    strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S %Z", localtime(&t));
    puts(b); return 0;
}
int cmd_uptime(int argc, char **argv){
    (void)argc;(void)argv;
    FILE *f=fopen("/proc/uptime","r");
    if(!f){ perror("uptime"); return 1; }
    double up=0; if(fscanf(f,"%lf",&up)!=1) up=0; fclose(f);
    long s=(long)up;
    printf("up %ldh %ldm %lds\n", s/3600,(s/60)%60,s%60);
    return 0;
}
int cmd_free(int argc, char **argv){
    (void)argc;(void)argv;
    FILE *f=fopen("/proc/meminfo","r");
    if(!f){ perror("free"); return 1; }
    long total=0, avail=0; char l[256];
    while(fgets(l,sizeof(l),f)){
        sscanf(l,"MemTotal: %ld kB",&total);
        sscanf(l,"MemAvailable: %ld kB",&avail);
    }
    fclose(f);
    printf("              total        used        free\n");
    printf("Mem:  %10ld  %10ld  %10ld\n", total/1024, (total-avail)/1024, avail/1024);
    return 0;
}
int cmd_df(int argc, char **argv){
    (void)argc;(void)argv;
    struct statvfs v;
    if(statvfs("/",&v)<0){ perror("df"); return 1; }
    unsigned long total = v.f_blocks*v.f_frsize/1024/1024;
    unsigned long freeb = v.f_bfree *v.f_frsize/1024/1024;
    unsigned long avail = v.f_bavail*v.f_frsize/1024/1024;
    printf("Filesystem      Size      Used     Avail\n");
    printf("/              %5luM    %5luM    %5luM\n", total, total-freeb, avail);
    return 0;
}
/* ps: cmdline (если есть) с фолбэком на comm */
int cmd_ps(int argc, char **argv){
    (void)argc;(void)argv;
    DIR *d=opendir("/proc"); if(!d){ perror("ps"); return 1; }
    printf("  PID  CMD\n");
    struct dirent *e;
    while((e=readdir(d))){
        if(e->d_name[0]<'0'||e->d_name[0]>'9') continue;
        char path[256], name[256]="?";
        snprintf(path,sizeof(path),"/proc/%s/cmdline",e->d_name);
        FILE *f=fopen(path,"r");
        if(f){
            size_t n=fread(name,1,sizeof(name)-1,f);
            fclose(f);
            if(n>0){
                name[n]=0;
                for(size_t i=0;i<n;i++) if(name[i]=='\0') name[i]=' ';
                /* trim trailing space */
                while(n && name[n-1]==' ') name[--n]=0;
            } else name[0]=0;
        }
        if(!name[0]){
            snprintf(path,sizeof(path),"/proc/%s/comm",e->d_name);
            f=fopen(path,"r");
            if(f){ if(fgets(name,sizeof(name),f)){ char *p=strchr(name,'\n'); if(p)*p=0; } fclose(f); }
            else strcpy(name,"?");
        }
        printf("%5s  %s\n", e->d_name, name);
    }
    closedir(d); return 0;
}
/* dmesg: klogctl READ_ALL — вернёт весь буфер даже при повторном вызове */
int cmd_dmesg(int argc, char **argv){
    (void)argc;(void)argv;
    int len = klogctl(10 /*SYSLOG_ACTION_SIZE_BUFFER*/, NULL, 0);
    if(len <= 0){ perror("dmesg"); return 1; }
    char *buf = malloc((size_t)len + 1);
    if(!buf){ perror("dmesg"); return 1; }
    int n = klogctl(3 /*SYSLOG_ACTION_READ_ALL*/, buf, len);
    if(n < 0){ perror("dmesg"); free(buf); return 1; }
    buf[n] = 0;
    fputs(buf, stdout);
    free(buf); return 0;
}
int cmd_env(int argc, char **argv){
    (void)argc;(void)argv;
    for(char **e=environ;*e;e++) puts(*e);
    return 0;
}
int cmd_export(int argc, char **argv){
    if(argc<2) return cmd_env(0,NULL);
    for(int i=1;i<argc;i++){
        /* putenv требует владения строкой — дублируем в кучу */
        char *dup = strdup(argv[i]);
        if(!dup){ perror("export"); return 1; }
        putenv(dup);
    }
    return 0;
}
int cmd_unset(int argc, char **argv){
    for(int i=1;i<argc;i++) unsetenv(argv[i]);
    return 0;
}
int cmd_which(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: which CMD...\n"); return 1; }
    const char *path=getenv("PATH"); if(!path) path="/bin:/sbin:/usr/bin:/usr/sbin";
    int rc=1;
    for(int i=1;i<argc;i++){
        char b[4096]; const char *p=path;
        while(*p){
            const char *c=strchr(p,':');
            size_t ln = c ? (size_t)(c-p) : strlen(p);
            snprintf(b,sizeof(b),"%.*s/%s",(int)ln,p,argv[i]);
            if(access(b,X_OK)==0){ puts(b); rc=0; break; }
            if(!c) break; p=c+1;
        }
    }
    return rc;
}
int cmd_mount(int argc, char **argv){
    (void)argc;(void)argv;
    FILE *f=fopen("/proc/mounts","r");
    if(!f){ perror("mount"); return 1; }
    char l[512];
    while(fgets(l,sizeof(l),f)) fputs(l,stdout);
    fclose(f); return 0;
}
int cmd_umount(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: umount MOUNT\n"); return 1; }
    if(umount(argv[1])<0){ perror("umount"); return 1; }
    return 0;
}
