# Lab5: xv6 lazy page allocation

## Eliminate allocation from sbrk()

1. 更改kernel/sysproc.c中的sys_sbrk函数：

    ```c
    uint64
    sys_sbrk(void)
    {
        int addr;
        int n;

        if(argint(0, &n) < 0)
            return -1;
        addr = myproc()->sz;
        /* if(growproc(n) < 0)
            return -1; */
        return addr;
    }
    ```
