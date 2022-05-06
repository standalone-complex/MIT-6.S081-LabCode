#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
    int argv_idx = 0, begin_idx = 0, end_idx = 0, len = 0;
    char* exec_argv[MAXARG] = {0};
    char buf_argv[512] = {0};
    
    // 从第三个参数开始是需要的命令参数
    for(int i = 1; i<argc; ++i) {
        exec_argv[argv_idx++] = argv[i];
    }

    while(1) {
        begin_idx = end_idx;

        // 从标准输入取得一个参数
        while(1) {
            // 读一个字节
            len = read(0, buf_argv+end_idx, 1);

            // 如果读完
            if(!len || buf_argv[end_idx] == '\n') {
                buf_argv[end_idx] = 0;
                break;
            }
            
            ++end_idx;
        }

        // 读完就跳出
        if(len == 0)
            break;

        if(begin_idx == end_idx++) {
            continue;
        }

        exec_argv[argv_idx++] = buf_argv + begin_idx;
    }

    if(fork()) {
        // 父进程
        wait(0);
    } else {
        // 子进程
        exec(argv[1], exec_argv);
    }

    exit(0);
}