#define _GNU_SOURCE
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cmd_head(int argc, char **argv){
    int n=10, fi=1;
    if(argc>2 && !strcmp(argv[1],"-n")){ n=atoi(argv[2]); fi=3; }
    FILE *f = fi<argc ? fopen(argv[fi],"r") : stdin;
    if(!f){ perror(argv[fi]); return 1; }
    char l[4096];
    while(n-->0 && fgets(l,sizeof(l),f)) fputs(l,stdout);
    if(f!=stdin) fclose(f); return 0;
}
int cmd_tail(int argc, char **argv){
    int n=10, fi=1;
    if(argc>2 && !strcmp(argv[1],"-n")){ n=atoi(argv[2]); fi=3; }
    if(n<=0) return 0;
    FILE *f = fi<argc ? fopen(argv[fi],"r") : stdin;
    if(!f){ perror(argv[fi]); return 1; }
    char **buf = calloc(n,sizeof(char*));
    int idx=0,total=0; char l[4096];
    while(fgets(l,sizeof(l),f)){
        free(buf[idx]); buf[idx]=strdup(l);
        idx=(idx+1)%n; total++;
    }
    int start = total<n ? 0 : idx;
    int cnt   = total<n ? total : n;
    for(int i=0;i<cnt;i++) fputs(buf[(start+i)%n], stdout);
    for(int i=0;i<n;i++) free(buf[i]);
    free(buf); if(f!=stdin) fclose(f); return 0;
}
int cmd_wc(int argc, char **argv){
    FILE *f = argc>1 ? fopen(argv[1],"r") : stdin;
    if(!f){ perror(argv[1]); return 1; }
    long ln=0,wd=0,ch=0; int inw=0,c;
    while((c=fgetc(f))!=EOF){
        ch++;
        if(c=='\n') ln++;
        if(c==' '||c=='\t'||c=='\n') inw=0;
        else if(!inw){ inw=1; wd++; }
    }
    printf("%ld %ld %ld %s\n", ln,wd,ch, argc>1?argv[1]:"-");
    if(f!=stdin) fclose(f); return 0;
}
int cmd_grep(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: grep PATTERN [FILE...]\n"); return 1; }
    const char *pat=argv[1];
    if(argc==2){
        char l[4096];
        while(fgets(l,sizeof(l),stdin)) if(strstr(l,pat)) fputs(l,stdout);
        return 0;
    }
    int rc=1;
    for(int i=2;i<argc;i++){
        FILE *f=fopen(argv[i],"r"); if(!f){ perror(argv[i]); continue; }
        char l[4096];
        while(fgets(l,sizeof(l),f))
            if(strstr(l,pat)){ fputs(l,stdout); rc=0; }
        fclose(f);
    }
    return rc;
}
static int cmps(const void *a,const void *b){
    return strcmp(*(const char**)a, *(const char**)b);
}
int cmd_sort(int argc, char **argv){
    FILE *f = argc>1 ? fopen(argv[1],"r") : stdin;
    if(!f){ perror(argv[1]); return 1; }
    char **ls=NULL; size_t cap=0,len=0; char l[4096];
    while(fgets(l,sizeof(l),f)){
        if(len==cap){ cap=cap?cap*2:64; ls=realloc(ls,cap*sizeof(char*)); }
        ls[len++]=strdup(l);
    }
    if(f!=stdin) fclose(f);
    qsort(ls,len,sizeof(char*),cmps);
    for(size_t i=0;i<len;i++){ fputs(ls[i],stdout); free(ls[i]); }
    free(ls); return 0;
}
int cmd_uniq(int argc, char **argv){
    FILE *f = argc>1 ? fopen(argv[1],"r") : stdin;
    if(!f){ perror(argv[1]); return 1; }
    char prev[4096]="", l[4096]; int first=1;
    while(fgets(l,sizeof(l),f)){
        if(first || strcmp(l,prev)){ fputs(l,stdout); strcpy(prev,l); first=0; }
    }
    if(f!=stdin) fclose(f); return 0;
}
int cmd_tee(int argc, char **argv){
    int n=argc-1; FILE **fs=calloc(n?n:1,sizeof(FILE*));
    for(int i=0;i<n;i++){
        fs[i]=fopen(argv[i+1],"w");
        if(!fs[i]) perror(argv[i+1]);
    }
    char b[4096]; ssize_t r;
    while((r=read(0,b,sizeof(b)))>0){
        write(1,b,r);
        for(int i=0;i<n;i++) if(fs[i]) fwrite(b,1,r,fs[i]);
    }
    for(int i=0;i<n;i++) if(fs[i]) fclose(fs[i]);
    free(fs); return 0;
}
