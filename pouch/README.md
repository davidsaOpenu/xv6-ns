# Pouch start
Pouch start program starts a container.

## Usage:
pouch_start <name> [--tty tty<0-2>] <--demonize | &>

  name: name of container to create.
  --tty tty<0-3> select tty number for container manualy.
  --demonize: run conatiner under init.
  &: run container in background.

**NOTE: if demonize is not provided starting a container must be done in the background.**


## Examples
```
pouch_start my_cont &
pouch_start my_cont --tty tty0 --demonize
pouch_start my_cont --tty tty2 &
```

# Container operation 
Conatiner is a shell running under inside a new pid and mount namespace.
conatiner shell will be connected to tty.
Container will also create a file (cont_<name>) and write down some of the container params used for connecting tty and cgroups.

## Disconnecting:
run in container shell 
```
disconnect
```
Command will disconnect the tty and reconect the console.

## Connecting
Can be done in two ways.
1) 2DO should be done by the pouch_attach <name> command
2) Read the <name>_cont file TTY number.
   run ```connect ttyX>``` where X is the tty number.


# Pouch attach 
TBD

# Pouch distroy 
TBD


# What is a container?

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


