# Lab7: Multithreading

## Uthread: switching between threads

1. 在结构`struct thread`中增加字段：

    ```c
    struct thread {
        char       stack[STACK_SIZE]; /* the thread's stack */
        int        state;             /* FREE, RUNNING, RUNNABLE */
        struct  context thread_context;
    };
    ```

2. 在函数`thread_scheduler`中添加`thread_switch`函数：

    ```c
    thread_switch((uint64)&t->thread_context, (uint64)&current_thread->thread_context);
    ```

3. 在函数`thread_create`中设定第一次调度的指令指针和栈指针地址：

    ```c
    t->thread_context.ra = (uint64)func;
    t->thread_context.sp = (uint64)t->stack + STACK_SIZE;
    ```

4. 增加头文件：

    ```c
    #include "kernel/spinlock.h"
    #include "kernel/param.h"
    #include "kernel/riscv.h"
    #include "kernel/proc.h"
    ```
