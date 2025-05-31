#ifndef SIGCHLD_TEST_C
#define SIGCHLD_TEST_C

#include "../lib/user.h"


static volatile int child_exit_code = -1;
static volatile int chld_handler_called = 0;

void sigchld_handler(int signo, siginfo_t* info, void* context) {
    assert(signo == SIGCHLD);
    assert(info != 0);
    assert(info->si_signo == SIGCHLD);
    assert(info->si_pid > 0);

    int status = 0;
    int pid = wait(0, &status);  // 回收子进程
    assert(pid == info->si_pid);
    child_exit_code = status;
    chld_handler_called = 1;

    fprintf(1, "[SIGCHLD handler] child %d exited with code %d\n", pid, status);
}

void sigchldtest(char* s) {
    sigaction_t sa = {
        .sa_sigaction = sigchld_handler,
        .sa_restorer = sigreturn,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, 0);

    int pid = fork();
    if (pid == 0) {
        // 子进程，退出码为123
        sleep(3);
        exit(123);
    } else {
        // 父进程，等待 SIGCHLD 处理函数完成
        for (int i = 0; i < 50; i++) {
            if (chld_handler_called) break;
            sleep(1);
        }

        assert(chld_handler_called == 1);
        assert(child_exit_code == 123);

        fprintf(1, "[parent] received SIGCHLD for pid %d, exit code = %d\n", pid, child_exit_code);
    }
}

#endif /* SIGCHLD_TEST_C */ 