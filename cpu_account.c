#include "cpu_account.h"
#include "steady_clock.h"
#include "proc.h"
#include "cgfs.h"
#include "namespace.h"
#include "spinlock.h"

struct spinlock toal_process_cpu_time_lock;
static unsigned int curr_frame_toal_process_and_cpu_time = 1;

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
    initlock(&toal_process_cpu_time_lock, "toal_process_cpu_time_lock");
    cpu->now = steady_clock_now();
}

void cpu_account_schedule_proc_update(struct cpu_account * cpu, struct proc * p)
{
    // If cpu accounting frame has passed, update CPU accounting.
    if (cpu->cpu_account_frame > p->cpu_account_frame) {
        unsigned int current_cpu_time = min(p->cpu_period_time, cpu->cpu_account_period);
        p->cpu_percent = current_cpu_time * 100 / cpu->cpu_account_period;
        p->cpu_account_frame = cpu->cpu_account_frame;
        p->cpu_period_time -= current_cpu_time;
        //cprintf("sub id: %d, cpu->cpu_account_frame: %d\n", cpu->cpu_id, cpu->cpu_account_frame);
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
    struct cgroup *cgroup = p->cgroup;
    acquire(&toal_process_cpu_time_lock);
    //cprintf("###namespace-pid=%d\n", get_pid_for_ns(p, p->nsproxy->pid_ns));
    //cprintf("###in cpu_account_schedule_process_decision_by_weight\n");
    int debug = 0 && get_pid_for_ns(p, p->nsproxy->pid_ns) != 1 && get_pid_for_ns(p, p->nsproxy->pid_ns) != 2 && cpu->cpu_account_frame %10 == 0;
    if (debug) {
        //cprintf("###in cpu_account_schedule_process_decision_by_weight\n");
        //cprintf("%d --cpu->curr_frame_toal_process_cpu_time", cpu->curr_frame_toal_process_cpu_time);
        cprintf("###curr_frame_toal_process_and_cpu_time=%d\n", curr_frame_toal_process_and_cpu_time);
        //cprintf("###p->cpu_time=%d\n", p->cpu_time);
        //cprintf("%d --p->cpu_percent %d \n",p->cpu_percent, p->cpu_period_time * 100 / cpu->curr_frame_toal_process_cpu_time);
        //cprintf("%d --p->cpu_percent %d \n",p->cpu_percent, p->cpu_period_time * 100 / curr_frame_toal_process_and_cpu_time); // Tmp
        cprintf("###p->cpu_percent=%d\n",p->cpu_percent);
        cprintf("###p->cpu_period_time * 100 / curr_frame_toal_process_and_cpu_time=%d\n", p->cpu_period_time * 100 / curr_frame_toal_process_and_cpu_time);
        cprintf("###p->cpu_period_time=%d\n", p->cpu_period_time);
        cprintf("###p->cpu_account_frame=%d\n",p->cpu_account_frame);;
        cprintf("###cpu->cpu_id=%d\n", cpu->cpu_id);
        cprintf("###namespace-pid=%d\n", get_pid_for_ns(p, p->nsproxy->pid_ns));
    }
    
    int my_weight = DEFAULT_CGROUP_CPU_WEIGHT;
    while (cgroup != 0) { // loop until the very first genaration
        int total_generation_weight = unsafe_get_sum_children_weights(cgroup);
        expected_cpu_precent =  expected_cpu_precent * my_weight / total_generation_weight;

        if (debug) {
            //cprintf("%s: ", cgroup->cgroup_dir_path);
            cprintf("expected_cpu_precent=%d\n", expected_cpu_precent);
            //cprintf("my_weigh=%d, ",my_weight);
            //cprintf("total_generation_weight=%d\n", total_generation_weight);
        }
        my_weight = cgroup->cpu_weight;
        cgroup = cgroup->parent;
    }
        
    
    //cprintf("cpu_account_schedule_process_decision_by_weights - p->cpu_percent: %d, expected_cpu_precent: %d\n", p->cpu_percent, expected_cpu_precent);
    //if (expected_cpu_precent < p->cpu_period_time * 100 / cpu->curr_frame_toal_process_cpu_time) {
    if (expected_cpu_precent < p->cpu_period_time * 100 / curr_frame_toal_process_and_cpu_time) {
        // Current cpu precent has passed the expected precent from its weight
        if (debug) {
            cprintf("returning 0\n");
        }
        release(&toal_process_cpu_time_lock);
        return 0;
    }
    if (debug) {
        cprintf("returning 1\n");
    }
    release(&toal_process_cpu_time_lock);
    return 1;
}

void cpu_account_before_process_schedule(struct cpu_account * cpu,
                                         struct proc * p)
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
    acquire(&toal_process_cpu_time_lock);
    if (cpu->now / cpu->cpu_account_period > cpu->cpu_account_frame) {
        // New frame
        cpu->curr_frame_toal_process_cpu_time = 1;
        curr_frame_toal_process_and_cpu_time = 1;
        //cprintf("cpu %d ", cpu->cpu_id);
        //cprintf("going to new frame %d \n", cpu->now / cpu->cpu_account_period);
    }
    cpu->cpu_account_frame = cpu->now / cpu->cpu_account_period;

    // Update "per cpu" cpu time.
    cpu->process_cpu_time = cpu->now - cpu->process_cpu_time;
    cpu->curr_frame_toal_process_cpu_time += cpu->process_cpu_time;
    curr_frame_toal_process_and_cpu_time += cpu->process_cpu_time;
    
    release(&toal_process_cpu_time_lock);
    

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
        acquire(&toal_process_cpu_time_lock);
        curr_frame_toal_process_and_cpu_time = 1;
        release(&toal_process_cpu_time_lock);
        //cprintf("after halt cpu %d going to new frame %d\n", cpu->cpu_id, cpu->now / cpu->cpu_account_period);
    }
    cpu->cpu_account_frame = cpu->now / cpu->cpu_account_period;
}
