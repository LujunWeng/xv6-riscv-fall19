#include "kernel/types.h"
#include "user/user.h"


#define MAXARGS 10
#define BUFSIZE 100

int
get_line(char* buf, int max)
{
    int len;
    gets(buf, max);
    len = strlen(buf) - 1;
    if (len >= 0) {
        buf[len] = '\0';
    }
    return len;
}

int
extend_argv(char* line, char* cmdline[], int argc, int max_argc)
{
    int in_token = 0; // a flag

    for (int i = 0; argc < max_argc && line[i] != '\0'; ++i) {
        if (line[i] != '\t' && line[i] != ' ') {
            if (in_token == 0) {
                in_token = 1;
                cmdline[argc++] = line + i;
            }
        } else {
            in_token = 0;
            line[i]  = 0;
        }
    }

    return argc;
}

int
main(int argc, char* argv[])
{
    char buf[BUFSIZE];
    char* cmdline[MAXARGS+1];
    int base_argc, ext_argc;
    int pid;

    if (argc < 2) {
        fprintf(2, "Usage: xargs cmd options...");
        exit();
    }

    for (base_argc = 0; base_argc < argc-1; ++base_argc)
        cmdline[base_argc] = argv[base_argc+1];
    
    while (get_line(buf, BUFSIZE) >= 0) {
        if (strlen(buf) == 0) continue;

        ext_argc = extend_argv(buf, cmdline, base_argc, MAXARGS);
        cmdline[ext_argc] = 0;

        pid = fork();
        if (pid == 0) {
            exec(cmdline[0], cmdline);
            fprintf(2, "exec [%s] failed\n", cmdline[0]);
            exit();
        } else if (pid > 0) {
            wait();
        } else {
            fprintf(2, "xargs: fork failed\n");
            exit();
        }
    }

    exit();
}