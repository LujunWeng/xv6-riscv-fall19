#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


const char*
basename(const char* path)
{
    const char* p;

    for (p = path+strlen(path); p >= path && *p != '/'; --p)
        ; // blank for-loop
    ++p;

    return p;
}


// Assuming path has an unlimited buffer
// Leave validation of path to system call.
void
find(char* path, const char* pattern)
{
    int fd;
    int base_len;
    struct dirent de;
    struct stat   st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot obtain metadata of %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        if (strcmp(pattern, basename(path)) == 0)
            printf("%s\n", path);
        break;
        
    case T_DIR:
        base_len = strlen(path);
        while ((read(fd, &de, sizeof(de)) == sizeof(de))) {
            if (de.inum == 0) continue;
            if (strcmp(".", de.name) == 0 || strcmp ("..", de.name) == 0) continue;

            path[base_len] = '/';
            safestrcpy(path + base_len + 1, de.name, DIRSIZ);
            find(path, pattern);
            path[base_len] = 0;
        }
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    char buf[512];

    if (argc != 3) {
        fprintf(2, "Usage: find dir pattern");
        exit();
    }

    memset(buf, 0, 512);
    strncpy(buf, argv[1], 512);
    find(buf, argv[2]);

    exit();
}