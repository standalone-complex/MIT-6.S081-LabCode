# Lab10: mmap

## mmap

1. 注册系统调用：

    ```c
    // user.h
    void *mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset);
    int munmap(void* addr, uint64 length);

    // syscall.h
    #define SYS_mmap   22
    #define SYS_munmap 23

    // syscall.c
    extern uint64 sys_mmap(void);
    extern uint64 sys_munmap(void);

    [SYS_mmap]    sys_mmap,
    [SYS_munmap]  sys_munmap,
    ```

    ```perl
    # usys.pl
    entry("mmap");
    entry("munmap");
    ```

    ```makefile
    $U/_mmaptest\
    ```

2. 定义结构体VMA，宏NVMA，增加struct proc字段：

    ```c
    // param.h
    #define NVMA         16

    // proc.h
    struct VMA {
        uint64 addr;
        uint64 length;
        int prot;
        int flags;
        struct file *file;
    };

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
        struct trapframe *trapframe; // data page for trampoline.S
        struct context context;      // swtch() here to run process
        struct file *ofile[NOFILE];  // Open files
        struct inode *cwd;           // Current directory
        char name[16];               // Process name (debugging)
        struct VMA vmas[NVMA];
    };
    ```

3. 定义函数sys_mmap：

    ```c
    uint64 sys_mmap(void) {
        uint64 addr, length, offset;
        int prot, flags, fd;
        struct file *file;
        struct proc *p = myproc();

        if(argaddr(0, &addr) || argaddr(1, &length) || argint(2, &prot)
        || argint(3, &flags) || argfd(4, &fd, &file) || argaddr(5, &offset))
            return -1;

        if(!file->writable)
            if(flags==MAP_SHARED && (prot&PROT_WRITE))
                return -1;

        for(int i = 0; i<NVMA; ++i) {
            if(!p->vmas[i].length) {
                p->vmas[i].addr = p->sz;
                p->vmas[i].length = length;
                p->vmas[i].prot  = prot;
                p->vmas[i].flags = flags;
                p->vmas[i].file = filedup(file);
                p->sz += length;

                return p->vmas[i].addr;
            }
        }

        panic("no free VMA");
    }
    ```

4. 修改函数uvmunmap、uvmcopy：

    ```c
    /* if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped"); */

    if((*pte & PTE_V) == 0)
      continue;
    ```

5. 修改函数usertrap：

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
    } else if(r_scause()==13 || r_scause()==15) {
        uint64 va = r_stval();
        struct VMA *vma = 0;

        for(int i = 0; i<NVMA; ++i) {
            if(p->vmas[i].addr <= va
            && va < p->vmas[i].addr+p->vmas[i].length) {
                vma = &(p->vmas[i]);
                break;
            }
        }

        if(!vma)
            p->killed = 1;
        else {
            uint64 offset = va - vma->addr;
            uint64 pa = (uint64)kalloc();

            if(!pa)
                p->killed = 1;
            else {
                int flags = PTE_U;

                memset((void*)pa, 0, PGSIZE);

                ilock(vma->file->ip);
                readi(vma->file->ip, 0, pa, offset, PGSIZE);
                iunlock(vma->file->ip);

                if(vma->prot&PROT_READ)
                    flags |= PTE_R;
                if(vma->prot&PROT_WRITE)
                    flags |= PTE_W;
                if(vma->prot&PROT_EXEC)
                    flags |= PTE_X;
                
                if(mappages(p->pagetable, va, PGSIZE, pa, flags)) {
                    kfree((void*)pa);
                    p->killed = 1;
                }
            }
        }
    } else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
    }
    ```

6. 定义函数sys_munmap：

    ```c
    uint64 sys_munmap(void) {
    struct proc *p = myproc();
    uint64 addr, length;

    if(argaddr(0, &addr) || argaddr(1, &length))
        return -1;

    for(int i = 0; i<NVMA; ++i) {
        if(p->vmas[i].addr == addr) {
            p->vmas[i].addr += length;
            p->vmas[i].length -= length;
            if(p->vmas[i].flags == MAP_SHARED)
                filewrite(p->vmas[i].file, addr, length);
            uvmunmap(p->pagetable, addr, length/PGSIZE, 1);
            if(!p->vmas[i].length)
                fileclose(p->vmas[i].file);
            return 0;
        }
    }

    return -1;
    }
    ```

7. 增加fork、exit字段：

    ```c
    // fork
    for(int i = 0; i<NVMA; ++i) {
        if(p->vmas[i].length) {
            memmove(&np->vmas[i], &p->vmas[i], sizeof(p->vmas[i]));
            filedup(p->vmas[i].file);
        }
    }

    // exit
    for(int i = 0; i<NVMA; ++i) {
        if(p->vmas[i].length) {
            if(p->vmas[i].flags == MAP_SHARED)
                filewrite(p->vmas[i].file, p->vmas[i].addr, p->vmas[i].length);
            uvmunmap(p->pagetable, p->vmas[i].addr, p->vmas[i].length/PGSIZE, 1);
            p->vmas[i].length = 0;
        }
    }
    ```

8. 根据编译器提示增加头文件
