#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: sleep [N ticks]\n");
        exit();
    }

    int n = atoi(argv[1]);

    if (sleep(n) < 0) {
        fprintf(2, "There is an error when calling \"sleep %d\"", n);
    }

	exit();
}
