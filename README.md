# CS302 Project: Signal

1. 请注意：Origin仓库为 https://github.com/yuk1i/SUSTechOS.git 
2. Origin会不定期更新，我们要从上面拉取更新
3. 而Project仓库(https://github.com/NerLeta09/OS-Project-Signal.git)才是我们的仓库，我们在这里完成Project
4. 有更改也请尽量不要往signals-codebase分支上提交。保持该分支与origin上同步

## Checkpoints 

### checkpoint1
> base checkpoint

实现sigaction系统调用，进程可以指定某个signal的处理方式为SIG_DFL、SIG_IGN或指定的sa_sigaction。
实现sigkill 系统调用，进程能向某个指定进程Sendsignal。
参照POSIX.1 规范中的2.1 Execution of signal handlers 以及 2.1 Signal Actions，在回退到用户空间时检
查Pending 的 signal，如果用户指定了处理函数，则在用户态跳转到signal 处理函数，并在其返回后恢复执行
signal handler 前的用户态状态。
1. 在每次从内核回退到用户态时检查PendingSignals。
2. 如果该Signal没有被Blocked或者不能被Blocked，执行(SIG_DFL、SIG_IGN) 或准备跳转到以前在sigac
tion 系统调用里指定的用户态处理函数。
3. 在用户栈上保存用户程序进入 signal handler 前的状态，即 ucontext 结构体，包含 PC 指针、GPRs
（architecture-specific register state）、以及 signal 相关的信息（即 signal mask）。架构相关寄存器的内容可
以从Trapframe 中获得。在 CPU 从用户态进入内核态时，所有架构相关的信息都被保存在了Trapframe
上。
4. 在用户栈上构造siginfo_t （暂时全为0即可）。该指针要求传入signal handler 的第二个参数。
5. 参照2.1 Execution of signal handlers，根据指定的处理方式 sigaction_t，修改当前进程的 signal mask。
6. 设置Trapframe，使得当 sret 后，用户态开始执行 signal 处理函数 sa_sigaction。你需要设置 signal
 handler 的三个参数：signo、用户指针 siginfo_t 以及用户指针 ucontext。并且，在 signal handler 退
出后执行sa_restorer 函数（即从用户态发起 sigreturn 系统调用）。
7. 用户从Signal Handler 中退出时，会调用 sigreturn 系统调用后，从用户栈上恢复以前保存的用户态状
态，使用户程序继续执行进入signalhandler 前的代码。

在完成上述步骤时，你需要实现sigaction、sigreturn、sigpending 等系统调用。
### checkpoint2
> base checkpoint

实现一个特殊的signal SIGKILL，它的行为如下：
1. 该signal 无法被 Blocked 或者 Ignored。
2. 该signal 无法被处理。
3. 该signal 会终止进程，退出代码为-10- SIGKILL。
### checkpoint3
> base checkpoint

实现signal 在 fork 和 exec 之间的行为。
1. 在fork 时，子进程继承父进程的signal处理方式(sigaction)、signal mask，但是清空所有 pending signal。
2. 在exec 时，子进程重置所有的signal处理方式为默认值，但是保留被手动指定为ignore的那些sigaction，
signal mask 以及 pending signal 不变。



