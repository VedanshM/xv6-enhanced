#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pinfo.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->ctime = ticks;
  p->etime = -1;
  p->rtime = 0;
  p->rn_cnt = 0;
  p->tot_wtime=0;
  p->curr_wtime=0;
  p->curr_rtime=0;
  p->priority = 60;
  p->curr_q=0;
#if SCHEDULER != PBS_SCHED
  p->priority = -1;
#endif
#if SCHEDULER != MLFQ_SCHED
  p->curr_q = -1;
#endif
  for(int i=0;i<QCNT;i++){
    p->ticks_inq[i]=0;
#if SCHEDULER != MLFQ_SCHED
  	p->ticks_inq[i]=-1;
#endif
  }
  p->used_limit=0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
#if SCHEDULER == MLFQ_SCHED
  pushq(0, p);
  p->curr_q = 0;
#endif

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
#if SCHEDULER == MLFQ_SCHED
  pushq(0, np);
  np->curr_q = 0;
#endif
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Setting up end_time of the process
  curproc->etime = ticks;

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int waitx(int *wtime, int *rtime) {
	int havekids, pid;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->parent != curproc)
				continue;
			havekids = 1;
			if (p->state == ZOMBIE) {
				// Found one.
				pid = p->pid;
				*rtime = p->rtime;
				*wtime = p->tot_wtime;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || curproc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock); //DOC: wait-sleep
	}
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
#if SCHEDULER == RR_SCHED

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef DEBUG
	  cprintf("RR  cpu: %d pid: %d name: %s\n", c - cpus, p->pid, p->name);
#endif
	    c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->rn_cnt++;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
	}

#elif SCHEDULER == FCFS_SCHED

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
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

    if(p){
#ifdef DEBUG
		cprintf("FCFS  cpu: %d pid: %d name: %s\n", c - cpus, p->pid, p->name);
#endif

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->rn_cnt++;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
	  }
    release(&ptable.lock);
	}

#elif SCHEDULER == PBS_SCHED

  for (;;) {
	  // Enable interrupts on this processor.
	  sti();

	  // Loop over process table looking for process to run.
	  acquire(&ptable.lock);
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

	  if (p) {
#ifdef DEBUG
		  cprintf("PBS  cpu: %d pid: %d(%d) name: %s pty: %d \n",
				  c - cpus, p->pid, p->parent ? p->parent->pid : 0, p->name, p->priority);
#endif

		  p->rn_cnt++;
		  c->proc = p;
		  switchuvm(p);
		  p->state = RUNNING;
		  swtch(&(c->scheduler), p->context);
		  switchkvm();

		  // Process is done running for now.
		  // It should have changed its p->state before coming back.
		  c->proc = 0;
	  }
	  release(&ptable.lock);
  }

#elif SCHEDULER == MLFQ_SCHED
  for (;;) {
	  // Enable interrupts on this processor.
	  sti();
	  acquire(&ptable.lock);
	  struct proc *selcp = 0;

    //select from fronts and if not runnable rem from queue
    // queue should only store runnable procs
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
    // cprintf("SELECTED 0: %d\n", selcp ==0);
    if(selcp){
      selcp->rn_cnt++;
      selcp->curr_wtime=0;
      selcp->curr_rtime=1;
      selcp->ticks_inq[selcp->curr_q]++;
#ifdef LOGS
      cprintf("%d %d %d::=\n", ticks, selcp->pid, selcp->curr_q);
#endif
      c->proc = selcp;

      switchuvm(selcp);
      selcp->state = RUNNING;
      swtch(&(c->scheduler), selcp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its selcp->state before coming back.
      c->proc = 0;

      if(selcp && selcp->state == RUNNABLE){
          if(selcp->used_limit && selcp->curr_q < QCNT-1){
            selcp->used_limit=0;
            selcp->curr_q++;
            selcp->curr_wtime=0;
            #ifdef DEBUG
              cprintf("Proc: %s (%d) queue inc: %d\n",selcp->name, selcp->pid, selcp->curr_q);
            #endif
#ifdef LOGS
  cprintf("%d %d %d::=\n", ticks,selcp->pid, selcp->curr_q);
#endif
          }
          selcp->curr_rtime=0;
          pushq(selcp->curr_q, selcp);
        }
  	}
	release(&ptable.lock);
  }

#endif 
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
#if SCHEDULER == MLFQ_SCHED
	  pushq(p->curr_q, p);
#endif
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
#if SCHEDULER == MLFQ_SCHED
        pushq(0, p);
        p->curr_q=0;
#endif        
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void update_proctime(void) {
	acquire(&ptable.lock);
	for (int i = 0; i < NPROC; i++) {
		if (ptable.proc[i].state == RUNNING)
			ptable.proc[i].rtime++;
    else if(ptable.proc[i].state == RUNNABLE){
      ptable.proc[i].tot_wtime++;
      ptable.proc[i].curr_wtime++;
    }
	}
	release(&ptable.lock);
}

int set_prioritiy(int new_priority, int pid) {
	if (new_priority < 0 || new_priority > 100)
		return -1;
	struct proc *pc = 0;
	acquire(&ptable.lock);
	for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->pid == pid) {
			pc = p;
			break;
		}
	}
	release(&ptable.lock);
	if (!pc)
		return -1;
	int old_pty = pc->priority;
	pc->priority = new_priority;
	// if (old_pty != new_priority) {
		// yield();
	// }
	return old_pty;
}

void pushq(int qid, struct proc *p) {
	if (priorq[qid].size < 2 * NPROC) {
#ifdef DEBUG
  // cprintf("proc: %d added to q:%d\n", p->pid, qid);
#endif
		priorq[qid].p[priorq[qid].size++] = p;
	}
}

void remq(int qid, struct proc *p) {
	for (int i = 0; i < priorq[qid].size; i++)
		if (priorq[qid].p[i]->pid == p->pid) {
#ifdef DEBUG
			// cprintf("proc: %d removed to q:%d\n", p->pid, qid);
#endif
			for (int j = i + 1; j < priorq[qid].size; j++)
				priorq[qid].p[j - 1] = priorq[qid].p[j];
			priorq[qid].size--;
      priorq[qid].p[priorq[qid].size]=0;
			return;
		}
}

void age_procs() {
	for (int i = 1; i < QCNT; i++) {
		for (int j = 0; j < priorq[i].size; j++) {
			if (priorq[i].p + j && priorq[i].p[j]->state == RUNNABLE &&
				priorq[i].p[j]->curr_wtime > STARV_LIM) {
				struct proc *p = priorq[i].p[j];
				pushq(i - 1, p);
				remq(i, p);
				p->curr_q = i - 1;
        #ifdef DEBUG
          cprintf("proc: %s(%d) aged to q: %d\n", p->name, p->pid, p->curr_q);
        #endif
#ifdef LOGS
		  cprintf("%d %d %d::=\n", ticks,p->pid, p->curr_q);
#endif
			}
		}
	}
}

struct proc *frontq(int qid) {
	if (priorq[qid].size > 0) {
		return priorq[qid].p[0];
	}
	return 0;
}

int get_pinfos(struct pinfo *arg){
	int n = 0;
  
	acquire(&ptable.lock);
	for (struct proc *p = ptable.proc; p - ptable.proc < NPROC; p++) {
		switch (p->state) {
		case UNUSED:
		case EMBRYO:
			continue;
		case RUNNING:
			strncpy(arg[n].state, "running ", sizeof("running "));
			break;
		case RUNNABLE:
			strncpy(arg[n].state, "runnable", sizeof("runnable"));
			break;
		case SLEEPING:
			strncpy(arg[n].state, "sleeping", sizeof("sleeping"));
			break;
		case ZOMBIE:
			strncpy(arg[n].state, "zombie  ", sizeof("zombie  "));
		}
		arg[n].pid = p->pid;
		arg[n].n_run = p->rn_cnt;
		arg[n].rtime = p->rtime;
  #if SCHEDULER == PBS_SCHED
		arg[n].priority = p->priority;
  #else 
    arg[n].priority = -1;
  #endif

		arg[n].cur_q = p->curr_q;
		for (int i = 0; i < QCNT; i++) {
			arg[n].ticks[i] = p->ticks_inq[i];
		}
		n++;
	} 
	release(&ptable.lock);
	return n;
}
