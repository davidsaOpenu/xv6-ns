#include "fcntl.h"
#include "types.h"
#include "user.h"

void mount_cgroup_fs(const char * path)
{
    mount(0, path, "cgroup");
}

void make_cgroup_directory(const char * path)
{
    mkdir(path);
}

void delete_cgroup_directory(const char * path)
{
    unlink(path);
}

void unmount_cgroup_fs(const char * path)
{
    umount(path);
}

/**
 * Make a directory for the cgroup filesystem. (/cgroup)
 * Mount the cgroup filesystem at the directory. (/cgroup)
 * Make new cgroup directories and create a hierarchy at following paths:
 * 1)	"/cgroup/test1"
 * 2)	"/cgroup/test2"
 * 3)	"/cgroup/test1/test1.1"
 * 4) 	"/cgroup/test1/test1.2"
 */
void test_creating_cgroups()
{
    mkdir("/cgroup");
    mount_cgroup_fs("/cgroup");
    make_cgroup_directory("/cgroup/test1");
    make_cgroup_directory("/cgroup/test2");
    make_cgroup_directory("/cgroup/test1/test1.1");
    make_cgroup_directory("/cgroup/test1/test1.2");
}

/**
 * Opens the cgroup file at the path cgroup with name file_name and returns
 * the file descriptor.
 */
int open_cgroup_file(const char * path, char * file_name)
{
    char file_path[128];
    strcpy(file_path, path);
    int i;
    for (i = 0; file_path[i] != 0; i++)
        ;
    file_path[i++] = '/';
    strcpy(&file_path[i], file_name);

    return open(file_path, O_RDWR);
}

void test_opening_cgroup_files(const char * path,
                               int * cgroup_procs_fd,
                               int * cgroup_controllers_fd,
                               int * cgroup_subtree_control_fd,
                               int * cgroup_events_fd,
                               int * cgroup_max_descendants_fd,
                               int * cgroup_max_depth_fd,
                               int * cgroup_stat_fd)
{
    *cgroup_procs_fd = open_cgroup_file(path, "cgroup.procs");
    *cgroup_controllers_fd = open_cgroup_file(path, "cgroup.controllers");
    *cgroup_subtree_control_fd =
        open_cgroup_file(path, "cgroup.subtree_control");
    *cgroup_events_fd = open_cgroup_file(path, "cgroup.events");
    *cgroup_max_descendants_fd =
        open_cgroup_file(path, "cgroup.max.descendants");
    *cgroup_max_depth_fd = open_cgroup_file(path, "cgroup.max.depth");
    *cgroup_stat_fd = open_cgroup_file(path, "cgroup.stat");
}

void empty_string(char * string)
{
    for (int i = 0; i < strlen(string); i++)
        string[i] = 0;
}

/**
 * Open, read, print, and close all files in cgroup located at path.
 */
void test_reading_cgroup_files(const char * path)
{
    char buf[256];
    int cgroup_procs_fd = open_cgroup_file(path, "cgroup.procs");
    int cgroup_controllers_fd =
        open_cgroup_file(path, "cgroup.controllers");
    int cgroup_subtree_control_fd =
        open_cgroup_file(path, "cgroup.subtree_control");
    int cgroup_events_fd = open_cgroup_file(path, "cgroup.events");
    int cgroup_max_descendants_fd =
        open_cgroup_file(path, "cgroup.max.descendants");
    int cgroup_max_depth_fd = open_cgroup_file(path, "cgroup.max.depth");
    int cgroup_stat_fd = open_cgroup_file(path, "cgroup.stat");

    printf(1,
           "-----------------------------------------\nReading contents "
           "of \"%s\":\n-----------------------------------------\n",
           path);

	empty_string(buf);
    read(cgroup_procs_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.procs:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_controllers_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.controllers:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_subtree_control_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.subtree_control:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_events_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.events:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_max_descendants_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.max.descendants:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_max_depth_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.max.depth:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_stat_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.stat:\n%s\n", path, buf);

    close(cgroup_procs_fd);
    close(cgroup_controllers_fd);
    close(cgroup_subtree_control_fd);
    close(cgroup_events_fd);
    close(cgroup_max_descendants_fd);
    close(cgroup_max_depth_fd);
    close(cgroup_stat_fd);
}

static int itoa(char * buf, int n)
{
    int i = n;
    int length = 0;

    while (i > 0) {
        length++;
        i /= 10;
    }

    char revbuf[length];

    if (n == 0) {
        *buf++ = '0';
    }
    for (i = 0; n > 0 && i < length; i++) {
        revbuf[i] = (n % 10) + '0';
        n /= 10;
    }
    while (--i >= 0) {
        *buf++ = revbuf[i];
    }
    *buf = '\0';
    return length;
}

/**
 * Open, write, print, and close all files in cgroup located at path. move
 * current process to cgroup path, anable the cpu controller and print
 * contents. Then move the process back to root.
 */
void test_writing_cgroup_files(const char * path)
{
    char buf[256];
    int cgroup_procs_fd = open_cgroup_file(path, "cgroup.procs");
    int cgroup_controllers_fd =
        open_cgroup_file(path, "cgroup.controllers");
    int cgroup_subtree_control_fd =
        open_cgroup_file(path, "cgroup.subtree_control");
    int cgroup_events_fd = open_cgroup_file(path, "cgroup.events");
    int cgroup_max_descendants_fd =
        open_cgroup_file(path, "cgroup.max.descendants");
    int cgroup_max_depth_fd = open_cgroup_file(path, "cgroup.max.depth");
    int cgroup_stat_fd = open_cgroup_file(path, "cgroup.stat");

    printf(1,
           "-----------------------------------------\nWriting contents "
           "of \"%s\":\n-----------------------------------------\n",
           path);

    strcpy(buf, "+cpu -cpu +cpu");
    write(cgroup_subtree_control_fd, buf, sizeof(buf));
    int curpid = getpid();
    itoa(buf, curpid);
    write(cgroup_procs_fd, buf, sizeof(buf));
    strcpy(buf, "30");
    write(cgroup_max_descendants_fd, buf, sizeof(buf));
    strcpy(buf, "20");
    write(cgroup_max_depth_fd, buf, sizeof(buf));

    printf(1,
           "-----------------------------------------\nReading contents "
           "of \"%s\":\n-----------------------------------------\n",
           path);
    empty_string(buf);
    read(cgroup_procs_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.procs:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_controllers_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.controllers:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_subtree_control_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.subtree_control:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_events_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.events:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_max_descendants_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.max.descendants:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_max_depth_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.max.depth:\n%s\n", path, buf);
    empty_string(buf);
    read(cgroup_stat_fd, buf, sizeof(buf));
    printf(1, "Contents of %s/cgroup.stat:\n%s\n", path, buf);

    close(cgroup_procs_fd);
    close(cgroup_controllers_fd);
    close(cgroup_subtree_control_fd);
    close(cgroup_events_fd);
    close(cgroup_max_descendants_fd);
    close(cgroup_max_depth_fd);
    close(cgroup_stat_fd);

    test_reading_cgroup_files("/cgroup");
    test_reading_cgroup_files("/cgroup/test1");

    // return process to root cgroup
    printf(
        1,
        "-----------------------------------------\nReturning process #%d "
        "to root cgroup:\n-----------------------------------------\n",
        curpid);
    cgroup_procs_fd = open_cgroup_file("/cgroup", "cgroup.procs");
    itoa(buf, curpid);
    write(cgroup_procs_fd, buf, sizeof(buf));
    close(cgroup_procs_fd);

    test_reading_cgroup_files("/cgroup");
    test_reading_cgroup_files("/cgroup/test1");
}

void enable_cpu_controller(const char * path)
{
    char buf[256];
    int cgroup_controllers_fd =
        open_cgroup_file(path, "cgroup.controllers");
    int cgroup_subtree_control_fd =
        open_cgroup_file(path, "cgroup.subtree_control");

    read(cgroup_controllers_fd, buf, sizeof(buf));
    if (strcmp(buf, "cpu") == 0) {
        printf(1,
               "-----------------------------------------\nEnabling cpu "
               "controller at "
               "\"%s\":\n-----------------------------------------\n",
               path);
        strcpy(buf, "+cpu");
        write(cgroup_subtree_control_fd, buf, sizeof(buf));
    }
    close(cgroup_subtree_control_fd);
}

void disable_cpu_controller(const char * path)
{
    char buf[256];
    int cgroup_subtree_control_fd =
        open_cgroup_file(path, "cgroup.subtree_control");

    read(cgroup_subtree_control_fd, buf, sizeof(buf));
    if (strcmp(buf, "cpu") == 0) {
        printf(1,
               "-----------------------------------------\nDisabling cpu "
               "controller at "
               "\"%s\":\n-----------------------------------------\n",
               path);
        strcpy(buf, "-cpu");
        write(cgroup_subtree_control_fd, buf, sizeof(buf));
    }

    close(cgroup_subtree_control_fd);
}

void test_closing_cgroup_files(const char * path,
                               int * cgroup_procs_fd,
                               int * cgroup_controllers_fd,
                               int * cgroup_subtree_control_fd,
                               int * cgroup_events_fd,
                               int * cgroup_max_descendants_fd,
                               int * cgroup_max_depth_fd,
                               int * cgroup_stat_fd)
{
    close(*cgroup_procs_fd);
    close(*cgroup_controllers_fd);
    close(*cgroup_subtree_control_fd);
    close(*cgroup_events_fd);
    close(*cgroup_max_descendants_fd);
    close(*cgroup_max_depth_fd);
    close(*cgroup_stat_fd);
}

/**
 * Delete the cgroups at the following paths:
 * 1)	"/cgroup/test1"
 * 2)	"/cgroup/test2"
 * 3)	"/cgroup/test1/test1.1"
 * 4) 	"/cgroup/test1/test1.2"
 * Unmount the cgroup filesystem. (/cgroup)
 * Remove directory for the cgroup filesystem. (/cgroup)
 */
void test_deleting_cgroups()
{
    delete_cgroup_directory("/cgroup/test1/test1.1");
    delete_cgroup_directory("/cgroup/test1/test1.2");
    delete_cgroup_directory("/cgroup/test1");
    delete_cgroup_directory("/cgroup/test2");
    unmount_cgroup_fs("/cgroup");
    unlink("/cgroup");
}

int main(int argc, char * argv[])
{
    test_creating_cgroups();

    test_reading_cgroup_files("/cgroup");
    test_reading_cgroup_files("/cgroup/test1");
    test_reading_cgroup_files("/cgroup/test2");
    enable_cpu_controller("/cgroup/test1");

    test_reading_cgroup_files("/cgroup/test1/test1.1");
    test_writing_cgroup_files("/cgroup/test1/test1.1");

    disable_cpu_controller("/cgroup/test1/test1.1");
    test_reading_cgroup_files("/cgroup/test1/test1.1");

    test_deleting_cgroups();

    exit(0);
}
