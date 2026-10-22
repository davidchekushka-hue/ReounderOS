#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/reboot.h>
#include <sys/select.h>

#define ESC 27
#define RESCUE_SHELL "/bin/reoshell"

static struct termios g_saved;
static int g_tty = -1;
static int g_raw = 0;

static void raw_off(void){
    if(g_raw && g_tty >= 0){
        tcsetattr(g_tty, TCSANOW, &g_saved);
        g_raw = 0;
    }
}
static void raw_on(void){
    struct termios t;
    if(tcgetattr(g_tty, &t) < 0) return;
    g_saved = t;
    cfmakeraw(&t);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if(tcsetattr(g_tty, TCSANOW, &t) == 0) g_raw = 1;
}
static void on_fatal(int s){ raw_off(); _exit(128 + s); }

static const char *g_items[] = {
    "Boot ReounderOS",
    "Boot ReounderOS (verbose logs)",
    "Boot to shell (rescue)",
    "Reboot",
    "Poweroff",
};
#define NITEMS ((int)(sizeof(g_items)/sizeof(g_items[0])))

static void draw(int sel){
    printf("\033[2J\033[H");
    printf("\033[36m");
    printf("  ┌──────────────────────────────────────────────────┐\r\n");
    printf("  │              ReounderOS Boot Menu                │\r\n");
    printf("  │                  aarch64 / x86_64                │\r\n");
    printf("  └──────────────────────────────────────────────────┘\r\n");
    printf("\033[0m\r\n");
    for(int i=0;i<NITEMS;i++){
        if(i == sel) printf("\033[7m\033[35m  >  %-46s \033[0m\r\n", g_items[i]);
        else         printf("\033[90m     %-46s\033[0m\r\n", g_items[i]);
    }
    printf("\r\n\033[32m  ↑/↓  выбор    Enter  загрузка    r  reboot    p  poweroff\033[0m\r\n");
    fflush(stdout);
}

static int menu(void){
    int sel = 0;
    for(;;){
        draw(sel);
        unsigned char c;
        if(read(g_tty, &c, 1) != 1) continue;
        if(c == ESC){
            unsigned char seq[2];
            fd_set fs; FD_ZERO(&fs); FD_SET(g_tty, &fs);
            struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 };
            if(select(g_tty+1, &fs, NULL, NULL, &tv) <= 0) continue;
            if(read(g_tty, &seq[0], 1) != 1) continue;
            if(seq[0] != '[') continue;
            if(read(g_tty, &seq[1], 1) != 1) continue;
            if(seq[1] == 'A') sel = (sel + NITEMS - 1) % NITEMS;
            else if(seq[1] == 'B') sel = (sel + 1) % NITEMS;
            continue;
        }
        if(c == '\r' || c == '\n') return sel;
        if(c == 'r' || c == 'R'){ raw_off(); sync(); reboot(RB_AUTOBOOT);  perror("reboot"); }
        if(c == 'p' || c == 'P'){ raw_off(); sync(); reboot(RB_POWER_OFF); perror("poweroff"); }
    }
}

int main(void){
    /* приоритет: tty0 → console → tty (для работы и в VGA, и в serial) */
    g_tty = open("/dev/tty0", O_RDWR | O_NOCTTY);
    if(g_tty < 0) g_tty = open("/dev/console", O_RDWR | O_NOCTTY);
    if(g_tty < 0) g_tty = open("/dev/tty", O_RDWR | O_NOCTTY);
    if(g_tty < 0){
        execl("/bin/boot-display", "boot-display", "8", (char *)0);
        return 1;
    }

    struct sigaction sa = { .sa_handler = on_fatal };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);

    raw_on();
    int sel = menu();
    raw_off();
    close(g_tty); g_tty = -1;

    switch(sel){
        case 0: execl("/bin/boot-display", "boot-display", "5",  (char *)0); break;
        case 1: execl("/bin/boot-display", "boot-display", "30", (char *)0); break;
        case 2: execl(RESCUE_SHELL, "reoshell", (char *)0); break;
        case 3: sync(); reboot(RB_AUTOBOOT);  perror("reboot");  break;
        case 4: sync(); reboot(RB_POWER_OFF); perror("poweroff"); break;
    }
    perror("boot: exec");
    return 1;
}
