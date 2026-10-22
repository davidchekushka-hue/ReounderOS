/* reoinstall — ReounderOS disk setup */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/select.h>

#define CLR   "\033[2J\033[H"
#define RESET "\033[0m"
#define BG    "\033[44m"
#define FG    "\033[37m"
#define HDR   "\033[47;34m"
#define SEL   "\033[47;30m"
#define LOGH  "\033[46;30m"
#define LOGFG "\033[46;37m"
#define W     78
#define LOG_ROWS 5
#define LOG_MAX  256

static struct termios g_saved;
static int g_tty = -1, g_raw = 0;
static char g_log[LOG_MAX][128];
static int  g_log_n = 0;

static void logmsg(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int i = g_log_n % LOG_MAX;
    vsnprintf(g_log[i], sizeof(g_log[i]), fmt, ap);
    va_end(ap);
    g_log_n++;
}

static void raw_on(void){
    struct termios t;
    if(tcgetattr(g_tty,&t)<0) return;
    g_saved=t; cfmakeraw(&t);
    t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    if(tcsetattr(g_tty,TCSANOW,&t)==0) g_raw=1;
}
static void raw_off(void){ if(g_raw) tcsetattr(g_tty,TCSANOW,&g_saved); g_raw=0; }
static void clr(void){ fputs(CLR BG FG, stdout); }
static void at(int r,int c){ printf("\033[%d;%dH", r, c); }
static void reset(void){ fputs(RESET BG FG, stdout); }

static void bar(int row, const char *left, const char *right){
    at(row,1); fputs(HDR, stdout);
    int lp=(int)strlen(left), rp = right ? (int)strlen(right) : 0;
    fputs(left, stdout);
    for(int i=0;i<W-lp-rp;i++) putchar(' ');
    if(right) fputs(right, stdout);
    fputs(RESET BG FG, stdout);
}
static void line(int row, const char *text){
    at(row,1); printf("%-*.*s", W, W, text);
}

/* лог-панель: top_row — строка заголовка, ниже LOG_ROWS строк */
static void draw_log(int top_row) {
    at(top_row, 1); fputs(LOGH, stdout);
    printf("%-*.*s", W, W, " Log ");
    fputs(RESET BG FG, stdout);
    for (int r = 0; r < LOG_ROWS; r++) {
        at(top_row + 1 + r, 1);
        fputs(LOGFG, stdout);
        int idx = g_log_n - LOG_ROWS + r;
        char buf[W+1] = "";
        if (idx >= 0 && idx < g_log_n)
            snprintf(buf, sizeof(buf), " %s", g_log[idx % LOG_MAX]);
        printf("%-*.*s", W, W, buf);
        fputs(RESET BG FG, stdout);
    }
}

struct disk { char name[32]; char model[64]; long sectors; };


static int scan_disks(struct disk *out, int max){
    DIR *d = opendir("/sys/block");
    if(!d){ logmsg("ERR: open /sys/block: %s", strerror(errno)); return 0; }
    int n = 0, total = 0;
    struct dirent *e;
    while((e = readdir(d)) && n < max){
        if(e->d_name[0]=='.') continue;
        total++;
        
        // Отсеиваем только виртуальные псевдо-диски
        if(!strncmp(e->d_name,"loop",4) || !strncmp(e->d_name,"ram",3) ||
           !strncmp(e->d_name,"sr",2)   || !strncmp(e->d_name,"fd",2)  ||
           !strncmp(e->d_name,"dm-",3)){
            logmsg("skip %s (pseudo)", e->d_name);
            continue;
        }
        
        char path[256];
        long sectors = 0;
        snprintf(path,sizeof(path),"/sys/block/%s/size", e->d_name);
        FILE *f = fopen(path,"r");
        if(!f){ logmsg("skip %s (no size)", e->d_name); continue; }
        if(fscanf(f,"%ld",&sectors)!=1) sectors=0;
        fclose(f);
        
        if(sectors <= 0){ logmsg("skip %s (0 size)", e->d_name); continue; }
        
        // Берем диск без проверок /dev
        struct disk *dk = &out[n++];
        memset(dk,0,sizeof(*dk));
        strncpy(dk->name, e->d_name, sizeof(dk->name)-1);
        dk->sectors = sectors;
        
        snprintf(path,sizeof(path),"/sys/block/%s/device/model", e->d_name);
        f = fopen(path,"r");
        if(f){
            if(fgets(dk->model,sizeof(dk->model),f)){
                char *p = strchr(dk->model,'\n'); if(p)*p=0;
            }
            fclose(f);
        } else strcpy(dk->model,"(unknown)");
        
        logmsg("found /dev/%s (%ldM, %s)", dk->name, sectors*512/1024/1024, dk->model);
    }
    closedir(d);
    logmsg("scan: %d entries, %d usable", total, n);
    return n;
}
static const char *detect_fs(const char *dev){
    int fd = open(dev,O_RDONLY); if(fd<0) return NULL;
    unsigned char buf[4096];
    if(pread(fd,buf,2048,1024)==2048 && buf[0x38]==0x53 && buf[0x39]==0xEF){
        unsigned int inc = buf[0x60]|(buf[0x61]<<8)|(buf[0x62]<<16)|((unsigned)buf[0x63]<<24);
        unsigned int cm  = buf[0x5C]|(buf[0x5D]<<8)|(buf[0x5E]<<16)|((unsigned)buf[0x5F]<<24);
        close(fd);
        if(inc & 0x40) return "ext4";
        if(cm & 0x4)   return "ext3";
        return "ext2";
    }
    close(fd);
    fd = open(dev,O_RDONLY);
    if(fd>=0){
        if(pread(fd,buf,512,0)==512){
            if(memcmp(buf,"XFSB",4)==0){ close(fd); return "xfs"; }
            if(buf[3]=='N'&&buf[4]=='T'&&buf[5]=='F'&&buf[6]=='S'){ close(fd); return "ntfs"; }
            if((buf[54]=='F'&&buf[55]=='A'&&buf[56]=='T') ||
               (buf[82]=='F'&&buf[83]=='A'&&buf[84]=='T')){ close(fd); return "vfat"; }
        }
        close(fd);
    }
    return NULL;
}

static void copy_file(const char *s,const char *t,mode_t m){
    int fi = open(s,O_RDONLY); if(fi<0){ logmsg("ERR open %s: %s",s,strerror(errno)); return; }
    int fo = open(t,O_WRONLY|O_CREAT|O_TRUNC, m & 07777);
    if(fo<0){ logmsg("ERR create %s: %s",t,strerror(errno)); close(fi); return; }
    char buf[8192]; ssize_t r; long total=0;
    while((r = read(fi,buf,sizeof(buf))) > 0){ write(fo,buf,r); total+=r; }
    close(fi); close(fo);
    logmsg("cp %s (%ld B)", t, total);
}
static void copy_tree(const char *src,const char *dst){
    mkdir(dst,0755);
    logmsg("mkdir %s", dst);
    DIR *d = opendir(src); if(!d) return;
    struct dirent *e;
    while((e = readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue;
        const char *nm = e->d_name;
        if(!strcmp(src,"/") && (
           !strcmp(nm,"proc")||!strcmp(nm,"sys")||!strcmp(nm,"dev")||
           !strcmp(nm,"mnt")||!strcmp(nm,"tmp")||!strcmp(nm,"run"))) {
            logmsg("skip %s/ (pseudo)", nm);
            continue;
        }
        char s[4096],t[4096];
        snprintf(s,sizeof(s),"%s/%s",src,nm);
        snprintf(t,sizeof(t),"%s/%s",dst,nm);
        struct stat st;
        if(lstat(s,&st)<0) continue;
        if(S_ISDIR(st.st_mode)){ mkdir(t,st.st_mode&07777); copy_tree(s,t); }
        else if(S_ISLNK(st.st_mode)){
            char ln[4096]; ssize_t n = readlink(s,ln,sizeof(ln)-1);
            if(n>0){ ln[n]=0; symlink(ln,t); logmsg("ln %s -> %s",t,ln); }
        } else if(S_ISREG(st.st_mode)) copy_file(s,t,st.st_mode);
    }
    closedir(d);
}

/* ---- screens ---- */
static void draw_welcome(void){
    clr();
    bar(1," Install ReounderOS",NULL);
    for(int i=2;i<4;i++) line(i,"");
    line(4,"   Welcome to Setup.");
    line(5,"");
    line(6,"   This portion of the Setup program prepares ReounderOS");
    line(7,"   to run on your device.");
    line(8,"");
    line(9,"     * To set up ReounderOS now, press ENTER.");
    line(10,"");
    line(11,"     * To repair a ReounderOS installation, press R.");
    line(12,"");
    line(13,"     * To quit Setup without installing ReounderOS, press F3.");
    for(int i=14;i<22;i++) line(i,"");
    bar(23," ENTER=Continue   R=Repair   F3=Exit",NULL);
    reset(); fflush(stdout);
}

static void draw_disks(const struct disk *dk,int n,int sel){
    clr();
    bar(1," Install ReounderOS",NULL);
    line(2,"");
    line(3,"   Please select the disk to install ReounderOS on.");
    line(4,"");
    line(5,"        Device           Size       Model");
    line(6,"        ----------------------------------------------");
    if(!n) line(7,"        (no disks detected)");
    for(int i=0;i<n && i<9;i++){
        at(7+i,1);
        if(i==sel) fputs(SEL, stdout);
        char buf[128];
        snprintf(buf,sizeof(buf),"       %c /dev/%-9s %6ldM    %s",
                 i==sel?'>':' ', dk[i].name,
                 dk[i].sectors*512/1024/1024, dk[i].model);
        printf("%-*.*s",W,W,buf);
        fputs(RESET BG FG, stdout);
    }
    draw_log(16);
    bar(23," ENTER=Install   ESC=Cancel",NULL);
    reset(); fflush(stdout);
}

static void draw_confirm(const char *dev,const char *fs){
    clr();
    bar(1," Install ReounderOS",NULL);
    line(2,"");
    line(3,"   Please confirm the installation target.");
    line(4,"");
    line(5,"   Setup is about to install ReounderOS on:");
    line(6,"");
    at(7,1); printf("        " BG "\033[97m%s\033[37m" "  (\033[97m%s\033[37m)", dev, fs?fs:"no fs");
    line(8,"");
    line(9,"   Existing files will be preserved unless they collide");
    line(10,"   with files being installed by ReounderOS.");
    line(11,"");
    line(12,"   Press ENTER to begin installation.");
    line(13,"   Press ESC to return to disk selection.");
    draw_log(16);
    bar(23," ENTER=Continue   ESC=Back",NULL);
    reset(); fflush(stdout);
}

static void draw_progress(int pct,const char *msg){
    clr();
    bar(1," Install ReounderOS",NULL);
    line(2,"");
    line(3,"");
    at(4,1); printf("   %-*.*s", W-3, W-3, msg);
    line(5,"");
    char b[W+1]; int filled=(W-10)*pct/100;
    memset(b,0xDB,filled);
    for(int i=filled;i<W-10;i++) b[i]=' ';
    b[W-10]=0;
    at(6,1);
    printf("     [" BG "\033[97m%s\033[37m" "] " BG "\033[97m%3d%%\033[37m", b, pct);
    for(int i=8;i<15;i++) line(i,"");
    draw_log(15);
    bar(23," Please wait...",NULL);
    reset(); fflush(stdout);
}

static void draw_done(const char *dev){
    clr();
    bar(1," Install ReounderOS",NULL);
    line(2,"");
    line(3,"");
    line(4,"   Setup has finished installing ReounderOS on");
    line(5,"");
    at(6,1); printf("        " BG "\033[97m%s\033[37m", dev);
    line(7,"");
    line(8,"   You may safely remove the installation media now.");
    line(9,"");
    line(10,"   Press ENTER to reboot the device,");
    line(11,"   or F3 to return to the command prompt.");
    for(int i=12;i<22;i++) line(i,"");
    bar(23," ENTER=Reboot   F3=Exit",NULL);
    reset(); fflush(stdout);
}

static int readkey(void){
    unsigned char c;
    if(read(g_tty,&c,1)!=1) return 0;
    if(c==27){
        unsigned char s[2];
        fd_set fs; FD_ZERO(&fs); FD_SET(g_tty,&fs);
        struct timeval tv={0,50000};
        if(select(g_tty+1,&fs,NULL,NULL,&tv)<=0) return 27;
        if(read(g_tty,&s[0],1)!=1) return 27;
        if(s[0]!='[') return 27;
        if(read(g_tty,&s[1],1)!=1) return 27;
        if(s[1]=='A') return 0x100;
        if(s[1]=='B') return 0x101;
    }
    return c;
}

int main(void){
    g_tty = open("/dev/tty0",O_RDWR|O_NOCTTY);
    if(g_tty<0) g_tty = open("/dev/console",O_RDWR|O_NOCTTY);
    if(g_tty<0) g_tty = open("/dev/tty",O_RDWR|O_NOCTTY);
    if(g_tty<0){ perror("reoinstall"); return 1; }
    dup2(g_tty,0); dup2(g_tty,1); dup2(g_tty,2);

    raw_on();

    for(;;){
        draw_welcome();
        int k = readkey();
        if(k==13||k==10) break;
        if(k==27){ raw_off(); fputs(CLR "reoinstall cancelled.\n" RESET,stdout); return 0; }
    }

    logmsg("=== ReounderOS setup ===");
    logmsg("scanning /sys/block...");
    struct disk disks[16];
    int nd = scan_disks(disks,16);
    int sel = 0;
    for(;;){
        draw_disks(disks,nd,sel);
        int k = readkey();
        if(k==0x100 && sel>0) sel--;
        else if(k==0x101 && sel<nd-1) sel++;
        else if(k==27){ raw_off(); return 0; }
        else if((k==13||k==10) && nd>0) break;
    }
    if(!nd){ raw_off(); fputs(CLR "No disks found.\n" RESET,stdout); return 1; }

    char dev[64]; snprintf(dev,sizeof(dev),"/dev/%s",disks[sel].name);
    char part_dev[64]; snprintf(part_dev, sizeof(part_dev), "%s1", dev);
    char cmd[512];

    draw_progress(5, "   Partitioning disk...");
    logmsg("running fdisk on %s", dev);
    // Создаем MBR (o), новый раздел (n, p, 1), на весь диск, сохраняем (w)
    snprintf(cmd, sizeof(cmd), "echo -e 'o\nn\np\n1\n\n\nw' | fdisk %s >/dev/null 2>&1", dev);
    system(cmd);
    sleep(1); // Ждем, пока ядро создаст /dev/vda1

    draw_progress(10, "   Formatting partition...");
    logmsg("mkfs.ext4 %s", part_dev);
    snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F -L REOS-DATA %s >/dev/null 2>&1", part_dev);
    system(cmd);

    draw_progress(15,"   Mounting partition...");
    mkdir("/mnt",0755);
    logmsg("mount %s -> /mnt", part_dev);
    if(mount(part_dev,"/mnt","ext4",0,NULL)<0){
        logmsg("ERR mount: %s", strerror(errno));
        draw_progress(15,"   Mount failed — see log below");
        sleep(4);
        return 1;
    }

    draw_progress(15,"   Copying system files...");
    copy_tree("/","/mnt");

    draw_progress(60,"   Setting up bootloader...");
    
    // Явно копируем apk и базу пакетов, чтобы copy_tree не прое*ал их
    logmsg("ensuring apk is present...");
    system("mkdir -p /mnt/sbin /mnt/lib/apk /mnt/etc/apk");
    system("cp -a /sbin/apk /mnt/sbin/ 2>/dev/null");
    system("cp -a /lib/apk/* /mnt/lib/apk/ 2>/dev/null");
    system("cp -a /etc/apk/* /mnt/etc/apk/ 2>/dev/null");

    // Копируем ядро, если его нет
    system("cp -a /boot/vmlinuz-virt /mnt/boot/ 2>/dev/null");
    
    // Монтируем proc/sys/dev внутрь /mnt для chroot
    mkdir("/mnt/proc",0755); mount("proc", "/mnt/proc", "proc", 0, 0);
    mkdir("/mnt/sys",0755);  mount("sysfs", "/mnt/sys", "sysfs", 0, 0);
    mkdir("/mnt/dev",0755);  mount("devtmpfs", "/mnt/dev", "devtmpfs", 0, 0);

    // Генерируем initramfs на диске
    logmsg("running mkinitfs...");
    int ret = system("chroot /mnt /sbin/mkinitfs -o /boot/initramfs-virt");
    if(ret != 0) logmsg("WARN: mkinitfs failed");

    // Устанавливаем GRUB
    logmsg("installing grub...");
    char grub_cmd[256];
    snprintf(grub_cmd, sizeof(grub_cmd), "chroot /mnt /usr/sbin/grub-install --target=i386-pc --recheck %s", dev); // dev - это весь диск
    system(grub_cmd);

    // Пишем grub.cfg
    logmsg("writing grub.cfg...");
    FILE *grub = fopen("/mnt/boot/grub/grub.cfg", "w");
    if(grub) {
        fprintf(grub, "set timeout=5\nset default=0\n\n");
        fprintf(grub, "menuentry 'ReounderOS' {\n");
        fprintf(grub, "    linux /boot/vmlinuz-virt root=%s rw init=/init console=tty0\n", part_dev); // part_dev - это /dev/vda1
        fprintf(grub, "    initrd /boot/initramfs-virt\n}\n");
        fclose(grub);
    }

    // Размонтируем
    umount("/mnt/dev/pts");
    umount("/mnt/dev");
    umount("/mnt/sys");
    umount("/mnt/proc");

    draw_progress(85,"   Syncing disks...");
    logmsg("sync");
    sync();

    draw_progress(93,"   Unmounting target...");
    logmsg("umount /mnt");
    if(umount("/mnt")<0){
        logmsg("ERR umount: %s", strerror(errno));
        draw_progress(93,"   Unmount failed — see log below");
        sleep(4);
        return 1;
    }
    logmsg("umount OK");
    logmsg("=== install complete ===");

    draw_progress(100,"   Setup is complete.");
    sleep(2);

    for(;;){
        draw_done(dev);
        int k = readkey();
        if(k==13||k==10){ raw_off(); sync(); reboot(RB_AUTOBOOT); perror("reboot"); break; }
        if(k==27){ raw_off(); return 0; }
    }
    raw_off();
    return 0;
}
