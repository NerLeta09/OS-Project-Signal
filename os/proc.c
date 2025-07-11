#include "proc.h"

#include "defs.h"
#include "kalloc.h"
#include "loader.h"
#include "queue.h"
#include "trap.h"

struct proc *pool[NPROC];
struct proc *init_proc = NULL;
static allocator_t proc_allocator;

static spinlock_t pid_lock;
static spinlock_t wait_lock;

extern void sched_init();

// initialize the proc table at boot time.
void proc_init() {
    // we only init once.
    static int proc_inited = 0;
    assert(proc_inited == 0);
    proc_inited = 1;

    spinlock_init(&pid_lock, "pid");
    spinlock_init(&wait_lock, "wait");

    allocator_init(&proc_allocator, "proc", sizeof(struct proc), NPROC);
    struct proc *p;

    uint64 proc_kstack = KERNEL_STACK_PROCS;

    for (int i = 0; i < NPROC; i++) {
        p = kalloc(&proc_allocator);
        memset(p, 0, sizeof(*p));
        spinlock_init(&p->lock, "proc");
        p->index = i;
        p->state = UNUSED;

        // allocate the Trapframe.
        uint64 __pa tf = (uint64)kallocpage();
        // during system boots, we should always have enough memory.
        assert(tf);
        p->trapframe = (struct trapframe *)PA_TO_KVA(tf);

        p->kstack = proc_kstack;
        for (uint64 va = proc_kstack; va < proc_kstack + KERNEL_STACK_SIZE; va += PGSIZE) {
            uint64 __pa newpg = (uint64)kallocpage();
            assert(newpg);
            kvmmap(kernel_pagetable, va, newpg, PGSIZE, PTE_A | PTE_D | PTE_R | PTE_W);
        }
        sfence_vma();
        proc_kstack += 2 * KERNEL_STACK_SIZE;

        pool[i] = p;
    }
    sched_init();
}

static int allocpid() {
    static int PID = 1;
    int retpid     = -1;

    acquire(&pid_lock);
    retpid = PID++;
    release(&pid_lock);

    return retpid;
}
static void first_sched_ret(void) {
    release(&curr_proc()->lock);
    assert(curr_proc()->state == RUNNING);
    intr_off();
    usertrapret();
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *allocproc() {
    struct proc *p;
    for (int i = 0; i < NPROC; i++) {
        p = pool[i];
        acquire(&p->lock);
        if (p->state == UNUSED) {
            goto found;
        }
        release(&p->lock);
    }
    return 0;

found:
    // initialize a proc
    tracef("init proc %p", p);
    p->parent     = NULL;
    p->exit_code  = 0;
    p->sleep_chan = NULL;
    p->pid        = allocpid();
    p->state      = USED;
    p->need_stop  = 0;  // 初始化need_stop字段

    // fork or exec(load_user_elf) will initialize these:
    p->mm      = NULL;
    p->vma_brk = NULL;

    // prepare trapframe and the first return context.
    memset(&p->context, 0, sizeof(p->context));
    memset((void *)p->kstack, 0, KERNEL_STACK_SIZE);
    memset((void *)p->trapframe, 0, PGSIZE);
    p->context.ra = (uint64)first_sched_ret;
    p->context.sp = p->kstack + KERNEL_STACK_SIZE;

    // Project signal: signal_init
    siginit(p);

    assert(holding(&p->lock));

    return p;
}

static void freeproc(struct proc *p) {
    assert(holding(&p->lock));

    p->state      = UNUSED;
    p->pid        = -1;
    p->exit_code  = 0xdeadbeef;
    p->sleep_chan = NULL;
    p->killed     = 0;
    p->parent     = NULL;

    if (p->mm) {
        assert(!holding(&p->mm->lock));
        acquire(&p->mm->lock);
        mm_free(p->mm);
    }

    p->mm      = NULL;
    p->vma_brk = NULL;
}

void sleep(void *chan, spinlock_t *lk) {
    struct proc *p = curr_proc();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&p->lock);  // DOC: sleeplock1
    release(lk);

    // Go to sleep.
    p->sleep_chan = chan;
    p->state      = SLEEPING;

    sched();

    // p get waking up, Tidy up.
    p->sleep_chan = 0;

    // Reacquire original lock.
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = pool[i];
        acquire(&p->lock);
        if (p->state == SLEEPING && p->sleep_chan == chan) {
            p->state = RUNNABLE;
            add_task(p);
        }
        release(&p->lock);
    }
}

int fork() {
    int ret;
    struct proc *np = allocproc();
    // Allocate process.
    if (np == NULL) {
        return -ENOMEM;
    }
    np->mm = mm_create(np->trapframe);
    if (np->mm == NULL) {
        freeproc(np);
        release(&np->lock);
        return -ENOMEM;
    }

    assert(holding(&np->lock));

    struct proc *p = curr_proc();
    acquire(&p->lock);
    acquire(&p->mm->lock);

    // Copy user memory from parent to child.
    if ((ret = mm_copy(p->mm, np->mm)) < 0)
        goto err_free;
    // Set np's vma_brk
    np->vma_brk = mm_find_vma(np->mm, p->vma_brk->vm_start);
    np->brk     = p->brk;

    release(&p->mm->lock);
    release(&np->mm->lock);

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Project signal: fork
    siginit_fork(p, np);

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;
    np->parent        = p;
    np->state         = RUNNABLE;
    add_task(np);
    release(&np->lock);
    release(&p->lock);

    return np->pid;

err_free:
    release(&np->mm->lock);
    release(&p->mm->lock);
    release(&p->lock);
    freeproc(np);
    release(&np->lock);
    return ret;
}

int exec(char *name, char *args[]) {
    struct user_app *app = get_elf(name);
    if (app == NULL)
        return -ENOENT;

    int ret;
    struct proc *p = curr_proc();

    acquire(&p->lock);

    // execve does NOT preserve memory mappings:
    //  free VMAs including program_brk, and ustack
    // load_user_elf() will create a new mm for the new process and free the old one
    //  , if page allocations all succeed.
    // Otherwise, we will return to the old process.
    // However, keep the phys page of trapframe, because it belongs to struct proc.
    if ((ret = load_user_elf(app, p, args)) < 0) {
        release(&p->lock);
        return ret;
    }

    // Project signal: exec
    siginit_exec(p);

    release(&p->lock);

    // syscall() will overwrite trapframe->a0 to the return value.
    return p->trapframe->a0;
}

int wait(int pid, int __user *code) {
    struct proc *child;
    int havekids;
    struct proc *p = curr_proc();

    acquire(&wait_lock);

    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (int i = 0; i < NPROC; i++) {
            child = pool[i];
            if (child == p)
                continue;

            acquire(&child->lock);
            if (child->parent == p) {
                havekids = 1;
                if (child->state == ZOMBIE && (pid <= 0 || child->pid == pid)) {
                    int cpid = child->pid;
                    // Found one.
                    if (code) {
                        acquire(&p->mm->lock);
                        int exit_code = child->exit_code;
                        copy_to_user(p->mm, (uint64)code, (char*)&exit_code, sizeof(int));
                        release(&p->mm->lock);
                    }
                    freeproc(child);
                    release(&child->lock);

                    release(&wait_lock);
                    return cpid;
                }
            }
            release(&child->lock);
        }

        // No waiting if we don't have any children.
        if (!havekids || p->killed) {
            release(&wait_lock);
            return -ECHILD;
        }

        debugf("pid %d sleeps for wait", p->pid);
        // Wait for a child to exit.
        sleep(p, &wait_lock);  // DOC: wait-sleep
    }
}

// Exit the current process.
void exit(int code) {
    struct proc *p = curr_proc();
    struct proc *parent_proc = p->parent;
    
    if (p == init_proc) {
        panic("init process exited");
    }

    acquire(&wait_lock);

    int wakeinit = 0;

    // reparent:
    for (int i = 0; i < NPROC; i++) {
        struct proc *child = pool[i];
        if (child == p)
            continue;
        acquire(&child->lock);
        if (child->parent == p) {
            child->parent = init_proc;
            wakeinit      = 1;
            // if child has dead, wake up init to do clean up.
        }
        release(&child->lock);
    }
    if (wakeinit)
        wakeup(init_proc);



    //5.3.3: 向父进程发送 SIGCHLD 信号（排除init进程）
    if (parent_proc && !wakeinit && parent_proc != init_proc) {

        // 保存父进程PID（防止父进程改变）
        int ppid = parent_proc->pid;
        int child_pid = p->pid;
        int exit_code = 0;
        
        // 临时释放锁避免死锁
        release(&wait_lock);
        
        // 发送SIGCHLD信号
        sys_sigkill(ppid, SIGCHLD, code);
        
        // 重新获取锁
        acquire(&wait_lock);
            
    }

    // 唤醒等待的父进程
    wakeup(p->parent);

    acquire(&p->lock);

    p->exit_code = code;
    p->state     = ZOMBIE;

    release(&wait_lock);

    sched();
    panic_never_reach();
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid) {
    struct proc *p;

    for (int i = 0; i < NPROC; i++) {
        p = pool[i];
        acquire(&p->lock);
        if (p->pid == pid) {
            p->killed = -10 - SIGKILL; // 使用SIGKILL信号码
            if (p->state == SLEEPING) {
                // Wake process from sleep().
                p->state = RUNNABLE;
                add_task(p);
            }
            struct proc *parent_proc = p->parent;

            if (parent_proc != NULL) {
                
                sys_sigkill(parent_proc->pid, SIGCHLD, -1);
        
            }
            
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -EINVAL;
}

void setkilled(struct proc *p, int reason) {
    assert(reason < 0);
    acquire(&p->lock);
    p->killed = reason;
    release(&p->lock);
}

int iskilled(struct proc *p) {
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}


struct proc *findByPid(int pid) {
    struct proc *p;
    for (int i = 0; i < NPROC; i++) {
        p = pool[i];
        acquire(&p->lock);
        if (p->pid == pid) {
            // 不释放锁，调用者负责释放
            return p;
        }
        release(&p->lock);
    }
    return NULL;
}
