# Thinking

2022_05_17_10:09

## RISC-V assembly(easy)

见answers-traps.txt

## Backtrace(moderate)

1. 在`kernel/defs.h`中添加函数声明`void backstrace(void)`

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

3. 在`kernel/printf.h`中定义`backstrace`函数：

    ```c
    void
    backstrace(void) {
        uint64 fp = r_fp();
        uint64 end = PGROUNDUP(fp); // end >= fp

        printf("backstarce:\n");

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

        backstrace(); // ←←←←←←←←

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

