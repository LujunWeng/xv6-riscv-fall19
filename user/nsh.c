#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define STDIN 0
#define MAXLINE 100
#define MAXARGS 10 
#define MAXCMDS 10


int shell_exit = -1;

struct cmd {
    char* argv[MAXARGS+1];  // arguments including command exec file, NULL-terminated
    int   in_fd;            // redirect standard input to in_fd
    int   out_fd;           // redirect standard output to out_fd
};

struct cmdline {
    struct cmd cmds[MAXCMDS];  // an array of commands from the input of a command line.
    int        n;              // Number of parsed commands 
};

void
close_io_fds(struct cmd* cp)
{
    if (cp->in_fd > 2) close(cp->in_fd);
    if (cp->out_fd > 2) close(cp->out_fd);
}

void
print_cmdline(struct cmdline* cp)
{
    printf("n: %d\n", cp->n);
    for (int i = 0; i <= cp->n; ++i) {
        printf("%d:", i);
        printf("%s %s", cp->cmds[i].argv[0], cp->cmds[i].argv[1]);
        printf(":%d:%d\n", cp->cmds[i].in_fd, cp->cmds[i].out_fd); 
    }
}

int
start_with(char* s, char* p)
{
    while (*s != '\0' && *p != '\0') {
        if (*s++ != *p++)
            return 1;
    }
    if (*s == '\0' && *p != '\0') return 1;

    return 0;
}

char*
next_token()
{
    static char buf[500];
    static char c = '\0';
    int  i;

    if (c == '\0' && read(STDIN, &c, 1) < 1) {
        c = '\0';
        shell_exit = 0;
        return 0;
    }

    while (c == ' ' || c == '\t') {
        if (read(STDIN, &c, 1) < 1) {
            shell_exit = 0;
            break;
        }
    }

    if (c == '|' || c == '<' || c == '>') {
        buf[0] = c;
        buf[1] = '\0';
        c = '\0';
    } else if (c == '\n' || c == '\r') {
        c = '\0';
        return 0;
    } else {
        i = 0;
        while (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            buf[i++] = c;
            if (i == 500-1) break;
            if (read(STDIN, &c, 1) < 1) {
                shell_exit = 0;
                break;
            }
        }
        buf[i] = '\0';
    }

    return buf;
}

int
parse_next_cmd(struct cmdline* cp, char* buf, int max)
{
    char* p;
    char* bufp = buf;
    char** argvp;
    int fd;
    int i;

    cp->n = 0;
    argvp = cp->cmds[cp->n].argv;
    while ((p = next_token()) != 0) {
        // printf("token:%s\n", p);
        if (strcmp(p, "|") == 0) {
            ++(cp->n);
            argvp = cp->cmds[cp->n].argv;
        } else if (strcmp(p, "<") == 0) {
            p = next_token();
            if (p == 0) {
                fprintf(2, "file needed!");
                return -1;
            }
            if ((fd = open(p, O_RDONLY)) < 0) {
                fprintf(2, "cannot open file!");
                return -1;
            }
            cp->cmds[cp->n].in_fd = fd;
        } else if (strcmp(p, ">") == 0) {
            p = next_token();
            if (p == 0) {
                fprintf(2, "file needed!");
                return -1;
            }
            if ((fd = open(p, O_WRONLY|O_CREATE)) < 0) {
                fprintf(2, "cannot open file!");
                return -1;
            }
            cp->cmds[cp->n].out_fd = fd;
        } else {
            *argvp++ = strcpy(bufp, p);
            bufp += strlen(p) + 1;
        }
    }        

    for (i = 1; i <= cp->n; i += 2) {
        int fds[2];
        if (pipe(fds) < 0) {
            fprintf(2, "cannot allocate pipe file descriptors!");
            return -1;
        }
        cp->cmds[i-1].out_fd = fds[1];
        cp->cmds[i].in_fd    = fds[0];
    }

    return 0;
}

void
runcmd(struct cmd* cp)
{
    if (cp->in_fd > 2) {
        close(0);
        dup(cp->in_fd);
        close(cp->in_fd);
    }
    if (cp->out_fd > 2) {
        close(1);
        dup(cp->out_fd);
        close(cp->out_fd);
    }

    exec(cp->argv[0], cp->argv);
}

int
main(int argc, char *argv[])
{
    struct cmdline cmdline;
    char buf[500];
    int i, pid;

    while (shell_exit != 0) {
        printf("@ ");

        memset(buf, 0, sizeof(buf));
        memset(&cmdline, 0, sizeof(cmdline));
        if (parse_next_cmd(&cmdline, buf, 500) < 0) {
            // Clean up open files
            for (i = 0; i <= cmdline.n; ++i) {
                close_io_fds(&(cmdline.cmds[i]));
            }
            continue;
        }

        // print_cmdline(&cmdline);

        if (cmdline.cmds[0].argv[0] == 0) continue;

        if (start_with(cmdline.cmds[0].argv[0], "exit") == 0)
            break;

        for (i = 0; i <= cmdline.n; ++i) {
            pid = fork();
            if (pid == 0) {
                runcmd(&cmdline.cmds[i]);    // exec is called inside and so error when return
                fprintf(2, "exec %s failed!\n", cmdline.cmds[i].argv[0]);
                exit(-1);
            } else if (pid > 0) {
                close_io_fds(&(cmdline.cmds[i]));
                continue;
            } else {
                fprintf(2, "fork failed for executing %s!\n", cmdline.cmds[i].argv[0]);
                exit(-1);
            }
        } 

        // Wait for all children to exit
        while (wait(0) != -1) { }
    }
    exit(0);
}
