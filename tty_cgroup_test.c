#include "fcntl.h"
#include "types.h"
#include "user.h"
#include "test.h"
#include "param.h"

/*
static int read_test_done()
{
    int msg_fd = -1;
    int result = -1;
    msg_fd = open("/msg_read_test", O_RDWR | O_CREATE);

    if(0 > write(msg_fd, "1", 1))
    {
        result = -1;
        goto l_cleanup;
    }

result = 1;

l_cleanup:
    if(msg_fd > -1)
        close(msg_fd);
    return result;
}

static int write_test_done()
{
    int msg_fd = -1;
    int result = -1;
    msg_fd = open("/msg_write_test", O_RDWR | O_CREATE);

    if(0 > write(msg_fd, "1", 1))
    {
        result = -1;
        goto l_cleanup;
    }

    result = 1;
l_cleanup:
    if(msg_fd > -1)
        close(msg_fd);
    return result;
}


static int is_write_test_done()
{
    char buf[4] = {0};
    int msg_fd = -1;
    int result = -1;
    msg_fd = open("/msg_write_test", O_RDWR);

    if(0 > read(msg_fd, buf, 1))
    {   
        result = -1;
        goto l_cleanup;
    }

    result = atoi(buf);

l_cleanup:
    if(msg_fd > -1)
        close(msg_fd);
    return result;
}

static int is_read_test_done()
{
    char buf[4] = {0};
    int msg_fd = -1;
    int result = -1;
    msg_fd = open("/msg_read_test", O_RDWR);

    if(0 > read(msg_fd, buf, 1))
    {   
        result = -1;
        goto l_cleanup;
    }

    result = atoi(buf);

l_cleanup:
    if(msg_fd > -1)
        close(msg_fd);
    return result;
}
*/

int
main(int argc, char * argv[])
{
    char buf[256] = {0};
    //char tty_name[10] = {0};

    printf(1, "tty test started\n ");
    
    //open tty0
    int tty0_fd = open("/tty0", O_RDWR);
    printf(1, "got tty %d\n", tty0_fd);
    if(tty0_fd < 0)
    {
        printf(1, "Failed to open tty0!... \n");
        return -1;
    }
    
    
    // mount cgroup fs
    mkdir("/cgroup");
    mount(0, "/cgroup", "cgroup");

    //create cgroup
    mkdir("/cgroup/temp");
    
    char cgpath[256];
    memset(cgpath,'\0',256);
    strcpy(cgpath, "/cgroup/temp");
    strcat(cgpath,"/cgroup.subtree_control");
    int cgroup_subtree_control_fd = open(cgpath, O_RDWR);
    // Enable cpu controller
    memset(buf, '\0', 256);
    strcpy(buf, "+cpu");
   
    if(write(cgroup_subtree_control_fd, buf, sizeof(buf)) < 0)
        return -1;
    if(close(cgroup_subtree_control_fd) < 0)
        return -1;

    
    sleep(100);
    if(fork() == 0){
      if(unshare(2) != 0){
        printf(stderr, "Cannot create pid namespace\n");
        return -1;
      }
        int current_pid = fork();

        if(0 == current_pid)
        {
            printf(1, "in child proc: %d\n", getpid());
            sleep(200);
            if(0 != attach_tty(tty0_fd))
            {
                printf(stderr, "failed to attach tty\n");
                return -1;
            }
            connect_tty(tty0_fd);

            write(tty0_fd, "AAAAABBBBB", 10);
            write(tty0_fd, "AAAAABBBBB", 10);
            write(tty0_fd, "AAAAABBBBB", 10);
            write(tty0_fd, "AAAAABBBBB", 10);

            memset(buf, 0 , 256);
            read(tty0_fd, buf, 40);

            disconnect_tty(tty0_fd);
            close(tty0_fd);
            
            exit(0);
        }
        else
        {
            char buf2[100] = {0};
            strcpy(buf2, "/cgroup/temp");
            strcat(buf2,"/cgroup.procs");
            int cgroup_procs_fd = open(buf2, O_RDWR);
            char cur_pid_buf[10];
            itoa(cur_pid_buf, current_pid);
            if(write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0)
                return -1;
            if(close(cgroup_procs_fd) < 0)
                return -1;

            printf(1, "in parent proc\n");

            wait(0);
            exit(0);
        } 
    }
    exit(0);
    return 0;
}
//TODO: we should do this after fork()!!