# Lab6: Copy-on-Write FOrk for xv6

## Implement copy-on write

1. 在kernel/riscv.h中增加字段：

    ```c
    #define PTE_COW (1L << 8)
    ```

2. 修改kernel/vm.c中的uvmcopy函数：

    ```c
    int
    uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
    {
        pte_t *pte;
        uint64 pa, i;
        uint flags;

        for(i = 0; i < sz; i += PGSIZE){
            if((pte = walk(old, i, 0)) == 0)
                panic("uvmcopy: pte should exist");
            if((*pte & PTE_V) == 0)
                panic("uvmcopy: page not present");

            *pte = (*pte&~PTE_W)|PTE_COW;

            pa = PTE2PA(*pte);
            flags = PTE_FLAGS(*pte);

            inc_ref(pa);

            if(mappages(new, i, PGSIZE, pa, flags) != 0)
                goto err;
        }
        return 0;

        err:
        uvmunmap(new, 0, i / PGSIZE, 1);
        return -1;
    }
    ```

3. 在kernel/defs.h中声明函数walk：

    ```c
    pte_t*          walk(pagetable_t, uint64, int);
    ```

4. 更改kernel/trap.c中的usertrap函数，定义handle_cow_fault函数，在kernel/def.h中增加函数声明：

    ```c
    else if(r_scause() == 0xf) {
        if(handle_cow_fault(p->pagetable, r_stval()))
        p->killed = 1;
    }
    ```

    ```c
    int handle_cow_fault(pagetable_t pagetable, uint64 va) {
        if(va >= MAXVA)
            return -1;
        
        uint64 mem;
        pte_t* pte;

        if(!(pte=walk(pagetable, va, 0)))
            return -1;
        if(!(*pte&PTE_V) || !(*pte&PTE_U) || !(*pte&PTE_COW))
            return -1;
        if(!(mem=(uint64)kalloc()))
            return -1;

        memmove((void*)mem, (const void*)PTE2PA(*pte), PGSIZE);

        kfree((void*)PTE2PA(*pte));

        *pte = PA2PTE(mem) | (PTE_V|PTE_U|PTE_R|PTE_W|PTE_X);

        return 0;
    }
    ```

    ```c
    pte_t*          walk(pagetable_t, uint64, int);
    ```

5. 修改kernel/kalloc.c，增加引用计数功能，更改kalloc、kfree、freerange函数：

    ```c
    int ref_count[(PHYSTOP-KERNBASE)/PGSIZE];
    ```

    ```c
    void
    freerange(void *pa_start, void *pa_end)
    {
        char *p;
        p = (char*)PGROUNDUP((uint64)pa_start);
        for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
            ref_count[((uint64)p-KERNBASE)/PGSIZE]++;
            kfree(p);
            //ref_count[((uint64)p-KERNBASE)/PGSIZE]--;
        }
    }
    ```

    ```c
    void inc_ref(uint64 pa) {
        acquire(&kmem.lock);
        if(pa>=PHYSTOP || !ref_count[((uint64)pa-KERNBASE)/PGSIZE])
            panic("inc_ref");
        ref_count[((uint64)pa-KERNBASE)/PGSIZE]++;
        release(&kmem.lock);
    }
    ```

    ```c
    void
    kfree(void *pa)
    {
        uint64 tmp;
        struct run *r;

        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
            panic("kfree");

        acquire(&kmem.lock);
        if(!ref_count[((uint64)pa-KERNBASE)/PGSIZE])
            panic("kfree ref");
        tmp = --ref_count[((uint64)pa-KERNBASE)/PGSIZE];
        release(&kmem.lock);

        if(tmp > 0)
            return;

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run*)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    }
    ```

    ```c
    void *
    kalloc(void)
    {
        struct run *r;

        acquire(&kmem.lock);
        r = kmem.freelist;
        if(r) {
            kmem.freelist = r->next;
            if(ref_count[((uint64)r-KERNBASE)/PGSIZE])
            panic("kalloc ref");
            ref_count[((uint64)r-KERNBASE)/PGSIZE]++;
        }
        release(&kmem.lock);

        if(r)
            memset((char*)r, 5, PGSIZE); // fill with junk
        return (void*)r;
    }
    ```

6. 在kernel/def.h中增加函数inc_ref定义：

    ```c
    pte_t*          walk(pagetable_t, uint64, int);
    ```

7. 更改kernel/vm.c中的copyout函数：

    ```c
    int
    copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
    {
        uint64 n, va0, pa0;
        pte_t* pte;

        while(len > 0){
            va0 = PGROUNDDOWN(dstva);

            if(va0 >= MAXVA)
                return -1;

            if(!(pte=walk(pagetable, va0, 0)))
                return -1;

            if(!(*pte&PTE_V) || !(*pte&PTE_U))
                return -1;

            if(*pte&PTE_COW)
                if(handle_cow_fault(pagetable, va0))
                    return -1;

            pa0 = PTE2PA(*pte);

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
