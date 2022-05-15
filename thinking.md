# Thinking

## Print a page table

接收一个pagetable_t并把它指向的页表打印。

1. 在`kernel/def.h`中增加函数声明`void vmprint(void)`并在`kernel/vm.c`中定义：

    ```c
    void vmprint(pagetable_t p) {
        printf("page table %p\n", p);
        _vmprint(p, 0);
    }

    // vmprint的递归实现
    void _vmprint(pagetable_t p, int level) {
        uint64 pa;
        
        for(int i = 0; i<512; ++i) {
            if(p[i] & PTE_V) {
            pa = PTE2PA(p[i]);
            switch(level) {
            case 0:
                printf("..%d: pte %p pa %p\n", i, p[i], pa);
                _vmprint((pagetable_t)pa, level+1);
                break;
            case 1:
                printf(".. ..%d: pte %p pa %p\n", i, p[i], pa);
                _vmprint((pagetable_t)pa, level+1);
                break;
            case 2:
                printf(".. .. ..%d: pte %p pa %p\n", i, p[i], pa);
                break;
            }
            }
        }
    }
    ```

2. 在`kernel/exec.c`函数中增加如下字段：

    ```c
    p->trapframe->epc = elf.entry;  // initial program counter = main
    p->trapframe->sp = sp; // initial stack pointer
    proc_freepagetable(oldpagetable, oldsz);

    // ↓↓↓↓↓↓↓↓
    if(p->pid == 1)
        vmprint(p->pagetable);
    // ↑↑↑↑↑↑↑↑

    return argc; // this ends up in a0, the first argument to main(argc, argv)
    ```

## A kernel page table per processor

给进程增加一个内核级用户页表，同时拥有用户内存映射和内核内存映射，便于在用户陷入内核时对用户内存进行操作

1. 给在`kernel/proc.h`结构体`struct proc`增加字段`pagetable_t k_pagetable`：

    ```c
    // Per-process state
    struct proc {
        struct spinlock lock;

        // p->lock must be held when using these:
        enum procstate state;        // Process state
        struct proc *parent;         // Parent process
        void *chan;                  // If non-zero, sleeping on chan
        int killed;                  // If non-zero, have been killed
        int xstate;                  // Exit status to be returned to parent's wait
        int pid;                     // Process ID

        // these are private to the process, so p->lock need not be held.
        uint64 kstack;               // Virtual address of kernel stack
        uint64 sz;                   // Size of process memory (bytes)
        pagetable_t pagetable;       // User page table
        pagetable_t k_pagetable;     // Kernel user page table // ←←←←←←←←
        struct trapframe *trapframe; // data page for trampoline.S
        struct context context;      // swtch() here to run process
        struct file *ofile[NOFILE];  // Open files
        struct inode *cwd;           // Current directory
        char name[16];               // Process name (debugging)
    };
    ```

    在`kernel/vm.c`定义函数`pagetable_t _kvminit(void)`并在`kernel/def.h`中增加`pagetable_t _kvminit(void)`函数声明：

    ```c
    pagetable_t _kvminit() {
        pagetable_t k_pagetable = (pagetable_t)kalloc();
        memset(k_pagetable, 0, PGSIZE);
        
        if(mappages(k_pagetable, UART0, PGSIZE, UART0, PTE_R|PTE_W))
            panic("mappages");
        if(mappages(k_pagetable, VIRTIO0, PGSIZE, VIRTIO0, PTE_R|PTE_W))
            panic("mappages");
        if(mappages(k_pagetable, PLIC, 0x400000, PLIC, PTE_R|PTE_W))
            panic("mappages");
        if(mappages(k_pagetable, KERNBASE, (uint64)etext-KERNBASE, KERNBASE, PTE_R|PTE_X))
            panic("mappages");
        if(mappages(k_pagetable, (uint64)etext, PHYSTOP-(uint64)etext, (uint64)etext, PTE_R|PTE_W))
            panic("mappages");
        if(mappages(k_pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline, PTE_R|PTE_X))
            panic("mappages");

        return k_pagetable;
    }
    ```

    更改`kernel/vm.c`中的`kvminit`函数：

    ```c
    void
    kvminit()
    {
        kernel_pagetable = _kvminit();
        if(mappages(kernel_pagetable, CLINT, 0x10000, CLINT, PTE_R|PTE_W))
            panic("mappages");
    }
    ```

2. 删除`kernel/main.c`中的`procinit()`函数调用、`kernel/def.h`中的`void procinit(void)`函数声明、`kernel/proc.c`中的`procinit`函数定义，在`kernel/proc.c`中的`struct proc* allocproc(void)`函数增加以下字段：

    ```c
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // ↓↓↓↓↓↓↓↓
    // An empty kernel user page table
    p->k_pagetable = _kvminit();
    if(p->k_pagetable == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // map user's kernel stack at kernel user page table
    uint64 pa = (uint64)kalloc();
    uint64 va = KSTACK(0);
    if(mappages(p->k_pagetable, va, PGSIZE, pa, PTE_R|PTE_W))
        panic("mappages");
    p->kstack = va;
    // ↑↑↑↑↑↑↑↑

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;
    ```

3. 在`kernel/proc.c`的`void scheduler(void)`函数中增加以下字段：

    ```c
    p->state = RUNNING;
    c->proc = p;

    // ↓↓↓↓↓↓↓↓
    w_satp(MAKE_SATP(p->k_pagetable));
    sfence_vma();

    swtch(&c->context, &p->context);

    kvminithart();
    // ↑↑↑↑↑↑↑↑

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    found = 1;
    ```

4. 在`kernel/proc.c`中定义`void proc_freekpagetable(pagetable_t pagetable, uint64 sz)`函数，在`kernel/def.h`中增加`void proc_freekpagetable(pagetable_t, uint64)`函数声明，并在`void freeproc(struct proc* p)`函数中增加以下字段：

    ```c
    void proc_freekpagetable(pagetable_t pagetable, uint64 sz) {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmunmap(pagetable, (uint64)etext, (PHYSTOP-(uint64)etext)/PGSIZE, 0);
        uvmunmap(pagetable, KERNBASE, ((uint64)etext-KERNBASE)/PGSIZE, 0);
        uvmunmap(pagetable, PLIC, 0x400000/PGSIZE, 0);
        uvmunmap(pagetable, VIRTIO0, 1, 0);
        uvmunmap(pagetable, UART0, 1, 0);
        // uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 0);

        uvmfree(pagetable, 0);
    }
    ```

    ```c
    // free a proc structure and the data hanging from it,
    // including user pages.
    // p->lock must be held.
    static void
    freeproc(struct proc *p)
    {
        if(p->trapframe)
            kfree((void*)p->trapframe);
        p->trapframe = 0;
        // ↓↓↓↓↓↓↓↓
        if(p->kstack)
            uvmunmap(p->k_pagetable, p->kstack, 1, 1);
        p->kstack = 0;
        // ↑↑↑↑↑↑↑
        if(p->pagetable)
            proc_freepagetable(p->pagetable, p->sz);
        p->pagetable = 0;
        // ↓↓↓↓↓↓↓↓
        if(p->k_pagetable)
            proc_freekpagetable(p->k_pagetable, p->sz);
        p->k_pagetable = 0;
        // ↑↑↑↑↑↑↑↑
        p->sz = 0;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->chan = 0;
        p->killed = 0;
        p->xstate = 0;
        p->state = UNUSED;
    }
    ```

5. 在`kernel/vm.c`中增加以下字段，并对`uint64 kvmpa(uint64 pa)`函数做如下改动：

    ```c
    #include "param.h"
    #include "types.h"
    #include "memlayout.h"
    #include "elf.h"
    #include "riscv.h"
    #include "defs.h"
    #include "fs.h"
    // ↓↓↓↓↓↓↓↓
    #include "spinlock.h" 
    #include "proc.h"
    // ↑↑↑↑↑↑↑↑
    ```

    ```c
    uint64
    kvmpa(uint64 va)
    {
        uint64 off = va % PGSIZE;
        pte_t *pte;
        uint64 pa;
        
        pte = walk(myproc()->k_pagetable, va, 0); // ←←←←←←←←
        if(pte == 0)
            panic("kvmpa");
        if((*pte & PTE_V) == 0)
            panic("kvmpa");
        pa = PTE2PA(*pte);
        return pa+off;
    }
    ```
