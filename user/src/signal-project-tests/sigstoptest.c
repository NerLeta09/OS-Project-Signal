#ifndef SIGSTOP_TEST_C
#define SIGSTOP_TEST_C

#include "../../lib/user.h"

// 测试SIGSTOP和SIGCONT的基本功能
void sigstop_basic(char *s) {
    printf("Testing SIGSTOP and SIGCONT...\n");
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    } else if (pid == 0) {
        // 子进程
        printf("Child process started, pid = %d\n", getpid());
        
        // 循环打印，直到被终止
        for (int i = 0; i < 10; i++) {
            printf("Child: counting %d\n", i);
            sleep(1);
        }
        
        exit(0);
    } else {
        // 父进程
        // 等待子进程打印一些输出
        sleep(2);
        
        // 发送SIGSTOP信号暂停子进程
        printf("Parent: sending SIGSTOP to child %d\n", pid);
        sigkill(pid, SIGSTOP, 0);
        
        // 等待一段时间，此时子进程应该已经暂停
        sleep(3);
        printf("Child process should be stopped now\n");
        
        // 发送SIGCONT信号恢复子进程
        printf("Parent: sending SIGCONT to child %d\n", pid);
        sigkill(pid, SIGCONT, 0);
        
        // 等待子进程结束
        int status;
        wait(pid, &status);
        printf("Child exited with status %d\n", status);
    }
}

// 测试SIGSTOP不能被阻塞
void sigstop_block_test(char *s) {
    printf("Testing SIGSTOP cannot be blocked...\n");
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    } else if (pid == 0) {
        // 子进程
        printf("Child process started, pid = %d\n", getpid());
        
        // 尝试阻塞SIGSTOP信号
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGSTOP);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        
        printf("Child: attempted to block SIGSTOP\n");
        
        // 循环打印，即使SIGSTOP被"阻塞"，进程也应该被暂停
        for (int i = 0; i < 10; i++) {
            printf("Child: counting %d\n", i);
            sleep(1);
        }
        
        exit(0);
    } else {
        // 父进程
        // 等待子进程打印一些输出
        sleep(2);
        
        // 发送SIGSTOP信号暂停子进程，即使子进程尝试阻塞它
        printf("Parent: sending SIGSTOP to child %d even though it's blocked\n", pid);
        sigkill(pid, SIGSTOP, 0);
        
        // 等待一段时间，此时子进程应该已经暂停
        sleep(3);
        printf("Child process should be stopped now, despite blocking SIGSTOP\n");
        
        // 发送SIGCONT信号恢复子进程
        printf("Parent: sending SIGCONT to child %d\n", pid);
        sigkill(pid, SIGCONT, 0);
        
        // 等待子进程结束
        int status;
        wait(pid, &status);
        printf("Child exited with status %d\n", status);
    }
}

// 测试SIGCONT对于多次SIGSTOP的处理
void sigstop_multi_test(char *s) {
    printf("Testing multiple SIGSTOP followed by one SIGCONT...\n");
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        exit(1);
    } else if (pid == 0) {
        // 子进程
        printf("Child process started, pid = %d\n", getpid());
        
        // 循环打印，直到被终止
        for (int i = 0; i < 20; i++) {
            printf("Child: counting %d\n", i);
            sleep(1);
        }
        
        exit(0);
    } else {
        // 父进程
        // 等待子进程打印一些输出
        sleep(2);
        
        // 多次发送SIGSTOP信号
        printf("Parent: sending multiple SIGSTOP signals to child %d\n", pid);
        sigkill(pid, SIGSTOP, 0);
        sigkill(pid, SIGSTOP, 0);
        sigkill(pid, SIGSTOP, 0);
        
        // 等待一段时间，此时子进程应该已经暂停
        sleep(3);
        printf("Child process should be stopped now\n");
        
        // 只发送一次SIGCONT信号，应该能恢复子进程
        printf("Parent: sending one SIGCONT to child %d\n", pid);
        sigkill(pid, SIGCONT, 0);
        
        // 等待一段时间，确认子进程继续执行
        sleep(3);
        
        // 终止子进程
        printf("Parent: killing child %d\n", pid);
        sigkill(pid, SIGKILL, 0);
        
        // 等待子进程结束
        int status;
        wait(pid, &status);
        printf("Child exited with status %d\n", status);
    }
}

#endif /* SIGSTOP_TEST_C */ 