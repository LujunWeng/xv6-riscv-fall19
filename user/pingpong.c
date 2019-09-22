#include "kernel/types.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int parent_fd[2], child_fd[2];
    int pid;
    int i;
    char buf[5];
    const char *parent_str = "ping";
    const char *child_str  = "pong";

    if (pipe(parent_fd) != 0) {
        fprintf(2, "pipe(parent_fd) failed!\n");
        exit();
    }

    if (pipe(child_fd) != 0) {
        fprintf(2, "pipe(child_fd) failed!\n");
        exit();
    }

    i = 0;
    pid = fork();
    if (pid == 0) {
        // Child
        close(parent_fd[1]);
        close(child_fd[0]);

        while (child_str[i] != '\0') {
            if (read(parent_fd[0], buf+i, 1) != 1) {
                fprintf(2, "Child read failed!\n");
                exit();
            }
            if (write(child_fd[1], child_str+i, 1) != 1) {
                fprintf(2, "Child write failed!\n");
                exit();
            }
            ++i;
        }
        buf[i] = '\0';
    } else if (pid > 0) {
        // Parent
        close(parent_fd[0]);
        close(child_fd[1]);

        while (parent_str[i] != '\0') {
            if (write(parent_fd[1], parent_str+i, 1) != 1) {
                fprintf(2, "Parent write failed!\n");
                exit();
            }
            if (read(child_fd[0], buf+i, 1) != 1) { 
                fprintf(2, "Parent read failed!\n");
                exit();
            }
            ++i;
        }
        buf[i] = '\0';

        wait();
    } else {
        fprintf(2, "fork() failed!\n");
    }
    
    printf("%d: received %s\n", getpid(), buf);

    exit();
}