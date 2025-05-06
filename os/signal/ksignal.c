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

int siginit_fork(struct proc *parent, struct proc *child) {
    // copy parent's sigactions and signal mask
    // but clear all pending signals
    return 0;
}

int siginit_exec(struct proc *p) {
    // inherit signal mask and pending signals.
    // but reset all sigactions (except ignored) to default.
    return 0;
}

int do_signal(void) {
    assert(!intr_get());

    return 0;
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

    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    
    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    
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