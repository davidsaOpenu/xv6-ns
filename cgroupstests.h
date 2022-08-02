#ifndef CGROUPSTESTS_H
#define CGROUPSTESTS_H

#define MAX_CONTROLLER_NAME_LENGTH      16
#define CONTROLLER_COUNT                4

enum controller_types { CPU_CNT, PID_CNT, SET_CNT, MEM_CNT };

// For memory controler test
#define KERNBASE "2147483648" // KERNBASE defined in memlayout.h as 0x80000000 == 2147483648
#define MORE_THEN_KERNBASE "2147483649"
// This is the minimum memory size as initialized in mein.c line 43 kinit2, from 4 * 1024 * 1024 to PHYSTOP.
// [PHYSTOP == 0xE000000 as defined in memlayout.h], 230686720 == 0xE000000 - (4 * 1024 * 1024)
#define MEM_SIZE 230686720 

#define CGROUP                          "cgroup"
#define ROOT_CGROUP                     "/cgroup"
#define TEST_1                          "/cgroup/test1"
#define TEST_2                          "/cgroup/test2"
#define TEST_TMP                        "/cgroup/testtmp"
#define TEST_1_1                        "/cgroup/test1.1"
#define TEST_1_2                        "/cgroup/test1.2"

#define TEST_1_CGROUP_PROCS             "/cgroup/test1/cgroup.procs"
#define TEST_1_CGROUP_CONTROLLERS       "/cgroup/test1/cgroup.controllers"
#define TEST_1_CGROUP_SUBTREE_CONTROL   "/cgroup/test1/cgroup.subtree_control"
#define TEST_1_CGROUP_EVENTS            "/cgroup/test1/cgroup.events"
#define TEST_1_CGROUP_DESCENDANTS       "/cgroup/test1/cgroup.max.descendants"
#define TEST_1_CGROUP_MAX_DEPTH         "/cgroup/test1/cgroup.max.depth"
#define TEST_1_CGROUP_STAT              "/cgroup/test1/cgroup.stat"
#define TEST_1_CPU_MAX                  "/cgroup/test1/cpu.max"
#define TEST_1_CPU_WEIGHT               "/cgroup/test1/cpu.weight"
#define TEST_1_CPU_STAT                 "/cgroup/test1/cpu.stat"
#define TEST_1_PID_MAX                  "/cgroup/test1/pid.max"
#define TEST_1_PID_CURRENT              "/cgroup/test1/pid.current"
#define TEST_1_SET_CPU                  "/cgroup/test1/cpuset.cpus"
#define TEST_1_SET_FRZ                  "/cgroup/test1/cgroup.freeze"
#define TEST_1_MEM_CURRENT              "/cgroup/test1/memory.current"
#define TEST_1_MEM_MAX                  "/cgroup/test1/memory.max"
#define TEST_1_MEM_MIN                  "/cgroup/test1/memory.min"
#define TEST_1_MEM_STAT                 "/cgroup/test1/memory.stat"

#define TEST_2_CGROUP_SUBTREE_CONTROL   "/cgroup/test2/cgroup.subtree_control"
#define TEST_2_MEM_MIN                  "/cgroup/test2/memory.min"
#define ROOT_CGROUP_PROCS               "/cgroup/cgroup.procs"

#define TEST_TMP_CGROUP_SUBTREE_CONTROL   "/cgroup/testtmp/cgroup.subtree_control"
#define TEST_TMP_MEM_MIN                  "/cgroup/testtmp/memory.min"

#define TEMP_FILE "temp_file"

#define TESTED_NESTED_CGROUP_CHILD          ("/nested")
#define TEST_NESTED_MEM_MAX                 ("/memory.max")
#define TEST_NESTED_MEM_MIN                 ("/memory.min")
#define TEST_NESTED_SUBTREE_CONTROL         ("/cgroup.subtree_control")
#define TEST_NESTED_CURRENT_MEM             ("/memory.current")
#endif
