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

## Lazytests and Usertests

1. 修改kernel/sysproc.c中的sys_sbrk函数：

    ```c
    uint64
    sys_sbrk(void)
    {
        int addr;
        int n;
        uint64 newaddr;

        if(argint(0, &n) < 0)
            return -1;
        addr = myproc()->sz;
        newaddr = myproc()->sz + n;

        if(newaddr >= MAXVA)
            return addr;

        if(n < 0) {
            if(newaddr > addr) {
                newaddr = 0;
                uvmunmap(myproc()->pagetable, 0, PGROUNDUP(addr)/PGSIZE, 1);
            } else
                uvmunmap(myproc()->pagetable, PGROUNDUP(newaddr), (PGROUNDUP(addr)-PGROUNDUP(newaddr))/ PGSIZE, 1);
        }

        myproc()->sz += n;

        return addr;
    }
    ```

2. 修改kernel/trap.c中的usertrap函数：

    ```c
    if(r_scause() == 8) {
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
        uint64 pa = (uint64)kalloc();
        
        if(r_stval() >= p->sz)
            p->killed = 1;
        else if(pa == 0)
            p->killed = 1;
        else if(r_stval()<p->trapframe->sp)
            p->killed = 1;
        else
            mappages(p->pagetable, PGROUNDDOWN(r_stval()), PGSIZE, pa, PTE_U|PTE_W|PTE_R|PTE_X);
    } else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
    }
    ```

3. 修改kernel/vm.c：

    ```c
    #include "spinlock.h"
    #include "proc.h"
    ```

    ```c
    void
    uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
    {
        uint64 a;
        pte_t *pte;

        if((va % PGSIZE) != 0)
            panic("uvmunmap: not aligned");

        for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
            if((pte = walk(pagetable, a, 0)) == 0)
                continue; //panic("uvmunmap: walk");
            if((*pte & PTE_V) == 0)
                continue; // panic("uvmunmap: not mapped");
            if(PTE_FLAGS(*pte) == PTE_V)
                panic("uvmunmap: not a leaf");
            if(do_free){
                uint64 pa = PTE2PA(*pte);
                kfree((void*)pa);
            }
            *pte = 0;
        }
    }
    ```

    ```c
    int
    uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
    {
        pte_t *pte;
        uint64 pa, i;
        uint flags;
        char *mem;

        for(i = 0; i < sz; i += PGSIZE){
            if((pte = walk(old, i, 0)) == 0)
                continue; // panic("uvmcopy: pte should exist");
            if((*pte & PTE_V) == 0)
                continue; // panic("uvmcopy: page not present");
            pa = PTE2PA(*pte);
            flags = PTE_FLAGS(*pte);
            if((mem = kalloc()) == 0)
                goto err;
            memmove(mem, (char*)pa, PGSIZE);
            if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
                kfree(mem);
                goto err;
            }
        }
        return 0;

        err:
        uvmunmap(new, 0, i / PGSIZE, 1);
        return -1;
    }
    ```

    ```c
    int
    copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
    {
        uint64 n, va0, pa0;

        while(len > 0){
            va0 = PGROUNDDOWN(dstva);
            pa0 = walkaddr(pagetable, va0);
            if(pa0 == 0) {
                if(dstva >= myproc()->sz)
                    return -1;
                uint64 mem = (uint64)kalloc();
                if(!mem)
                    return -1;
                pa0 = mem;
                memset((void*)mem, 0, PGSIZE);
                mappages(pagetable, va0, PGSIZE, pa0, PTE_R|PTE_W|PTE_U);
            }
            n = PGSIZE - (dstva - va0);
            if(n > len)
                n = len;
            memmove((void *)(pa0 + (dstva - va0)), src, n);

            len -= n;
            src += n;
            dstva = va0 + PGSIZE;
        }
        return 0;
    }
    ```

    ```c
    int
    copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
    {
        uint64 n, va0, pa0;

        while(len > 0){
            va0 = PGROUNDDOWN(srcva);
            pa0 = walkaddr(pagetable, va0);
            if(pa0 == 0) {
                if(srcva >= myproc()->sz)
                    return -1;
                uint64 mem = (uint64)kalloc();
                if(!mem)
                    return -1;
                pa0 = mem;
                memset((void*)mem, 0, PGSIZE);
                mappages(pagetable, va0, PGSIZE, pa0, PTE_R|PTE_W|PTE_U);
            }
            n = PGSIZE - (srcva - va0);
            if(n > len)
                n = len;
            memmove(dst, (void *)(pa0 + (srcva - va0)), n);

            len -= n;
            dst += n;
            srcva = va0 + PGSIZE;
        }
        return 0;
    }
    ```
