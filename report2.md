# Report 2

OS-Project-Signal 自选部分报告

## 实现功能

### 5.3.2 SIGINFO

#### 用户进程的`siginfo`
在`sys_sigkill()`中，我们在checkpoint 1的基础上添加了`siginfo`.

- `siginfo.signo` 为信号的signo

- `siginfo.si_pid` 为发送信号的用户进程pid

- `siginfo`其他变量为0

```c
//sys_sigkill()

p->signal.siginfos[signo].si_signo = signo;
p->signal.siginfos[signo].si_code = 0; // 默认为0
p->signal.siginfos[signo].si_pid = curr_proc()->pid;
p->signal.siginfos[signo].si_status = 0;
p->signal.siginfos[signo].addr = 0;
```


#### 内核信号的`siginfo`
由于原代码中不涉及内核发送的信号，我们将`SIGUSR2`作为自定义的内核发出信号，并在系统调用中添加了`KTEST_SEND_SIGUSR2`作为用户进程主动请求

```c
uint64 ktest_syscall(uint64 args[6]) {
    uint64 which = args[0];
    switch (which) {
        // ...
        case KTEST_SEND_SIGUSR2:
            return sys_sigkill(args[1], SIGUSR2, 0);
    }
    return 0;
}
```

在发送时将pid置为-1
```c
//sys_sigkill()

//特殊处理: 内核发送的信号
if(signo == SIGUSR2) {
    p->signal.siginfos[signo].si_pid = -1;
    sigaddset(&p->signal.sigpending, signo);

    release(&p->lock);
    return 0;
}
```


### 5.3.4 SIGSTOP & SIGCONT
增加两种信号：`SIGSTOP` 和 `SIGCONT`。当前者被 delivered 到某个进程时，使其进入无限期暂停运行的状
态，直到 `SIGCONT` 被 delivered 到该进程。



```c
//do_signal()

if (signo == SIGSTOP) {
    sigdelset(&p->signal.sigpending, signo);
    p->need_stop = 1;
    continue;
}

if (signo == SIGCONT) {
    sigdelset(&p->signal.sigpending, signo);
    // 如果进程已经被SIGSTOP停止，恢复它
    if (p->state == STOPPED) {
        p->state = RUNNABLE;
    }
    // 清除need_stop标志，防止进程在返回用户态时被停止
    p->need_stop = 0;
    // 清除所有等待的SIGSTOP信号
    if (sigismember(&p->signal.sigpending, SIGSTOP)) {
        sigdelset(&p->signal.sigpending, SIGSTOP);
    }
    continue;
}
```


```c
//sys_sigkill()

// 特殊处理SIGSTOP信号
if (signo == SIGSTOP) {
    // SIGSTOP不能被阻塞或忽略
    // 不直接设置进程状态为STOPPED，而是设置pending信号
    // 让进程在do_signal中处理SIGSTOP信号时自行停止
    sigaddset(&p->signal.sigpending, signo);

    // 如果进程正在睡眠，唤醒它让它处理信号
    if (p->state == SLEEPING) {
        p->state = RUNNABLE;
        add_task(p);
    }
    
    debugf("proc %d received SIGSTOP via sys_sigkill", p->pid);
    release(&p->lock);
    return 0;
}

// 特殊处理SIGCONT信号
if (signo == SIGCONT) {
    sigaddset(&p->signal.sigpending, signo);
    // 清除need_stop标志，防止进程在返回用户态时被停止
    p->need_stop = 0;
    
    // 如果进程已经被SIGSTOP停止，恢复它
    if (p->state == STOPPED) {
        p->state = RUNNABLE;
        add_task(p);
    }
    
    // 清除所有等待的SIGSTOP信号
    if (sigismember(&p->signal.sigpending, SIGSTOP)) {
        sigdelset(&p->signal.sigpending, SIGSTOP);
    }
    
    release(&p->lock); // 释放在findByPid中获取的锁
    return 0;
}

```


## 测试截图

### 5.3.2 SIGINFO

（这里需要一张上板截图）

### 5.3.4 SIGSTOP & SIGCONT

![05ce69f292f003666ca69086c819b74](D:\FinresandPheoton\2025Spring\OS\OS-Project-Signal\assets\05ce69f292f003666ca69086c819b74.png)