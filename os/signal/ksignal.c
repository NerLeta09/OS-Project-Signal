#include "ksignal.h"

#include <defs.h>
#include <proc.h>
#include <trap.h>

/**
 * @brief init the signal struct inside a PCB.
 * 
 * @param p 
 * @return int 
 */
int siginit(struct proc *p) {
    // init p->signal
    if(p == NULL) return -1;
    struct ksignal *ksig = &p->signal;

    for (int i = SIGMIN; i <= SIGMAX; i++) {
        ksig->sa[i].sa_sigaction = SIG_DFL;
        sigemptyset(&ksig->sa[i].sa_mask);
        ksig->sa[i].sa_restorer = 0;
    }

    sigemptyset(&ksig->sigmask);
    sigemptyset(&ksig->sigpending);

    return 0;
}

// copy parent's sigactions and signal mask
// but clear all pending signals
int siginit_fork(struct proc *parent, struct proc *child) {
    if (!parent || !child) return -1;

    struct ksignal *psig = &parent->signal;
    struct ksignal *csig = &child->signal;

    // sigaction
    for (int i = SIGMIN; i <= SIGMAX; i++) {
        csig->sa[i] = psig->sa[i];
    }

    // sigmask
    csig->sigmask = psig->sigmask;

    // clear all pending signals
    sigemptyset(&csig->sigpending);

    return 0;
}

// inherit signal mask and pending signals.
// but reset all sigactions (except ignored) to default.
int siginit_exec(struct proc *p) {
    if(p == NULL) return -1;

    struct ksignal *sig = &p->signal;
    struct ksignal *parent_sig = &curr_proc()->signal;
    
    sig->sigmask = parent_sig->sigmask;
    sig->sigpending = parent_sig->sigpending;
 
    //reset
    for(int signo = SIGMIN; signo <= SIGMAX; signo++) {
        struct sigaction *thi_sa = &p->signal.sa[signo];
        if (thi_sa->sa_sigaction != SIG_IGN) {
            thi_sa->sa_sigaction = SIG_DFL;
        }
       
        sigemptyset(&thi_sa->sa_mask);
        thi_sa->sa_restorer = 0;
    }
    
    return 0;
}

//如果用户指定了处理函数，则在用户态跳转到 signal 处理函数，并在其返回后恢复执行 signal handler 前的用户态状态
int do_signal(void) {
    //TODO: Finish.
    assert(!intr_get());

    struct proc *p = curr_proc();
    struct trapframe *tf = p->trapframe;

    for(int signo = SIGMIN; signo <= SIGMAX; signo++) {
        if (!sigismember(&p->signal.sigpending, signo)) continue;
        if (sigismember(&p->signal.sigmask, signo)) continue;
        //find signal
        //获取当前signo handler
        //从sigpending中将signo移除
        //handle 进程可以指定某个 signal 的处理方式为 SIG_DFL、SIG_IGN 或指定的 sa_sigaction

        
        //保存寄存器上下文

    }
    return 0; //no signal in pending, return
}

// syscall handlers:
//  sys_* functions are called by syscall.c

int sys_sigaction(int signo, const sigaction_t __user *act, sigaction_t __user *oldact) {
    struct proc *p = curr_proc();
    struct sigaction *old_sa = &p->signal.sa[signo];
    
    if (oldact != NULL) {
        if (copy_to_user(p->mm, (uint64)oldact, (char *)old_sa, sizeof(struct sigaction)) < 0)
            return -EINVAL;
    }

    if (act != NULL) {
        if (copy_from_user(p->mm, (char *)act, (uint64)old_sa, sizeof(struct sigaction)) < 0)
            return -EINVAL;
    }
    return 0;
}

int sys_sigreturn() {
    //TODO: Finish.
    struct proc *p = curr_proc();
    struct trapframe *ktf = p->trapframe;  
    
    //refine sigmask
    p->signal.sigmask = p->signal.sigpending;

    //refine context

    //refine pc & sp    
    
    //refine other regs
    
    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    struct proc *p = curr_proc();
    if(oldset != NULL) {
        // *oldset = p->signal.sigmask;
        copy_to_user(p->mm, (uint64)oldset, (char *)&p->signal.sigmask, sizeof(sigset_t));
    }

    if(set != NULL) {
        sigset_t tmp;
        copy_from_user(p->mm, (char *)&tmp, (uint64)set, sizeof(sigset_t));
        switch(how) {
            case SIG_BLOCK:
                p->signal.sigmask |= tmp;
                break;
            case SIG_UNBLOCK:
                p->signal.sigmask &= ~tmp;
                break;
            case SIG_SETMASK:
                p->signal.sigmask = tmp;
                break;
            default:
                return -EINVAL;
        }
    }

    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    struct proc *p = curr_proc();

    if (set == NULL) 
        return -EINVAL;
    
    if (copy_to_user(p->mm, (uint64)set, (char *)&p->signal.sigpending, sizeof(sigset_t)) < 0) {
        return -1;
    }

    return 0;
}

int sys_sigkill(int pid, int signo, int code) {
    if (signo < SIGMIN || signo > SIGMAX) return -1;
    struct proc *p = NULL;
    for(int i = 0; i < NPROC; i++) {
        struct proc *tmpp = pool[i];
        if(tmpp && tmpp->pid == pid) { //How to find proc by pid? 要不要在proc.c里写个函数？
            p = tmpp;
            //向目标进程发送signal
            sigaddset(&p->signal.sigpending, signo);

            setkilled(p, -10 - signo);
            return 0;
        }
    }
    return -1;//no find proc
}