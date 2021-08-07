#include "types.h"
#include "user.h"

#define BUF 256

void timer_test(void)
{
    char buf[BUF];
    int start, end, interval, err;

    gets(buf, BUF);
    start = uptime();
    interval = atoi(buf) * 1000;
    gets(buf, BUF);
    end = uptime();

    if(end - start < interval - (interval / 100))
        err = 1;
    else if(end - start > interval + (interval / 100))
        err = 1;
    else
        err = 0;
    printf(stderr, "TIMER_TEST: [%s] ", err ? "FAILURE" : "SUCCESS");
    printf(stderr, "measured: %d ms\n", end - start);

    exit(err);
}

int main(int argc, char *argv[])
{
    int i = 0;
    if(argc == 2 && !strcmp(argv[1],"test"))
        timer_test();
    while (1) {
        printf(1, "seconds: %d\n", i);
        usleep(1 * 1000 * 1000);
        ++i;
    }
    return 0;
}
