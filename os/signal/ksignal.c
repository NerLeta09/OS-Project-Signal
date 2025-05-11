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
    assert(!intr_get());

    struct proc *p = curr_proc();
    struct trapframe *tf = p->trapframe;

    // 查找待处理的信号
    for(int signo = SIGMIN; signo <= SIGMAX; signo++) {
        // 检查是否有pending信号
        if (!sigismember(&p->signal.sigpending, signo)) 
            continue;

        // SIGKILL和SIGSTOP不能被阻塞
        if (signo == SIGKILL || signo == SIGSTOP) {
            sigdelset(&p->signal.sigpending, signo);
            setkilled(p, -10 - signo);
            continue;
        }
        
        // 检查信号是否被阻塞
        if (sigismember(&p->signal.sigmask, signo)) 
            continue;
            
        // 获取当前信号处理函数
        struct sigaction *sa = &p->signal.sa[signo];
        
        // 移除pending信号标记
        sigdelset(&p->signal.sigpending, signo);
        
        // 如果信号被忽略，继续处理下一个信号
        if (sa->sa_sigaction == SIG_IGN)
            continue;
            
        // 如果使用默认处理方式，结束进程
        if(sa->sa_sigaction == SIG_DFL) {
            setkilled(p, -10 - signo);
            continue;
        }
        
        // 自定义处理函数的情况
        // 1. 计算需要在用户栈上分配的空间
        uint64 old_sp = tf->sp;
        
        // 2. 在用户栈上为ucontext和siginfo_t分配空间，需要16字节对齐
        uint64 new_sp = PGROUNDDOWN(old_sp - sizeof(struct ucontext) - sizeof(siginfo_t));
        
        // 3. 构造ucontext结构
        struct ucontext uc;
        uc.uc_sigmask = p->signal.sigmask; // 保存当前signal mask
        
        // 保存当前用户态上下文
        uc.uc_mcontext.epc = tf->epc;
        // 保存通用寄存器
        uc.uc_mcontext.regs[0] = 0; // zero寄存器
        uc.uc_mcontext.regs[1] = tf->ra;
        uc.uc_mcontext.regs[2] = tf->sp;
        uc.uc_mcontext.regs[3] = tf->gp;
        uc.uc_mcontext.regs[4] = tf->tp;
        uc.uc_mcontext.regs[5] = tf->t0;
        uc.uc_mcontext.regs[6] = tf->t1;
        uc.uc_mcontext.regs[7] = tf->t2;
        uc.uc_mcontext.regs[8] = tf->s0;
        uc.uc_mcontext.regs[9] = tf->s1;
        uc.uc_mcontext.regs[10] = tf->a0;
        uc.uc_mcontext.regs[11] = tf->a1;
        uc.uc_mcontext.regs[12] = tf->a2;
        uc.uc_mcontext.regs[13] = tf->a3;
        uc.uc_mcontext.regs[14] = tf->a4;
        uc.uc_mcontext.regs[15] = tf->a5;
        uc.uc_mcontext.regs[16] = tf->a6;
        uc.uc_mcontext.regs[17] = tf->a7;
        uc.uc_mcontext.regs[18] = tf->s2;
        uc.uc_mcontext.regs[19] = tf->s3;
        uc.uc_mcontext.regs[20] = tf->s4;
        uc.uc_mcontext.regs[21] = tf->s5;
        uc.uc_mcontext.regs[22] = tf->s6;
        uc.uc_mcontext.regs[23] = tf->s7;
        uc.uc_mcontext.regs[24] = tf->s8;
        uc.uc_mcontext.regs[25] = tf->s9;
        uc.uc_mcontext.regs[26] = tf->s10;
        uc.uc_mcontext.regs[27] = tf->s11;
        uc.uc_mcontext.regs[28] = tf->t3;
        uc.uc_mcontext.regs[29] = tf->t4;
        uc.uc_mcontext.regs[30] = tf->t5;
        
        // 4. 构造siginfo_t结构
        siginfo_t info = p->signal.siginfos[signo];
        
        // 5. 将ucontext和siginfo_t复制到用户栈
        uint64 uc_addr = new_sp + sizeof(siginfo_t);
        uint64 info_addr = new_sp;
        
        // 获取锁，先获取p->lock，再获取p->mm->lock
        acquire(&p->lock);
        acquire(&p->mm->lock);
        
        // 复制到用户空间
        int copy_result = 0;
        copy_result = copy_to_user(p->mm, uc_addr, (char *)&uc, sizeof(struct ucontext));
        if (copy_result >= 0) {
            copy_result = copy_to_user(p->mm, info_addr, (char *)&info, sizeof(siginfo_t));
        }
        
        // 释放锁，按获取顺序的相反顺序释放
        release(&p->mm->lock);
        release(&p->lock);
        
        if (copy_result < 0) {
            // 复制失败，忽略这个信号
            continue;
        }
        
        // 6. 根据sigaction修改当前进程的signal mask
        // 在执行信号处理函数时，添加sa_mask中的信号到当前mask
        p->signal.sigmask |= sa->sa_mask;
        // 同时自动屏蔽当前正在处理的信号，防止重入
        p->signal.sigmask |= sigmask(signo);
        
        // 7. 修改trapframe，以便在用户态开始执行信号处理函数
        tf->epc = (uint64)sa->sa_sigaction;  // 设置程序计数器为信号处理函数
        tf->sp = new_sp;  // 设置栈指针
        
        // 设置信号处理函数的参数
        tf->a0 = signo;         // a0: signo
        tf->a1 = info_addr;     // a1: siginfo_t*
        tf->a2 = uc_addr;       // a2: ucontext*
        
        // 设置返回地址为sa_restorer
        tf->ra = (uint64)sa->sa_restorer;  // ra
        
        return 0; // 一次只处理一个信号
    }
    
    return 0; // 没有待处理的信号
}

// syscall handlers:
//  sys_* functions are called by syscall.c

int sys_sigaction(int signo, const sigaction_t __user *act, sigaction_t __user *oldact) {
    struct proc *p = curr_proc();
    
    // 检查signo是否合法
    if (signo < SIGMIN || signo > SIGMAX) {
        return -EINVAL;
    }
    
    // 获取锁
    // acquire(&p->lock);
    acquire(&p->mm->lock);
    
    // // 检查SIGKILL和SIGSTOP不能被忽略或捕获
    // if ((signo == SIGKILL || signo == SIGSTOP) && act != NULL) {
    //     struct sigaction tmp_sa;
    //     if (copy_from_user(p->mm, (char *)&tmp_sa, (uint64)act, sizeof(struct sigaction)) < 0) {
    //         release(&p->mm->lock);
    //         // release(&p->lock);
    //         return -EINVAL;
    //     }
        
    //     if (tmp_sa.sa_sigaction != SIG_DFL) {
    //         release(&p->mm->lock);
    //         // release(&p->lock);
    //         return -EINVAL;
    //     }
    // }
    
    struct sigaction *old_sa = &p->signal.sa[signo];
    
    // 如果oldact不为NULL，将旧的sigaction拷贝到用户空间
    if (oldact != NULL) {
        if (copy_to_user(p->mm, (uint64)oldact, (char *)old_sa, sizeof(struct sigaction)) < 0) {
            release(&p->mm->lock);
            // release(&p->lock);
            return -EINVAL;
        }
    }

    // 如果act不为NULL，从用户空间拷贝新的sigaction
    if (act != NULL) {
        if (copy_from_user(p->mm, (char *)old_sa, (uint64)act, sizeof(struct sigaction)) < 0) {
            release(&p->mm->lock);
            // release(&p->lock);
            return -EINVAL;
        }
    }
    
    release(&p->mm->lock);
    // release(&p->lock);
    
    return 0;
}

int sys_sigreturn() {
    struct proc *p = curr_proc();
    struct trapframe *tf = p->trapframe;
    
    // 1. 计算ucontext和siginfo_t在用户栈上的地址
    uint64 sp = tf->sp; // 当前栈指针
    uint64 uc_addr = sp + sizeof(siginfo_t);
    
    // 2. 从用户空间拷贝ucontext结构
    struct ucontext uc;
    
    // 获取锁
    acquire(&p->lock);
    acquire(&p->mm->lock);
    
    int copy_result = copy_from_user(p->mm, (char *)&uc, uc_addr, sizeof(struct ucontext));
    
    // 释放锁
    release(&p->mm->lock);
    release(&p->lock);
    
    if (copy_result < 0) {
        return -EINVAL;
    }
    
    // 3. 恢复信号掩码
    p->signal.sigmask = uc.uc_sigmask;
    
    // 4. 恢复用户态上下文，包括程序计数器和通用寄存器
    tf->epc = uc.uc_mcontext.epc;
    tf->ra = uc.uc_mcontext.regs[1];
    tf->sp = uc.uc_mcontext.regs[2];
    tf->gp = uc.uc_mcontext.regs[3];
    tf->tp = uc.uc_mcontext.regs[4];
    tf->t0 = uc.uc_mcontext.regs[5];
    tf->t1 = uc.uc_mcontext.regs[6];
    tf->t2 = uc.uc_mcontext.regs[7];
    tf->s0 = uc.uc_mcontext.regs[8];
    tf->s1 = uc.uc_mcontext.regs[9];
    tf->a0 = uc.uc_mcontext.regs[10];
    tf->a1 = uc.uc_mcontext.regs[11];
    tf->a2 = uc.uc_mcontext.regs[12];
    tf->a3 = uc.uc_mcontext.regs[13];
    tf->a4 = uc.uc_mcontext.regs[14];
    tf->a5 = uc.uc_mcontext.regs[15];
    tf->a6 = uc.uc_mcontext.regs[16];
    tf->a7 = uc.uc_mcontext.regs[17];
    tf->s2 = uc.uc_mcontext.regs[18];
    tf->s3 = uc.uc_mcontext.regs[19];
    tf->s4 = uc.uc_mcontext.regs[20];
    tf->s5 = uc.uc_mcontext.regs[21];
    tf->s6 = uc.uc_mcontext.regs[22];
    tf->s7 = uc.uc_mcontext.regs[23];
    tf->s8 = uc.uc_mcontext.regs[24];
    tf->s9 = uc.uc_mcontext.regs[25];
    tf->s10 = uc.uc_mcontext.regs[26];
    tf->s11 = uc.uc_mcontext.regs[27];
    tf->t3 = uc.uc_mcontext.regs[28];
    tf->t4 = uc.uc_mcontext.regs[29];
    tf->t5 = uc.uc_mcontext.regs[30];
    tf->t6 = 0; // 这个可能在数组越界，谨慎起见设为0
    
    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    struct proc *p = curr_proc();
    
    acquire(&p->lock);
    acquire(&p->mm->lock);
    
    if(oldset != NULL) {
        if (copy_to_user(p->mm, (uint64)oldset, (char *)&p->signal.sigmask, sizeof(sigset_t)) < 0) {
            release(&p->mm->lock);
            release(&p->lock);
            return -EINVAL;
        }
    }

    if(set != NULL) {
        sigset_t tmp;
        if (copy_from_user(p->mm, (char *)&tmp, (uint64)set, sizeof(sigset_t)) < 0) {
            release(&p->mm->lock);
            release(&p->lock);
            return -EINVAL;
        }
        
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
                release(&p->mm->lock);
                release(&p->lock);
                return -EINVAL;
        }
    }
    
    release(&p->mm->lock);
    release(&p->lock);

    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    struct proc *p = curr_proc();

    if (set == NULL) 
        return -EINVAL;
    
    acquire(&p->lock);
    acquire(&p->mm->lock);
    
    int result = copy_to_user(p->mm, (uint64)set, (char *)&p->signal.sigpending, sizeof(sigset_t));
    
    release(&p->mm->lock);
    release(&p->lock);
    
    if (result < 0) {
        return -EINVAL;
    }

    return 0;
}

int sys_sigkill(int pid, int signo, int code) {
    // 检查信号是否合法
    if (signo < SIGMIN || signo > SIGMAX) {
        return -EINVAL;
    }
    
    // 查找目标进程
    struct proc *p = findByPid(pid);
    if(p == NULL) return -EINVAL; // 进程不存在
    
    // 特殊处理SIGKILL和SIGSTOP信号，直接结束或停止进程
    if (signo == SIGKILL) {
        // 获取p->lock以保护对进程状态的修改
        // acquire(&p->lock); //这一步重复持有锁了，要注释掉
        setkilled(p, -10 - signo);
        // release(&p->lock);
        return 0;
    }
    
    // 获取锁以保护对signal结构的修改
    // acquire(&p->lock);
    
    // 设置pending信号位
    sigaddset(&p->signal.sigpending, signo);
    
    // 填充siginfo信息
    p->signal.siginfos[signo].si_signo = signo;
    p->signal.siginfos[signo].si_code = code;
    p->signal.siginfos[signo].si_pid = curr_proc()->pid; // 设置发送进程的pid
    
    // release(&p->lock);
    
    return 0;
}