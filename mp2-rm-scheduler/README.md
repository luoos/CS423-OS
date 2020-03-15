# MP2: Rate Monotonic Scheduler

This MP implements a rate monotonic scheduler, in which each task has a fix `period` and a fix `CPU time` in each period. The `CPU time` should be got in advance. In `userapp.c`, this computation is to compute factorial.

## Usage

First `make`. A linux module will be generated - `mp2.ko`. Then insert this module by `sudo insmod mp2.ko`. With this module inserted, a proc file (`/proc/mp2/status`) will be created as the interface of this scheduler. A user process needs to read or write to this proc file to interact with the scheduler.

There are four different actions supported by this scheduler:

1. `Register`: let the scheduler know the writing process needs to be scheduled.
2. `Yield`: Voluntarily relinquish the CPU. Sleep till the next period.
3. `Deregister`: the process is exiting, tell the scheduler to stop schedule.
4. `Query`: Get a list of all tasks currently scheduled by the schduler.

`Register`, `Yield`, and `Deregister` are done by writing the proc file, while `Query` is done by reading.

Write format:
1. `Register`: `R,<pid>,<period (ms)>,<CPU time (ms)>`
2. `Yield`: `Y,<pid>`
3. `Deregister`: `D,<pid>`

Read content interpretation:
`<pid>,<period (ms)>,<CPU time (ms)>,<state>`

There are three states:

```
0 - Sleeping
1 - Ready
2 - Running
```

## Design Decisions

### Timer

We need a timer to wakeup dispatcher thread. Each process resets the wakeup timer in each `yield`. When the process calls the `yield`, this process already finished the computation and is going to sleep till the next period. The timer expires at the beginning of the next period, when the state of a task is set to `Ready` and the dispatcher thread is woken up.

### Scheduling Policy

When the dispatcher thread is woken up, it tries to get a `Ready` task with a minimal period. Compared with the running task, the ready task will preempt the running task if the period of the ready task is shorter than the running task
