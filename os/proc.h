#ifndef PROC_H
#define PROC_H

#include "queue.h"
#include "riscv.h"
#include "vm.h"
#include "signal/ksignal.h"

enum {
    STDIN  = 0,
    STDOUT = 1,
    STDERR = 2,
};

// Saved registers for kernel context switches.
struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct cpu {
    int mhart_id;                  // mhartid for this cpu, passed by OpenSBI
    struct proc *proc;             // current process
    struct context sched_context;  // scheduler context, swtch() here to run scheduler
    int inkernel_trap;             // whether we are in a kernel trap context
    int noff;                      // how many push-off
    int interrupt_on;              // Is the interrupt Enabled before the first push-off?
    uint64 sched_kstack_top;       // top of per-cpu sheduler kernel stack
    int cpuid;                     // for debug purpose
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, STOPPED };

extern struct proc *pool[];

// Per-process state
struct proc {
    spinlock_t lock;
    // p->lock must be held when accessing to these fields:
    enum procstate state;  // Process state
    int pid;               // Process ID
    int exit_code;
    void *sleep_chan;
    int killed;

    struct proc *parent;  // Parent process

    int index;
    struct mm *mm;
    struct vma *vma_brk;                // special vma for heap, included in mm->vma list.
    uint64 brk;                         // end address of heap
    struct trapframe *__kva trapframe;  // data page for trampoline.S
    uint64 __kva kstack;                // Virtual address of kernel stack
    struct context context;             // swtch() here to run process

    // Project signal:
    struct ksignal signal;
    
    // SIGSTOP相关
    int need_stop;                     // 标记进程是否需要停止
};

static inline int cpuid() {
    return r_tp();
}

// cpu.c
struct cpu *mycpu();
struct cpu *getcpu(int i);

static inline struct proc *curr_proc() {
    push_off();
    struct cpu *c  = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

// proc.c
void proc_init();
struct proc *allocproc();
int fork();
int exec(char *name, char *arg[]);
int wait(int, int *);
void exit(int);
int kill(int pid);
int iskilled(struct proc *);
void setkilled(struct proc *, int reason);

// findByPid函数声明
// 如果找到进程，返回进程指针且持有进程锁，由调用者负责释放锁
// 如果未找到进程，返回NULL
struct proc *findByPid(int pid);

void sleep(void *chan, spinlock_t *lk);
void wakeup(void *chan);

// sched.c
void scheduler() __attribute__((noreturn));
void sched();
void yield();
void add_task(struct proc *);

// swtch.S
void swtch(struct context *, struct context *);

#endif  // PROC_H
