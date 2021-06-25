#include "cpu_account.h"
#include "steady_clock.h"
#include "proc.h"
#include "cgfs.h"
#include "namespace.h"

void cpu_account_initialize(struct cpu_account * cpu)
{
    cpu->cgroup = 0;
    cpu->cpu_account_frame = 0;
    cpu->cpu_account_period = 1 * 100 * 1000; // 100ms
    cpu->now = 0;
    cpu->process_cpu_time = 0;
    cpu->curr_frame_toal_process_cpu_time = 1;
}

void cpu_account_schedule_start(struct cpu_account * cpu)
{
    cpu->now = steady_clock_now();
}

void cpu_account_schedule_proc_update(struct cpu_account * cpu, struct proc * p)
{
    // If cpu accounting frame has passed, update CPU accounting.
    if (cpu->cpu_account_frame > p->cpu_account_frame) {
        unsigned int current_cpu_time =
            p->cpu_period_time > cpu->cpu_account_period
                ? cpu->cpu_account_period
                : p->cpu_period_time;
        p->cpu_percent = current_cpu_time * 100 / cpu->cpu_account_period;
        p->cpu_account_frame = cpu->cpu_account_frame;
        p->cpu_period_time -= current_cpu_time;
        uartputc('s');
        uartputc('u');
        uartputc('b');
        uartputc('\n');
    }
}

int cpu_account_schedule_process_decision(struct cpu_account * cpu,
                                          struct proc * p)
{
    // The cpu account frame according to the cgroup account period.
    unsigned int cgroup_cpu_account_frame;

    // Whether to schedule or not.
    char schedule = 1;

    // Set the current cgroup.
    struct cgroup * cgroup = p->cgroup;
    cpu->cgroup = cgroup;

    // Lock cgroup table.
    cgroup_lock();

    // Update the cpu cgroup values of all the cgroup ansestors of this process and throttle the process if needed
    while (cgroup) {
        // The cgroup cpu account frame.
        cgroup_cpu_account_frame =
            cpu->now / cgroup->cpu_account_period;

        // If cgroup cpu accounting frame is over, start a new one.
        if (cgroup_cpu_account_frame > cgroup->cpu_account_frame) {
            unsigned int current_cpu_time =
                cgroup->cpu_period_time > cgroup->cpu_account_period
                    ? cgroup->cpu_account_period
                    : cgroup->cpu_period_time;
            if (cgroup->cpu_controller_enabled) {
                ++cgroup->cpu_nr_periods;
            }
            cgroup->cpu_is_throttled_period = 0;
            cgroup->cpu_percent =
                current_cpu_time * 100 / cgroup->cpu_account_period;
            cgroup->cpu_account_frame = cgroup_cpu_account_frame;
            cgroup->cpu_period_time -= current_cpu_time;
        }

        // If cpu time is larger than cpu time limit, skip this process.
        if (cgroup->cpu_controller_enabled &&
            cgroup->cpu_period_time > cgroup->cpu_time_limit) {
            // Increase throttled number if not yet done.
            if (!cgroup->cpu_is_throttled_period) {
                ++cgroup->cpu_nr_throttled;
                cgroup->cpu_throttled_usec +=
                    cgroup->cpu_account_period - cgroup->cpu_period_time;
                cgroup->cpu_is_throttled_period = 1;
            }

            // Do not schedule.
            schedule = 0;

            // Advance to parent and continue.
            cgroup = cgroup->parent;
            continue;
        }

        // Advance to parent and continue.
        cgroup = cgroup->parent;
    }
    if (schedule) {
        schedule = cpu_account_schedule_process_decision_by_weights(cpu, p);
    }
    // Unlock the cgroup lock and return the result.
    cgroup_unlock();
    return schedule;
}

int cpu_account_schedule_process_decision_by_weights(struct cpu_account * cpu, struct proc * p) {
    unsigned int expected_cpu_precent = 100;
    struct cgroup *parent_cgroup = p->cgroup;
    int debug = 1;
    if (debug) {
        cprintf("in cpu_account_schedule_process_decision_by_weight");
        cprintf("\n");
        cprintf("%d --cpu->curr_frame_toal_process_cpu_time", cpu->curr_frame_toal_process_cpu_time);
        cprintf(" ");
        cprintf("%d -p->cpu_time",p->cpu_time);
        cprintf(" ");
        cprintf("%d --p->cpu_percent %d \n",p->cpu_percent, p->cpu_period_time * 100 / cpu->curr_frame_toal_process_cpu_time);
        cprintf("%d --p->cpu_period_time ",p->cpu_period_time);
        cprintf(" ");
        cprintf("%d --p->cpu_account_frame",p->cpu_account_frame);
        cprintf(" ");
        cprintf("%d -cpu", cpu->cpu_id);
        cprintf(" ");
        cprintf("%d -pid\n", get_pid_for_ns(p, p->nsproxy->pid_ns));
    }
    
    int my_weight = DEFAULT_CGROUP_CPU_WEIGHT;
    while (parent_cgroup != 0) { // loop until the very first genaration
        int total_generation_weight = unsafe_get_sum_children_weights(parent_cgroup);
        expected_cpu_precent =  expected_cpu_precent * my_weight / total_generation_weight;

        if (debug) {
            cprintf("%d -expected_cpu_precen",expected_cpu_precent);
            cprintf(" ");
            cprintf("%d -my_weigh",my_weight);
            cprintf(" ");
            cprintf("%d -total_generation_weigh",total_generation_weight);
            cprintf("\n");
        }
        my_weight = parent_cgroup->cpu_weight;
        parent_cgroup = parent_cgroup->parent;
    }
        
    
    //cprintf("cpu_account_schedule_process_decision_by_weights - p->cpu_percent: %d, expected_cpu_precent: %d\n", p->cpu_percent, expected_cpu_precent);
    if (expected_cpu_precent < p->cpu_period_time * 100 / cpu->curr_frame_toal_process_cpu_time) {
        if (debug) {
            cprintf("returning 0");
            cprintf("\n");
        }
        return 0;
    }
    if (debug) {
        cprintf("returning 1");
        cprintf("\n");
    }
    return 1;
}

void cpu_account_before_process_schedule(struct cpu_account * cpu,
                                         struct proc * proc)
{
    // Update process cpu time.
    cpu->process_cpu_time = steady_clock_now();
}

void cpu_account_after_process_schedule(struct cpu_account * cpu,
                                        struct proc * p)
{
    //cprintf("in cpu_account_after_process_schedule\n");
    struct cgroup * cgroup = cpu->cgroup;

    // Update now.
    cpu->now = steady_clock_now();
    if (cpu->now / cpu->cpu_account_period > cpu->cpu_account_frame) {
        cpu->curr_frame_toal_process_cpu_time = 1;
    }
    cpu->cpu_account_frame = cpu->now / cpu->cpu_account_period;

    // Update process cpu time.
    cpu->process_cpu_time = cpu->now - cpu->process_cpu_time;
    cpu->curr_frame_toal_process_cpu_time += cpu->process_cpu_time;

    

    // Update process cpu time.
    p->cpu_time += cpu->process_cpu_time;
    p->cpu_period_time += cpu->process_cpu_time;

    // Lock the cgroup lock.
    cgroup_lock();

    // Update cgroup cpu time.
    while (cgroup) {
        // Update cgroup cpu time.
        cgroup->cpu_time += cpu->process_cpu_time;
        cgroup->cpu_period_time += cpu->process_cpu_time;

        // If cgroup cpu controller is not enabled.
        if (!cgroup->cpu_controller_enabled) {
            // Advance to parent and continue.
            cgroup = cgroup->parent;
            continue;
        }

        // Advance to parent.
        cgroup = cgroup->parent;
    }

    // Unlock the cgroup lock.
    cgroup_unlock();
}

void cpu_account_schedule_finish(struct cpu_account * cpu)
{
}

void cpu_account_before_hlt(struct cpu_account * cpu)
{
}

void cpu_account_after_hlt(struct cpu_account * cpu)
{
    // Update now.
    cpu->now = steady_clock_now();
    if (cpu->now / cpu->cpu_account_period > cpu->cpu_account_frame) {
        cpu->curr_frame_toal_process_cpu_time = 1;
    }
    cpu->cpu_account_frame = cpu->now / cpu->cpu_account_period;
}
