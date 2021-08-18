#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
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
    if (open("new", 0) < 0) {
        printf(2, "objtest: cannot open new\n");
        exit(0);
    }
    printf(1, "objtest: after open\n");

    exit(0);
}
