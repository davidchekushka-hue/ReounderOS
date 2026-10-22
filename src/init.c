#include <sys/mount.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

int main(void) {
    mount("proc","/proc","proc",0,0);
    mount("sysfs","/sys","sysfs",0,0);
    mount("devtmpfs","/dev","devtmpfs",0,0);

    int t0 = open("/dev/tty0", O_RDWR);
    if (t0 < 0) t0 = open("/dev/console", O_RDWR);
    if (t0 >= 0) { dup2(t0, 0); dup2(t0, 1); dup2(t0, 2); if (t0 > 2) close(t0); }

    system("/bin/modprobe ata_piix"); // IDE
    system("/bin/modprobe ahci");     // SATA
    system("/bin/modprobe sd_mod");   // SCSI диски
    system("/bin/modprobe virtio_blk"); // Virtio
    system("/bin/modprobe virtio_pci");
    system("/bin/modprobe ext4");     // Файловая система
    
    sleep(1);
    execl("/boot/bootmenu","bootmenu",(char*)0);
    return 1;
}
