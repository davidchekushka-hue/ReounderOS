#include "shell.h"
#include <stdio.h>
#include <string.h>

#define DECL(x) extern int cmd_##x(int,char**);
DECL(echo) DECL(pwd) DECL(cd) DECL(exit) DECL(help) DECL(clear) DECL(true) DECL(false)
DECL(ls) DECL(cat) DECL(mkdir) DECL(rmdir) DECL(rm) DECL(mv) DECL(cp) DECL(touch)
DECL(stat) DECL(readlink) DECL(basename) DECL(dirname) DECL(find)
DECL(head) DECL(tail) DECL(wc) DECL(grep) DECL(sort) DECL(uniq) DECL(tee)
DECL(uname) DECL(whoami) DECL(hostname) DECL(id) DECL(date) DECL(uptime)
DECL(free) DECL(df) DECL(ps) DECL(dmesg) DECL(env) DECL(export) DECL(unset)
DECL(which) DECL(mount) DECL(umount)
DECL(sleep) DECL(yes) DECL(seq) DECL(chmod) DECL(sync) DECL(reboot) DECL(poweroff) DECL(kill)
DECL(reoinstall)
#undef DECL

const struct cmd cmd_table[] = {
    {"echo","print arguments",cmd_echo},{"pwd","print working directory",cmd_pwd},
    {"cd","change directory",cmd_cd},{"exit","exit shell",cmd_exit},
    {"help","list commands",cmd_help},{"clear","clear screen",cmd_clear},
    {"true","return 0",cmd_true},{"false","return 1",cmd_false},
    {"ls","list directory",cmd_ls},{"cat","concatenate files",cmd_cat},
    {"mkdir","create directory",cmd_mkdir},{"rmdir","remove empty directory",cmd_rmdir},
    {"rm","remove file",cmd_rm},{"mv","move/rename",cmd_mv},
    {"cp","copy file",cmd_cp},{"touch","create empty file",cmd_touch},
    {"stat","file info",cmd_stat},{"readlink","resolve symlink",cmd_readlink},
    {"basename","strip path",cmd_basename},{"dirname","path directory part",cmd_dirname},
    {"find","recursive list",cmd_find},
    {"head","first N lines",cmd_head},{"tail","last N lines",cmd_tail},
    {"wc","word/line/char count",cmd_wc},{"grep","search text",cmd_grep},
    {"sort","sort lines",cmd_sort},{"uniq","dedupe adjacent lines",cmd_uniq},
    {"tee","write to stdout and files",cmd_tee},
    {"uname","system info",cmd_uname},{"whoami","current user",cmd_whoami},
    {"hostname","host name",cmd_hostname},{"id","user/group ids",cmd_id},
    {"date","current date/time",cmd_date},{"uptime","system uptime",cmd_uptime},
    {"free","memory usage",cmd_free},{"df","disk usage",cmd_df},
    {"ps","process list",cmd_ps},{"dmesg","kernel log",cmd_dmesg},
    {"env","environment",cmd_env},{"export","set env var",cmd_export},
    {"unset","unset env var",cmd_unset},{"which","locate command",cmd_which},
    {"mount","show mounts",cmd_mount},{"umount","unmount",cmd_umount},
    {"sleep","sleep seconds",cmd_sleep},{"yes","repeat string",cmd_yes},
    {"seq","print numbers",cmd_seq},{"chmod","change mode",cmd_chmod},
    {"sync","flush filesystems",cmd_sync},{"reboot","reboot",cmd_reboot},
    {"poweroff","power off",cmd_poweroff},{"kill","send SIGTERM",cmd_kill},
    {"reoinstall","install ReounderOS to a disk",cmd_reoinstall},
};
const int cmd_count = sizeof(cmd_table)/sizeof(cmd_table[0]);

int run_command(int argc, char **argv){
    for(int i=0;i<cmd_count;i++)
        if(!strcmp(cmd_table[i].name, argv[0]))
            return cmd_table[i].fn(argc, argv);
    fprintf(stderr,"reoshell: %s: command not found\n", argv[0]);
    return 127;
}
