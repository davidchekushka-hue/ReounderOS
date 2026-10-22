CC      ?= gcc
CFLAGS  ?= -O2 -static -Wall -Wextra

SHELL_OBJS = main.o cmdbox.o cmds_core.o cmds_file.o cmds_text.o cmds_sys.o cmds_misc.o

all: reoshell reoinstall

reoshell: $(SHELL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

reoinstall: reoinstall.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c shell.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SHELL_OBJS) reoshell reoinstall
.PHONY: all clean
