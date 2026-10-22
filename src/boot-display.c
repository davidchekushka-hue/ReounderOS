#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/utsname.h>

#define KMSG_PATH "/dev/kmsg"
#define LINE_BUF 4096
#define DEFAULT_DELAY_S 5

static volatile sig_atomic_t g_stop=0;
static void on_sig(int s){(void)s; g_stop=1;}
static long long now_ms(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (long long)ts.tv_sec*1000LL+ts.tv_nsec/1000000LL;}
static const char* kmsg_payload(const char*l){const char*s=strchr(l,';');return s?s+1:l;}

static void drain_kmsg(int fd,long long dl){
    static char buf[LINE_BUF]; static size_t bl=0;
    while(!g_stop){
        long long n=now_ms(); if(n>=dl)break;
        struct pollfd p={.fd=fd,.events=POLLIN};
        int r=poll(&p,1,(int)(dl-n));
        if(r<0){if(errno==EINTR)continue;break;}
        if(r==0)break;
        ssize_t k=read(fd,buf+bl,sizeof(buf)-bl-1);
        if(k<0){if(errno==EAGAIN||errno==EWOULDBLOCK){usleep(5000);continue;}if(errno==EINTR)continue;break;}
        if(k==0){usleep(5000);continue;}
        bl+=(size_t)k;
        size_t st=0;
        for(size_t i=0;i<bl;i++) if(buf[i]=='\n'){
            buf[i]='\0'; fputs(kmsg_payload(buf+st),stdout); fputc('\n',stdout);
            fflush(stdout); st=i+1;
        }
        if(st){memmove(buf,buf+st,bl-st);bl-=st;}
    }
}
static void reofetch(void){
    struct utsname u; char os[128]="ReounderOS", kern[160]="?";
    FILE*f=fopen("/etc/os-release","r");
    if(f){char l[256];while(fgets(l,sizeof(l),f)){if(!strncmp(l,"PRETTY_NAME=",12)){char*v=l+12;size_t n=strlen(v);while(n&&(v[n-1]=='\n'||v[n-1]=='"'))v[--n]=0;if(*v=='"')v++;strncpy(os,v,sizeof(os)-1);break;}}fclose(f);}
    if(uname(&u)==0) snprintf(kern,sizeof(kern),"%s %s",u.release,u.machine);
    double up=0; f=fopen("/proc/uptime","r"); if(f){if(fscanf(f,"%lf",&up)!=1)up=0;fclose(f);} long s=(long)up;
    long mt=0,ma=0; f=fopen("/proc/meminfo","r");
    if(f){char l[128];while(fgets(l,sizeof(l),f)){sscanf(l,"MemTotal: %ld kB",&mt);sscanf(l,"MemAvailable: %ld kB",&ma);}fclose(f);}
    printf("\033[36m  ┌── reofetch ────────────────────────────┐\033[0m\n");
    printf("   OS     : %s\n", os);
    printf("   Kernel : %s\n", kern);
    printf("   Uptime : %ldh %ldm %lds\n", s/3600,(s/60)%60,s%60);
    printf("   RAM    : %ld / %ld MiB\n", ma/1024,mt/1024);
    printf("\033[36m  └────────────────────────────────────────┘\033[0m\n");
    fflush(stdout);
}
int main(int argc,char**argv){
    long d=DEFAULT_DELAY_S;
    if(argc>1){char*e=NULL;long v=strtol(argv[1],&e,10);if(e&&!*e&&v>=0)d=v;}
    int t=open("/dev/tty0",O_RDWR|O_NOCTTY);
    if(t<0) t=open("/dev/console",O_RDWR|O_NOCTTY);
    if(t<0) t=open("/dev/tty",O_RDWR|O_NOCTTY);
    if(t>=0){
        dup2(t,0); dup2(t,1); dup2(t,2);
        if(t>2) close(t);
    }
    struct sigaction sa={.sa_handler=on_sig}; sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM,&sa,NULL); sigaction(SIGINT,&sa,NULL);
    printf("\033[2J\033[H"); fflush(stdout);
    int fd=open(KMSG_PATH,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0) fprintf(stderr,"boot-display: %s: %s\n",KMSG_PATH,strerror(errno));
    else { drain_kmsg(fd, now_ms()+d*1000LL); close(fd); }
    printf("\n"); reofetch();
    printf("\n\033[35m   Welcome to ReounderOS!\033[0m\n\n"); fflush(stdout);
    execl("/bin/login","login",(char*)NULL);
    perror("exec /bin/login");
    execl("/bin/reoshell","reoshell",(char*)NULL);
    perror("exec /bin/reoshell");
    _exit(1);
}
