#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child_process(int[]);

int main(int argc, char* argv[]) {
    int _pipe[2]; // 右通道

    pipe(_pipe);

    if(fork()) {
        // 父进程
        close(_pipe[0]); // 关闭右通道读端
        for(int i = 2; i<=35; ++i) {
            write(_pipe[1], &i, sizeof(int)); // 向右通道写入2~35
        }

        close(_pipe[1]); // 关闭右通道写端
        wait(0);
    } else {
        // 子进程
        child_process(_pipe);
    }

    exit(0);
}

void child_process(int _pipe[]) {
    int prime, next;
    int child_pipe[2];
    
    close(_pipe[1]); // 关闭左通道写端

    // 从左通道读一个整型
    if(!read(_pipe[0], &prime, sizeof(int))) {
        close(_pipe[0]);
        exit(0);
    }

    printf("prime %d\n", prime);

    pipe(child_pipe);

    if(fork()) {
        // 父进程
        close(child_pipe[0]); // 关闭右通道读端
        
        while(1) {
            if(!read(_pipe[0], &next, sizeof(int))) {
                close(_pipe[0]); // 关闭左通道读端
                close(child_pipe[1]); // 关闭右通道写端
                wait(0);
                exit(0);
            }

            if(next%prime) {
                write(child_pipe[1], &next, sizeof(int));
            }
        }
    } else {
        // 子进程
        close(_pipe[0]);
        child_process(child_pipe);
    }
}