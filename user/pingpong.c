#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    char buf[64];

    int pipe_1[2];
    int pipe_2[2];

    pipe(pipe_1);
    pipe(pipe_2);
    
    if(fork()) {
        // parent process
        write(pipe_1[1], "Hello ", 6);
        wait(0);
        printf("%d: received pong\n", getpid());
        read(pipe_2[0], buf, sizeof(8));
    } else {
        // child process
        read(pipe_1[0], buf, sizeof("Hello "));
        write(pipe_2[1], "World!\n", sizeof(8));
        printf("%d: received ping\n", getpid());
        exit(0);
    }

    close(pipe_1[0]);
    close(pipe_2[0]);
    close(pipe_2[1]);
    close(pipe_1[1]);

    exit(0);
}