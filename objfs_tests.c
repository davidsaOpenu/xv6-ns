#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int fd;
    struct stat st;

    printmounts();
    printdevices();

    if (mkdir("new") != 0) {
        printf(2, "objtest: failed to create dir new\n");
    }

    printf(1, "objtest: before mount\n");
    if (mount(0, "new", "objfs") != 0) {
        printf(2, "objtest: failed to mount objfs to new\n");
        exit(0);
    }
    printf(1, "objtest: after mount\n");

    printf(1, "objtest: before open\n");
    if ((fd = open("new", 0)) < 0) {
        printf(2, "objtest: cannot open new\n");
        exit(0);
    }
    printf(1, "objtest: after open\n");
    if(fstat(fd, &st) < 0){
        printf(2, "ls: cannot stat new\n");
        close(fd);
        exit(0);
    }

    printf(1, "objtest: after fstat\n");
    if(chdir("new") < 0) {
        printf(2, "objtest: failed to chdir to new\n");
    }
    printf(1, "objtest: after chdir\n");

    exit(0);
}
