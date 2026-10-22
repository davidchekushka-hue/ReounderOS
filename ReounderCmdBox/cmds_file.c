#define _GNU_SOURCE
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

static int cmps_name(const void *a,const void *b){
    return strcmp(*(const char**)a, *(const char**)b);
}
int cmd_ls(int argc, char **argv){
    int L=0, pi=1;
    if(argc>1 && !strcmp(argv[1],"-l")){ L=1; pi=2; }
    const char *path = pi<argc ? argv[pi] : ".";
    DIR *d = opendir(path);
    if(!d){ perror("ls"); return 1; }
    char **names = NULL; size_t cap = 0, len = 0;
    struct dirent *e;
    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue;
        if(len==cap){ cap = cap?cap*2:64; names = realloc(names, cap*sizeof(char*)); }
        names[len++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, len, sizeof(char*), cmps_name);
    for(size_t i=0;i<len;i++){
        if(!L){ puts(names[i]); free(names[i]); continue; }
        char full[4096]; snprintf(full,sizeof(full),"%s/%s",path,names[i]);
        struct stat st;
        if(lstat(full,&st)==0){
            char t = S_ISDIR(st.st_mode)?'d':S_ISLNK(st.st_mode)?'l':'-';
            printf("%c %10ld %s\n", t,(long)st.st_size, names[i]);
        } else puts(names[i]);
        free(names[i]);
    }
    free(names);
    return 0;
}
int cmd_cat(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: cat FILE...\n"); return 1; }
    int rc=0;
    for(int i=1;i<argc;i++){
        int fd=open(argv[i],O_RDONLY);
        if(fd<0){ perror(argv[i]); rc=1; continue; }
        char b[4096]; ssize_t n;
        while((n=read(fd,b,sizeof(b)))>0) if(write(1,b,n)!=n){ rc=1; break; }
        close(fd);
    }
    return rc;
}
int cmd_mkdir(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: mkdir DIR...\n"); return 1; }
    int rc=0; for(int i=1;i<argc;i++) if(mkdir(argv[i],0755)<0){ perror(argv[i]); rc=1; }
    return rc;
}
int cmd_rmdir(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: rmdir DIR...\n"); return 1; }
    int rc=0; for(int i=1;i<argc;i++) if(rmdir(argv[i])<0){ perror(argv[i]); rc=1; }
    return rc;
}
int cmd_rm(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: rm FILE...\n"); return 1; }
    int rc=0; for(int i=1;i<argc;i++) if(unlink(argv[i])<0){ perror(argv[i]); rc=1; }
    return rc;
}
int cmd_mv(int argc, char **argv){
    if(argc<3){ fprintf(stderr,"usage: mv SRC DST\n"); return 1; }
    if(rename(argv[1],argv[2])<0){ perror("mv"); return 1; } return 0;
}
int cmd_cp(int argc, char **argv){
    if(argc<3){ fprintf(stderr,"usage: cp SRC DST\n"); return 1; }
    int in=open(argv[1],O_RDONLY); if(in<0){ perror(argv[1]); return 1; }
    int out=open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(out<0){ perror(argv[2]); close(in); return 1; }
    char b[8192]; ssize_t n; int rc=0;
    while((n=read(in,b,sizeof(b)))>0) if(write(out,b,n)!=n){ rc=1; break; }
    close(in); close(out); return rc;
}
int cmd_touch(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: touch FILE...\n"); return 1; }
    int rc=0;
    for(int i=1;i<argc;i++){
        int fd=open(argv[i],O_WRONLY|O_CREAT,0644);
        if(fd<0){ perror(argv[i]); rc=1; continue; }
        close(fd);
    }
    return rc;
}
int cmd_stat(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: stat FILE\n"); return 1; }
    struct stat st;
    if(lstat(argv[1],&st)<0){ perror("stat"); return 1; }
    char tb[64];
    strftime(tb,sizeof(tb),"%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
    printf("  File: %s\n  Size: %ld\n  Mode: %04o\n  Type: %s\n  Mtime: %s\n",
           argv[1],(long)st.st_size, st.st_mode & 07777,
           S_ISDIR(st.st_mode)?"directory":S_ISLNK(st.st_mode)?"symlink":"file",
           tb);
    return 0;
}
int cmd_readlink(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: readlink LINK\n"); return 1; }
    char b[4096]; ssize_t n=readlink(argv[1],b,sizeof(b)-1);
    if(n<0){ perror("readlink"); return 1; }
    b[n]=0; puts(b); return 0;
}
int cmd_basename(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: basename PATH\n"); return 1; }
    char *p=strrchr(argv[1],'/'); puts(p?p+1:argv[1]); return 0;
}
int cmd_dirname(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: dirname PATH\n"); return 1; }
    char b[4096]; strncpy(b,argv[1],sizeof(b)-1); b[sizeof(b)-1]=0;
    char *p=strrchr(b,'/');
    if(!p){ puts("."); return 0; }
    if(p==b){ puts("/"); return 0; }
    *p=0; puts(b); return 0;
}
static void walk(const char *path, int depth){
    if(depth>20) return;
    DIR *d=opendir(path); if(!d) return;
    struct dirent *e;
    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue;
        char full[4096]; snprintf(full,sizeof(full),"%s/%s",path,e->d_name);
        puts(full);
        struct stat st;
        if(lstat(full,&st)==0 && S_ISDIR(st.st_mode)) walk(full, depth+1);
    }
    closedir(d);
}
int cmd_find(int argc, char **argv){
    walk(argc>1 ? argv[1] : ".", 0); return 0;
}
