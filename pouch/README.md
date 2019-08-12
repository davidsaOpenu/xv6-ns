
Pouch is a container program.
* Containers use namespaces and cgroups supported by the kernel to isoliate a executable.

* cgroups is a kernel feature that limits, accounts for, and isolates the resource usage (CPU, memory, disk I/O, network, etc.) of a collection of processes. 

* Mount namespaces provide isolation of the list of mount points seen
       by the processes in each namespace instance.  Thus, the processes in
       each of the mount namespace instances will see distinct single-
       directory hierarchie

* PID namespaces isolate the process ID number space, meaning that
       processes in different PID namespaces can have the same PID.  PID
       namespaces allow containers to provide functionality such as
       suspending/resuming the set of processes in the container and
       migrating the container to a new host while the processes inside the
       container maintain the same PIDs.



Usage:
pouch_start [--cgroup <max_microseconds> <period_durtion>] [--pid_ns] [--mount_ns <mount_point>]
