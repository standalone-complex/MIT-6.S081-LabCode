# Thinking

2022_05_17_10:09

## RISC-V assembly(easy)

见answers-traps.txt

## Backtrace(moderate)

1. 在`kernel/defs.h`中添加函数声明`void backtrace(void)`

2. 在`kernel/riscv.h`中添加以下字段：

    ```c
    // get frame pointer
    static inline uint64
    r_fp()
    {
        uint64 x;
        asm volatile("mv %0, s0" : "=r" (x));
        return x;
    }
    ```

3. 在`kernel/printf.h`中定义`backtrace`函数：

    ```c
    void
    backtrace(void) {
        uint64 fp = r_fp();
        uint64 end = PGROUNDUP(fp); // end >= fp

        printf("backtrace:\n");

        while(fp != end) {
            printf("%p\n", *(uint64*)((char*)fp-8));
            fp = *(uint64*)((void*)fp-16);
        }
    }
    ```

4. 在`kernel/sysproc.c`的`sys_sleep`函数中增加字段：

    ```c
    uint64
    sys_sleep(void)
        {
        int n;
        uint ticks0;

        backtrace(); // ←←←←←←←←

        if(argint(0, &n) < 0)
            return -1;
        acquire(&tickslock);
        ticks0 = ticks;
        while(ticks - ticks0 < n){
            if(myproc()->killed){
            release(&tickslock);
            return -1;
            }
            sleep(&ticks, &tickslock);
        }
        release(&tickslock);
        return 0;
    }
    ```

## Alarm(Hard)

### test0: invoke handler

1. 在`Makefile`中增加字段：

    ```makefile
    UPROGS=\
        $U/_cat\
        $U/_echo\
        $U/_forktest\
        $U/_grep\
        $U/_init\
        $U/_kill\
        $U/_ln\
        $U/_ls\
        $U/_mkdir\
        $U/_rm\
        $U/_sh\
        $U/_stressfs\
        $U/_usertests\
        $U/_grind\
        $U/_wc\
        $U/_zombie\
        $U/_alarmtest\ # ←←←←←←←←
    ```

2. 在`user/user.h`中增加字段：

    ```c
    int sigalarm(int, void (*)(void));
    int sigreturn(void);
    ```

3. 在`user/usys.pl`中增加字段：

    ```pl
    entry("sigalarm");
    entry("sigreturn");
    ```

4. 在`kernel/syscall.h`和`kernel/syscall.c`中增加字段：

    ```c
    #define SYS_sigalarm 22
    #define SYS_sigreturn 23
    ```

    ```c
    extern uint64 sys_sigalarm(void);
    extern uint64 sys_sigreturn(void);
    ```

    ```c
    [SYS_sigalarm] sys_sigalarm,
    [SYS_sigreturn]  sys_sigreturn,
    ```

5. 给`kernel/proc.h`中的`struct proc`结构增加字段：

    ```c
    int alarm;
    int count;
    uint64 callback;
    ```

6. 在`kernel/proc.c`中给`allocproc`函数增加字段：

    ```c
    p->alarm = 0;
    p->count = 0;
    p->callback = 0;
    ```

7. 在`kernel/sysproc.c`中增加`sigalarm`和`sigreturn`函数定义：

    ```c
    uint64 sys_sigalarm(void) {
        int arg1;
        uint64 arg2;
        struct proc* p = myproc();
        if(argint(0, &arg1) < 0)
            return -1;
        if(argaddr(1, &arg2) < 0)
            return -1;
        
        p->alarm = arg1;
        p->callback = arg2;

        return 0;
    }

    uint64 sys_sigreturn(void) {
        return 0;
    }
    ```

8. 在`kernel/trap.c`中增加字段：

    ```c
    if(r_scause() == 8){
        // system call

        if(p->killed)
            exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        p->trapframe->epc += 4;

        // an interrupt will change sstatus &c registers,
        // so don't enable until done with those registers.
        intr_on();

        syscall();
    } else if((which_dev = devintr()) != 0){
        // ok
        // ↓↓↓↓↓↓↓↓
        if(which_dev == 2) {
            if(p->alarm) {
                p->count = (p->count+1) % p->alarm;
                if(!p->count){
                    p->trapframe->epc = p->callback;
                }
            }
        }
        // ↑↑↑↑↑↑↑↑
    } else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
    }
    ```

### test1/test2(): resume interrupted code

1. 在`kernel/def.h`增加函数声明`void            savetrapframe(struct trapframe*, struct trapframe*)`，声明结构体`struct trapframe`

2. 在`kernel/trap.c`中增加函数定义：

    ```c
    void savetrapframe(struct trapframe* dst, struct trapframe* src) {
        dst->epc = src->epc;
        dst->kernel_hartid = src->kernel_hartid;
        dst->kernel_satp = src->kernel_satp;
        dst->kernel_sp = src->kernel_sp;
        dst->kernel_trap = src->kernel_trap;
        dst->a0 = src->a0;
        dst->a1 = src->a1;
        dst->a2 = src->a2;
        dst->a3 = src->a3;
        dst->a4 = src->a4;
        dst->a5 = src->a5;
        dst->a6 = src->a6;
        dst->a7 = src->a7;
        dst->ra = src->ra;
        dst->sp = src->sp;
        dst->gp = src->gp;
        dst->tp = src->tp;
        dst->t0 = src->t0;
        dst->t1 = src->t1;
        dst->t2 = src->t2;
        dst->t3 = src->t3;
        dst->t4 = src->t4;
        dst->t5 = src->t5;
        dst->t6 = src->t6;
        dst->s0 = src->s0;
        dst->s1 = src->s1;
        dst->s2 = src->s2;
        dst->s3 = src->s3;
        dst->s4 = src->s4;
        dst->s5 = src->s5;
        dst->s6 = src->s6;
        dst->s7 = src->s7;
        dst->s8 = src->s8;
        dst->s9 = src->s9;
        dst->s10 = src->s10;
        dst->s11 = src->s11;
    }
    ```

3. 给`kernel/proc.h`中的`struct proc`结构增加字段：

    ```c
    struct trapframe *trapframe_dup; // a context copy
    int callback_once_flag;
    ```

4. 给`kernel/proc.c`中的`allocproc`和`freeproc`函数增加字段：

    ```c
    // Allocate a trapframe_dup page.
    if((p->trapframe_dup = (struct trapframe *)kalloc()) == 0){
        release(&p->lock);
        return 0;
    }

    p->callback_once_flag = 0;
    ```

    ```c
    if(p->trapframe_dup)
        kfree((void*)p->trapframe_dup);
    p->trapframe_dup = 0;
    ```

5. 修改`sigreturn`和`usertrap`函数：

    ```c
    uint64 sys_sigreturn(void) {
        struct proc* p = myproc();

        savetrapframe(p->trapframe, p->trapframe_dup);
        p->callback_once_flag = 0;
        
        return 0;
    }
    ```

    ```c
    if(r_scause() == 8){
        // system call

        if(p->killed)
            exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        p->trapframe->epc += 4;

        // an interrupt will change sstatus &c registers,
        // so don't enable until done with those registers.
        intr_on();

        syscall();
    } else if((which_dev = devintr()) != 0){
        // ok
        if(which_dev == 2) {
            if(p->alarm && !p->callback_once_flag) {
                p->count = (p->count+1) % p->alarm;
                if(!p->count){
                    savetrapframe(p->trapframe_dup, p->trapframe);
                    p->callback_once_flag = 1;
                    p->trapframe->epc = p->callback;
                }
            }
        }
    } else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
    }
    ```
