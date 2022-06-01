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
