#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("%d ticks since system started\n", uptime());
    exit();
}
