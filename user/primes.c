#include "kernel/types.h"
#include "user/user.h"


const int int_size = sizeof(int);

int
read_int(int fd, int *np)
{
    int sz = sizeof(int);
    int nb;

    while ((nb = read(fd, np, sz)) < sz) {
        if (nb <= 0) return nb;
        np += nb;
        sz -= nb;
    }

    return 1;
}

void 
prime_pipe(int receiver_fd)
{
    int sender_fd   = -1;
    int prime = 2;
    int n, pid;

    printf("prime %d\n", prime);
    while (read_int(receiver_fd, &n) > 0) {
        if (n % prime == 0) continue;

        if (sender_fd == -1) {
            int tmp_fds[2];
            if (pipe(tmp_fds) != 0) {
                fprintf(2, "pipe failed!");
                exit();
            }

            pid = fork();
            if (pid > 0) {
                sender_fd = tmp_fds[1];
                close(tmp_fds[0]);
            } else if (pid == 0) {
                prime = n;
                printf("prime %d\n", prime);

                receiver_fd = tmp_fds[0];
                close(tmp_fds[1]);

                continue;
            } else {
                fprintf(2, "fork failed!");
                exit();
            }
        }

        if (write(sender_fd, &n, int_size) != int_size) {
            fprintf(2, "write failed!");
            exit();
        }
    }

    // Close pipe properly
    if (sender_fd != -1) close(sender_fd);
    close(receiver_fd);
    wait(); 
}

int
main(int argc, char *argv[])
{
    int pid;
    int pipe_fds[2];

    if (pipe(pipe_fds) != 0) {
        fprintf(2, "pipe failed!");
        exit();
    }

    pid = fork();
    if (pid > 0) {
        close(pipe_fds[0]);

        for (int i = 2; i <= 35; ++i) {
            if (write(pipe_fds[1], &i, int_size) != int_size) {
                fprintf(2, "write failed!");
                exit();
            }
        }

        close(pipe_fds[1]);

        // Wait the entire pipe to shut down
        wait();
    } else if (pid == 0) {
        close(pipe_fds[1]);

        prime_pipe(pipe_fds[0]);

        close(pipe_fds[0]);
    } else {
        fprintf(2, "fork failed!");
        exit();
    }
 
    exit();
}