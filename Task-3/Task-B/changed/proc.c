#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "stat.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

int SOP_PRESENT = 0;
int SIP_PRESENT = 0;

int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

int
close_file(int fd)
{
  struct file *f;

  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
write_file(int fd, char *p, int n)
{
  struct file *f;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  return filewrite(f, p, n);
}


static struct inode*
create_file(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}


static int
fdalloc_file(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int open_file(char *path, int omode){

  int fd;
  struct file *f;
  struct inode *ip;

  begin_op();

  if(omode & O_CREATE){
    ip = create_file(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc_file(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;

}

void num_to_str(int x, char *c){
  if(x==0) {
    c[0] = '0';
    c[1] = '\0';
    return;
  }
  int i = 0;
  while(x>0){
    c[i] = x%10+'0';
    x /= 10;
    i++;
  }
  c[i]='\0';

  for(int j=0; j<i/2; j++){
    char a = c[j];
    c[j] = c[i-j-1];
    c[i-j-1] = a;
  }

}

struct swap_req{
  struct spinlock lock; // lock to restrict access of this swap request queue
  struct proc* queue[NPROC];
  int start;
  int end;
};

// request queue for swapping out requests
struct swap_req swap_out_req;
// request queue for swapping in requests
struct swap_req swap_in_req;

struct proc* swap_req_pop(struct swap_req *q){

  acquire(&q->lock);
  if(q->start == q->end){
  	release(&q->lock);
  	return 0;
  }
  struct proc *p = q->queue[q->start];
  (q->start)++;
  (q->start) %= NPROC;
  release(&q->lock);

  return p;
}

int swap_req_push(struct proc *p, struct swap_req *q){

  acquire(&q->lock);
  if((q->end+1)%NPROC == q->start){
  	release(&q->lock);
    return 0;
  }
  q->queue[q->end] = p;
  q->end++;
  (q->end) %= NPROC;
  release(&q->lock);
  
  return 1;
}
 
void SWAP_OUT_PROCESS() {

  acquire(&swap_out_req.lock);
  while(swap_out_req.start != swap_out_req.end){
    struct proc *p = swap_req_pop(&swap_out_req);

    pde_t* pgdir = p->pgdir;
    for(int i=0; i<NPDENTRIES; i++){ // going throigh the page directory entries.

      //skip page table if accessed. chances are high, not every page table was accessed.
      if(pgdir[i] & PTE_A) continue;
      
      pte_t *pgtab = (pte_t*)P2V(PTE_ADDR(pgdir[i]));
      for(int j=0; j<NPTENTRIES; j++){ // going through the the page table entries

        //Skip if found
        if((pgtab[j]&PTE_A) || !(pgtab[j]&PTE_P)) continue;
        
        pte_t *pte = (pte_t*)P2V(PTE_ADDR(pgtab[j]));

        //for file name
        int pid = p->pid;
        // file name contians virtual address of the swaping out which helps SWAP_IN_PROCESS 
        // to swap in a particular page fault at a given address by the process.
        int virt_addr = ((1<<22)*i)+((1<<12)*j); 
        //file name
        char c[50];
        num_to_str(pid,c);
        int x = strlen(c);
        c[x] = '-';
        num_to_str(virt_addr,c+x+1);
        safestrcpy(c+strlen(c),".swp",5);

        // file management
        int fd = open_file(c, O_CREATE | O_RDWR);
        if(fd<0){
          cprintf("error creating or opening file: %start\n", c);
          panic("SWAP_OUT_PROCESS");
        }

        if(write_file(fd,(char *)pte, PGSIZE) != PGSIZE){
          cprintf("error writing to file: %start\n", c);
          panic("SWAP_OUT_PROCESS");
        }
        close_file(fd);

        kfree((char*)pte); // freeing this page and adding it back to the freelist pages.
        memset(&pgtab[j], 0, sizeof(pgtab[j]));

        //mark this page as being swapped out.
        pgtab[j] = ((pgtab[j])^(0x080));

        break;
      }
    }

  }

  release(&swap_out_req.lock);
  
  struct proc *p;
  if((p=myproc()) == 0)
    panic("swap out process");

  SOP_PRESENT = 0; // setting it zero so that another new SWAP_OUT_PROCESS can be cereated.
  p->parent = 0;
  p->name[0] = '*';
  p->killed = 0;
  p->state = UNUSED; // Killing this swapping out process.
  sched(); // calling scheduler.
}

int read_file(int fd, int n, char *p)
{
  struct file *f;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
  return -1;
  return fileread(f, p, n);

}

void SWAP_IN_PROCESS() {
    
    acquire(&swap_in_req.lock);
    while(swap_in_req.start != swap_in_req.end){
		struct proc *p = swap_req_pop(&swap_in_req);

		int pid = p->pid;
		int virt_addr = PTE_ADDR(p->PGFLT_addr);

		char c[50];
		num_to_str(pid,c);
		int x = strlen(c);
		c[x] = '-';
		num_to_str(virt_addr,c+x+1); // getting the page which existed at this va before getting swapped out.
		safestrcpy(c+strlen(c),".swp",5);

		int fd = open_file(c,O_RDONLY);
		if(fd<0){
			release(&swap_in_req.lock);
			cprintf("could not find page file in memory: %start\n", c);
			panic("SWAP_IN_PROCESS");
		}
		char *mem = kalloc();
		read_file(fd,PGSIZE,mem);

		if(mappages(p->pgdir, (void *)virt_addr, PGSIZE, V2P(mem), PTE_W|PTE_U)<0){
			release(&swap_in_req.lock);
			panic("mappages");
		}
		wakeup(p);
	}

    release(&swap_in_req.lock);
    struct proc *p;
	if((p=myproc()) == 0)
	  panic("SWAP_IN_PROCESS");

	SIP_PRESENT = 0; // resetting the value so a new SWAP_OUT_PROCESS may be created. 
	p->parent = 0;
	p->name[0] = '*';
	p->killed = 0;
	p->state = UNUSED;
	sched(); // calling the scheduler.
}

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
  // Intializing locks swap_qeues and swapsleep
  initlock(&swap_out_req.lock, "swap_out_req");
  initlock(&swapsleeplock, "swapsleep");
  initlock(&swap_in_req.lock, "swap_in_req");
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

void create_kernel_process(const char *name, void (*entrypoint)()){
    
  struct proc *p = allocproc();

  if(p == 0)
    panic("create_kernel_process failed");

  //Setting up kernel page table using setupkvm
  if((p->pgdir = setupkvm()) == 0)
    panic("setupkvm failed");

  //This is a kernel process. Trap frame stores user space registers. We don't need to initialise tf.
  //Also, since this doesn't need to have a userspace, we don't need to assign a size to this process.

  //eip stores address of next instruction to be executed
  p->context->eip = (uint)entrypoint;

  safestrcpy(p->name, name, sizeof(p->name));

  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);

}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  // intializing swap_queues.
  acquire(&swap_out_req.lock);
  swap_out_req.start=0;
  swap_out_req.end=0;
  release(&swap_out_req.lock);

  acquire(&swap_in_req.lock);
  swap_in_req.start=0;
  swap_in_req.end=0;
  release(&swap_in_req.lock);
  
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
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    
      //If the swap processes have stopped running, free its stack and name.
      if(p->state==UNUSED && p->name[0]=='*'){
        kfree(p->kstack);
        p->kstack = 0;
        p->name[0] = 0;
        p->pid = 0;
      }

      if(p->state != RUNNABLE)
        continue;

      // we will reset the access bit of the selected process as we will just mark a recently used if it used in the last quantum of the process.
      for(int i=0; i<NPDENTRIES; i++){
        //If PDE was accessed

        if(((p->pgdir)[i])&PTE_P && ((p->pgdir)[i])&PTE_A){
          pte_t* pgtab = (pte_t*)P2V(PTE_ADDR((p->pgdir)[i]));
          for(int j=0; j<NPTENTRIES; j++){
            if(pgtab[j] & PTE_A){
              pgtab[j] ^= PTE_A;
            }
          }
          ((p->pgdir)[i]) ^= PTE_A;
        }
      }
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
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
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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
