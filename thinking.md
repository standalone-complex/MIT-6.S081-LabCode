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

5. 在uthread_switch.S中增加字段：

    ```asm
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
    ```

## Using threads

1. 定义全局数组locks：

    ```c
    pthread_mutex_t locks[NBUCKET];
    ```

2. 在main中增加字段初始化互斥量：

    ```c
    for(int i = 0; i<NBUCKET; i++) 
        pthread_mutex_init(&locks[i], NULL);
    ```

3. 在put中增加字段：

    ```c
    static 
    void put(int key, int value)
    {
        int i = key % NBUCKET;
        pthread_mutex_lock(&locks[i]);
        // is the key already present?
        struct entry *e = 0;
        for (e = table[i]; e != 0; e = e->next) {
            if (e->key == key)
            break;
        }
        if(e){
            // update the existing key.
            e->value = value;
        } else {
            // the new is new.
            insert(key, value, &table[i], table[i]);
        }
        pthread_mutex_unlock(&locks[i]);
    }
    ```

## Barrier

1. 增加barrier中字段：

    ```c
    static void 
    barrier()
    {
        // YOUR CODE HERE
        //
        // Block until all threads have called barrier() and
        // then increment bstate.round.
        //
        pthread_mutex_lock(&bstate.barrier_mutex);
        bstate.nthread++;

        if(nthread == bstate.nthread) {
            bstate.round++;
            bstate.nthread = 0;
            pthread_cond_broadcast(&bstate.barrier_cond);
        } else
            pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);

        pthread_mutex_unlock(&bstate.barrier_mutex);
    }
    ```
