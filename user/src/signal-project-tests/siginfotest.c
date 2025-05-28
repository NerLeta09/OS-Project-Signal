#ifndef SIGINFO_TEST_C
#define SIGINFO_TEST_C

#include "../../lib/user.h"
#include "../../../os/ktest/ktest.h"

volatile int flag_info = 0;
volatile int expected_pid = -1;
static void siginfo_handler(int signo, siginfo_t *info, void* ctx2) {
    printf("Handle signo: \n");
    sleep(1);
    // fprintf(1, "handler4 triggered\n");

    printf("[handler] signo = %d\n", info->si_signo);
    printf("[handler] si_pid = %d\n", info->si_pid);
    printf("[handler] si_code = %d\n", info->si_code);
    printf("[handler] si_status = %d\n", info->si_status);
    printf("[handler] addr = %p\n", info->addr);

    assert(signo == info->si_signo);
    printf("[handler] si_signo match signo %d\n", signo);

    assert(expected_pid == info->si_pid);
    printf("[handler] si_pid match ppid %d\n", expected_pid);
    
    flag_info = 1;
}

// 测试SIGINFO功能能正常实现
void siginfo_basic(char *s) {
    printf("Testing SIGINFO...\n");

    int pid = fork();
    // 父进程向子进程发送SIGUSER0
    if (pid == 0) {
        // child process
        // Get
        expected_pid = getppid();
        sigaction_t sa = {
            .sa_sigaction = siginfo_handler,
            .sa_restorer = sigreturn,
        };
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR0, &sa, 0);  
        printf("Set signo handler = SIGUSER0\n");
        while(flag_info == 0);
        exit(104);
    } else {
        // parent process
        sleep(10);
        printf("Send signal to child: SIGUSR0\n");
        sigkill(pid, SIGUSR0, 0);
        int ret;
        wait(0, &ret);
        assert_eq(ret, 104);
    }
}

static void siginfo_khandler(int signo, siginfo_t *info, void* ctx2) {
    sleep(1);
    // fprintf(1, "handler4 triggered\n");

    printf("[handler] signo = %d\n", info->si_signo);
    printf("[handler] si_pid = %d\n", info->si_pid);
    printf("[handler] si_code = %d\n", info->si_code);
    printf("[handler] si_status = %d\n", info->si_status);
    printf("[handler] addr = %p\n", info->addr);

    assert(signo == info->si_signo);
    printf("[handler] si_signo match signo %d\n", signo);

    assert(expected_pid == -1);
    printf("[handler] si_pid: %d, signal sent by kernel\n", -1);
    
    flag_info = 1;
}

// 测试由内核发出的SIGINFO信号常数
void siginfo_kernel(char *s) {
    printf("Testing SIGINFO from kernel...\n");
    
    sigaction_t sa = {
        .sa_sigaction = siginfo_khandler,
        .sa_restorer = sigreturn,
    };
    sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, 0);  
        printf("Set signo handler = SIGUSER2\n");

    //触发系统调用：内核发送函数
    int ret = ktest(KTEST_SEND_SIGUSR2, (void *)(uint64)getpid(), 1);
    printf("return: %d\n", ret);
}

#endif /* SIGINFO_TEST_C */ 