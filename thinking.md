# Lab8: locks

## Memory allocator

1. 修改`kmem`结构体：

    ```c
    struct {
        struct spinlock lock[NCPU];
        struct run *freelist[NCPU];
    } kmem;
    ```

2. 修改函数`kinit`：

    ```c
    void
    kinit()
    {
        for(int i = 0; i<NCPU; ++i)
            initlock(&kmem.lock[i], "kmem");
        freerange(end, (void*)PHYSTOP);
    }
    ```

3. 修改函数`kfree`：

    ```c
    void
    kfree(void *pa)
    {
        struct run *r;

        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
            panic("kfree");

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run*)pa;

        push_off();
        acquire(&kmem.lock[cpuid()]);
        r->next = kmem.freelist[cpuid()];
        kmem.freelist[cpuid()] = r;
        release(&kmem.lock[cpuid()]);
        pop_off();
    }
    ```

4. 修改函数`kalloc`：

    ```c
    void *
    kalloc(void)
    {
        struct run *r;

        push_off();
        for(int i = cpuid(), j = 0; j<NCPU; j++, i = (i+1)%NCPU) {
            acquire(&kmem.lock[i]);

            r = kmem.freelist[i];
            if(r) {
            kmem.freelist[i] = r->next;
            release(&kmem.lock[i]);
            break;
            }

            release(&kmem.lock[i]);
        }
        pop_off();

        if(r)
            memset((char*)r, 5, PGSIZE); // fill with junk
        return (void*)r;
    }
    ```
