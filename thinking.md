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
        myproc()->sz += n;

        return addr;
    }
    ```

## Lazy allocation

1. 修改kernel/trap.c中的usertrap函数：

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
    } else if(r_scause() == 13 || r_scause() == 15) {
        mappages(p->pagetable, PGROUNDDOWN(r_stval()), PGSIZE, (uint64)kalloc(), PTE_U|PTE_W|PTE_R|PTE_X);
    } else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
    }
    ```
