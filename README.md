# Enhanced-xv6

## Steps to run

+ run `make qemu-nox SCHEDULER=SC CPUS=N`  to run the xv6 in terminal qemu. Replace SC with one of `RR`, `FCFS`, `PBS`, `MLFQ` to select appopiate scheduler. Change `N` to number of virtual CPUS required.
+ If command line arguments areto be changed after last run then run command `make clean`.

## Changes made to Original xv6

### waitx syscall

> `int waitx(int* wtime, int* rtime)`

Just like orignal `wait` syscall it waits for any child process to finish and return it's pid. The total wait time(time spend as runnable but could'nt run) and total run time(time spend as running) are stored in `*wtime` and `*rtime` respectively.

### ps (user program)

This user program will print details about all valid processes in the system.

### Custom Schedulers

+ #### First come - First Served (FCFS)

    It has a non-preemptive policy that selects the process with the lowest creation time. The process runs until it no longer needs CPU time.

    pseudo code:

    ```c
    int min_ctime=0;
    p = 0;
    for (int i=0; i<NPROC; i++) {
        if (ptable.proc[i].state != RUNNABLE)
            continue;
        if (ptable.proc[i].ctime < min_ctime || !p) {
            min_ctime = ptable.proc[i].ctime;
            p = ptable.proc + i;
      }
    }
    //p will be point to selected proc if there any
    ```

+ #### Priority Based Scheduler (PBS)

    A priority-based scheduler selects the process with the highest priority(lowest priority number) for execution. Priority is in range $[0,100]$. The following syscall can be used to set priority of a process:
    >`int set_priority(int new_priority, int pid)`  

    It is suitable to have higher priority of I/O bound processes.

    pseudo code:

    ```c
    int high_pty = 101;
      p = 0;
      for (int i = 0; i < NPROC; i++) {
          if (ptable.proc[i].state != RUNNABLE)
              continue;
          if (!p || ptable.proc[i].priority < high_pty ||
              (ptable.proc[i].priority == high_pty && ptable.proc[i].rn_cnt < p->rn_cnt)) {
              high_pty = ptable.proc[i].priority;
              p = ptable.proc + i;
          }
      }
    //p will be point to selected proc if there any
    ```

+ #### Multi-level Feedback queue scheduling

    MLFQ scheduler allows processes to move between different priority queues based
on their behavior and CPU bursts. If a process uses too much CPU time, it is pushed
to a lower priority queue, leaving I/O bound and interactive processes for higher
priority queues. Also, to prevent starvation, it implements aging, by pushing up a process in queue level. It has 5 queues with time slices as 1,2,4,8,16 ticks.

    pseudo code:

```c
for(int i=0; i<QCNT; i++){
      p = frontq(i);
      if(p){
        remq(i, p);
        if( p->state == RUNNABLE){
          selcp =p;
          break;
        } else i--;
      }
    }
//selcp will be point to selected proc if there any
if(selcp){
      selcp->rn_cnt++;
      selcp->curr_wtime=0;
      selcp->curr_rtime=1;
      selcp->ticks_inq[selcp->curr_q]++;
      c->proc = selcp;
      //process runs for time slot

      c->proc = 0;

      if(selcp && selcp->state == RUNNABLE){
          //this part is for demoting process in case found CPU bound
          if(selcp->used_limit && selcp->curr_q < QCNT-1){
            selcp->used_limit=0;
            selcp->curr_q++;
            selcp->curr_wtime=0;
          }
          selcp->curr_rtime=0;
          pushq(selcp->curr_q, selcp);
        }
}


      // apart from this processes are aged (if found starving) at every interrupt which increases ticks counter
```

> There is an accompanying report file comaparing different scheduling processes.
