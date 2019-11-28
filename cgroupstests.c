#include "fcntl.h"
#include "types.h"
#include "user.h"

void empty_string(char * string, int length)
{
    for (int i = 0; i < length; i++)
		string[i] = 0;
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

void test_mount_cgroup_fs()
{
	printf(1, "Testing mounting cgroup filesystem: ");
	
	if(mkdir("/cgroup") != 0 || mount(0, "/cgroup", "cgroup") != 0)
		printf(1, "FAIL.\n");
	else 
		printf(1, "OK.\n");
}

void test_umount_cgroup_fs()
{
	printf(1, "Testing unmounting cgroup filesystem: ");
	
	if(umount("/cgroup") != 0 || unlink("/cgroup") != 0)
		printf(1, "FAIL.\n");
	else 
		printf(1, "OK.\n");
}

void test_creating_cgroups()
{
	printf(1, "Testing creating cgroups: ");
	
	if(mkdir("/cgroup/test1") != 0 || mkdir("/cgroup/test2") != 0 || mkdir("/cgroup/test1/test1.1") != 0 || mkdir("/cgroup/test1/test1.2") != 0)
		printf(1, "FAIL.\n");
	else
		printf(1, "OK.\n");
}

void test_deleting_cgroups()
{
	printf(1, "Testing deleting cgroups: ");
	
	if(unlink("/cgroup/test1/test1.2") != 0 || unlink("/cgroup/test1/test1.1") != 0 || unlink("/cgroup/test2") != 0 || unlink("/cgroup/test1") != 0)
		printf(1, "FAIL.\n");
	else
		printf(1, "OK.\n");
}

void test_opening_and_closing_cgroup_files()
{
    printf(1, "Testing opening cgroup files: ");
	
	int cgroup_procs_fd = open("/cgroup/test1/cgroup.procs", O_RDWR);
    int cgroup_controllers_fd = open("/cgroup/test1/cgroup.controllers", O_RDWR);
    int cgroup_subtree_control_fd = open("/cgroup/test1/cgroup.subtree_control", O_RDWR);
    int cgroup_events_fd = open("/cgroup/test1/cgroup.events", O_RDWR);
    int cgroup_max_descendants_fd = open("/cgroup/test1/cgroup.max.descendants", O_RDWR);
    int cgroup_max_depth_fd = open("/cgroup/test1/cgroup.max.depth", O_RDWR);
    int cgroup_stat_fd = open("/cgroup/test1/cgroup.stat", O_RDWR);
	
	if (cgroup_procs_fd < 0 ||
        cgroup_controllers_fd < 0 ||
        cgroup_subtree_control_fd < 0 ||
	    cgroup_events_fd < 0 ||
	    cgroup_max_descendants_fd < 0 ||
	    cgroup_max_depth_fd < 0 ||
	    cgroup_stat_fd < 0)
	    printf(1, "FAIL.\n");
	else
		printf(1, "OK.\n");
	
	printf(1, "Testing closing cgroup files: ");
	
	if (close(cgroup_procs_fd) != 0 ||
        close(cgroup_controllers_fd) != 0 ||
        close(cgroup_subtree_control_fd) != 0 ||
		close(cgroup_events_fd) != 0 ||
		close(cgroup_max_descendants_fd) != 0 ||
		close(cgroup_max_depth_fd) != 0 ||
		close(cgroup_stat_fd) != 0)
		printf(1, "FAIL.\n");
	else
		printf(1, "OK.\n");
}

void test_reading_cgroup_files()
{
	printf(1, "Testing reading cgroup files: ");
	
	char buf[256];
	char result = 1;
    int cgroup_procs_fd = open("/cgroup/test1/cgroup.procs", O_RDWR);
    int cgroup_controllers_fd = open("/cgroup/test1/cgroup.controllers", O_RDWR);
    int cgroup_subtree_control_fd = open("/cgroup/test1/cgroup.subtree_control", O_RDWR);
    int cgroup_events_fd = open("/cgroup/test1/cgroup.events", O_RDWR);
    int cgroup_max_descendants_fd = open("/cgroup/test1/cgroup.max.descendants", O_RDWR);
    int cgroup_max_depth_fd = open("/cgroup/test1/cgroup.max.depth", O_RDWR);
    int cgroup_stat_fd = open("/cgroup/test1/cgroup.stat", O_RDWR);

	result &= cgroup_procs_fd >= 0 &&
              cgroup_controllers_fd >= 0 &&
              cgroup_subtree_control_fd >= 0 &&
	          cgroup_events_fd >= 0 &&
	          cgroup_max_descendants_fd >= 0 &&
	          cgroup_max_depth_fd >= 0 &&
	          cgroup_stat_fd >= 0;
			  
    printf(1,
           "\n-----------------------------------------\nReading contents "
           "of \"/cgroup/test1\":\n-----------------------------------------\n");

    empty_string(buf, sizeof(buf));
    result &= read(cgroup_procs_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.procs:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_controllers_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.controllers:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_subtree_control_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.subtree_control:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_events_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.events:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_max_descendants_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.max.descendants:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_max_depth_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.max.depth:\n%s\n", buf);
    empty_string(buf, sizeof(buf));
    result &= read(cgroup_stat_fd, buf, sizeof(buf)) >= 0;
    printf(1, "Contents of /cgroup/test1/cgroup.stat:\n%s\n", buf);

    result &= close(cgroup_procs_fd) == 0 &&
              close(cgroup_controllers_fd) == 0 &&
              close(cgroup_subtree_control_fd) == 0 &&
		      close(cgroup_events_fd) == 0 &&
		      close(cgroup_max_descendants_fd) == 0 &&
		      close(cgroup_max_depth_fd) == 0 &&
		      close(cgroup_stat_fd) == 0;
			  
    printf(1, "\nReading cgroup files test result: ");
			  
	if(result)
		printf(1, "OK.\n");
	else
		printf(1, "FAIL.\n");	
}

void test_moving_process()
{
	printf(1, "Testing moving processes between cgroups: ");
	
	char buf[256];
	char result = 1;
    int cgroup_procs_fd = open("/cgroup/test1/cgroup.procs", O_RDWR);

	result &= cgroup_procs_fd >= 0;
	
	// Move the current process to "/cgroup/test1" cgroup.
	int cur_pid = getpid();
	char cur_pid_buf[3];
    itoa(cur_pid_buf, cur_pid);
    result &= write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) > 0;
	
	// Check that the process we moved is really in "/cgroup/test1" cgroup.
	empty_string(buf, sizeof(buf));
    result &= read(cgroup_procs_fd, buf, sizeof(buf)) >= 0;
	if(cur_pid_buf[1] == 0)
		cur_pid_buf[1] = '\n';
	int i;
	for(i = 0; buf[i] != 0 && i < sizeof(buf) - 2; i++)
		if(cur_pid_buf[0] == buf[i] && cur_pid_buf[1] == buf[i+1])
		    break;
	if(i == sizeof(buf) - 2 || buf[i] == 0)
		result = 0;
	
	result &= close(cgroup_procs_fd) == 0;
			  
	// Return the process to root cgroup.	  
	cgroup_procs_fd = open("/cgroup/cgroup.procs", O_RDWR);
    itoa(cur_pid_buf, cur_pid);
    result &= write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) > 0;
    
	// Check that the process we returned is really in root cgroup.
	empty_string(buf, sizeof(buf));
    result &= read(cgroup_procs_fd, buf, sizeof(buf)) >= 0;
	if(cur_pid_buf[1] == 0)
		cur_pid_buf[1] = '\n';
	for(i = 0; buf[i] != 0 && i < sizeof(buf) - 2; i++)
		if(cur_pid_buf[0] == buf[i] && cur_pid_buf[1] == buf[i+1])
		    break;
	if(i == sizeof(buf) - 2 || buf[i] == 0)
		result = 0;
	
	result &= close(cgroup_procs_fd) == 0;
			  
	if(result)
		printf(1, "OK.\n");
	else
		printf(1, "FAIL.\n");
	
}

void test_enabling_and_disabling_cpu_controller()
{
	printf(1, "Testing enabling cpu controller: ");
	
	char buf[256];
	char result = 1;
	int cgroup_subtree_control_fd = open("/cgroup/test1/cgroup.subtree_control", O_RDWR);

	result &= cgroup_subtree_control_fd >= 0;
	
	// Enable cpu controller
	empty_string(buf, sizeof(buf));
	strcpy(buf, "+cpu -cpu +cpu");
    result &= write(cgroup_subtree_control_fd, buf, sizeof(buf)) > 0;
	
	// Check that cpu controller is really enabled
	empty_string(buf, sizeof(buf));
	result &= read(cgroup_subtree_control_fd, buf, sizeof(buf)) >= 0;
	int i;
	for(i = 0; buf[i] != 0 && i < sizeof(buf) - 2; i++)
		if(buf[i] == 'c' && buf[i+1] == 'p' && buf[i+2] == 'u')
		    break;
	if(i == sizeof(buf) - 2 || buf[i] == 0)
		result = 0;
	
	if(result)
		printf(1, "OK.\n");
	else
		printf(1, "FAIL.\n");
	
	printf(1, "Testing disabling cpu controller: ");
	
	// Disable cpu controller
	empty_string(buf, sizeof(buf));
	strcpy(buf, "+cpu -cpu");
    result &= write(cgroup_subtree_control_fd, buf, sizeof(buf)) > 0;
	
	// Check that cpu controller is really disabled
	empty_string(buf, sizeof(buf));
	result &= read(cgroup_subtree_control_fd, buf, sizeof(buf)) >= 0;
	for(i = 0; buf[i] != 0 && i < sizeof(buf) - 2; i++)
		if(buf[i] == 'c' && buf[i+1] == 'p' && buf[i+2] == 'u')
		    result = 0;
	
	result &= close(cgroup_subtree_control_fd) == 0;
			  
	if(result)
		printf(1, "OK.\n");
	else
		printf(1, "FAIL.\n");
}

void test_setting_max_descendants_and_max_depth()
{
	printf(1, "Testing setting max descendants allowed and max depth allowed values: ");
	
	char buf[256];
	char result = 1;
	int cgroup_max_descendants_fd = open("/cgroup/test1/cgroup.max.descendants", O_RDWR);
	int cgroup_max_depth_fd = open("/cgroup/test1/cgroup.max.depth", O_RDWR);

	result &= cgroup_max_descendants_fd >= 0 &&
	          cgroup_max_depth_fd >= 0;

	// Set new values for max descendants allowed and max depth allowed
	empty_string(buf, sizeof(buf));
    strcpy(buf, "30");
    result &= write(cgroup_max_descendants_fd, buf, sizeof(buf)) > 0;
	empty_string(buf, sizeof(buf));
    strcpy(buf, "20");
    result &= write(cgroup_max_depth_fd, buf, sizeof(buf)) > 0;

	// Check that the values have really been set
	empty_string(buf, sizeof(buf));
	result &= read(cgroup_max_descendants_fd, buf, sizeof(buf)) >= 0;
	if(buf[0] != '3' || buf[1] != '0')
		result = 0; 
	empty_string(buf, sizeof(buf));
	result &= read(cgroup_max_depth_fd, buf, sizeof(buf)) >= 0;
	if(buf[0] != '2' || buf[1] != '0')
		result = 0; 	
	
	result &= close(cgroup_max_descendants_fd) == 0 &&
	          close(cgroup_max_depth_fd) == 0;
	
	if(result)
		printf(1, "OK.\n");
	else
		printf(1, "FAIL.\n");
}

int main(int argc, char * argv[])
{
	test_mount_cgroup_fs();
	test_creating_cgroups();
	test_opening_and_closing_cgroup_files();
	test_reading_cgroup_files();
	test_moving_process();
	test_enabling_and_disabling_cpu_controller();
	test_setting_max_descendants_and_max_depth();	
	test_deleting_cgroups();
	test_umount_cgroup_fs();
	
    exit(0);
}
