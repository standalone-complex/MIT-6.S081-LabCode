# Lab8: locks

两个作业都是修改数据结构将粗粒度锁替换为细粒度锁降低锁争用增加并行度

## Memory allocator

将唯一的空闲链表改为每个CPU一个，只有在当前CPU去其他CPU下的空闲链表取块时会产生锁争用

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

## Buffer cache

不要吝啬加锁，锁越少越粗争用越多，进入和退出临界区都要记得加锁操作

1. 宏定义哈希桶数量、哈希函数

    ```c
    #define NBUCKET 13
    #define TSH(x) (x%NBUCKET)
    ```

2. 定义结构体`bmem`，增加结构体`bcache`字段：

    ```c
    struct bmem {
        struct spinlock locks[NBUCKET];
        struct buf heads[NBUCKET];
    };

    struct {
        struct spinlock lock;
        struct buf buf[NBUF];

        // Linked list of all buffers, through prev/next.
        // Sorted by how recently the buffer was used.
        // head.next is most recent, head.prev is least.
        struct bmem HT;
    } bcache;
    ```

3. 修改函数`binit`：

    ```c
    void
    binit(void)
    {
        struct buf *b;

        initlock(&bcache.lock, "bcache");

        // init buckets' lock and init list struct
        for(int i = 0; i<NBUCKET; ++i) {
            initlock(&bcache.HT.locks[i], "bucket");

            bcache.HT.heads[i].prev = &bcache.HT.heads[i];
            bcache.HT.heads[i].next = &bcache.HT.heads[i];
        }

        // init every buf sleeplock add them to the first bucket
        for(b = bcache.buf; b < bcache.buf+NBUF; ++b) {
            initsleeplock(&b->lock, "buf");

            b->next = bcache.HT.heads[TSH(b->blockno)].next;
            b->prev = &bcache.HT.heads[TSH(b->blockno)];
            bcache.HT.heads[TSH(b->blockno)].next->prev = b;
            bcache.HT.heads[TSH(b->blockno)].next = b;
        }
    }
    ```

4. 修改函数bget：

    ```c
    static struct buf*
    bget(uint dev, uint blockno)
    {
        struct buf* b;
        struct buf* t = 0;
        uint min_tick = ~0;

        acquire(&bcache.HT.locks[TSH(blockno)]);
        // Is the block already cached?
        for(b = bcache.HT.heads[TSH(blockno)].next; b != &bcache.HT.heads[TSH(blockno)]; b = b->next)
            if(b->dev == dev && b->blockno == blockno){
                b->refcnt++;
                release(&bcache.HT.locks[TSH(blockno)]);
                acquiresleep(&b->lock);
                return b;
            }
        release(&bcache.HT.locks[TSH(blockno)]);

        // Not cached.
        // Recycle the least recently used (LRU) unused buffer.
        acquire(&bcache.lock);

        acquire(&bcache.HT.locks[TSH(blockno)]);
        // check agein. Is the block already cached?
        for(b = bcache.HT.heads[TSH(blockno)].next; b != &bcache.HT.heads[TSH(blockno)]; b = b->next)
            if(b->dev == dev && b->blockno == blockno){
                b->refcnt++;
                release(&bcache.HT.locks[TSH(blockno)]);
                acquiresleep(&b->lock);
                return b;
            }
        release(&bcache.HT.locks[TSH(blockno)]);

        // get LRU buf
        for(int i = 0; i<NBUCKET; ++i) {
            acquire(&bcache.HT.locks[i]);
            for(b = bcache.HT.heads[i].next; b != &bcache.HT.heads[i]; b = b->next)
                if(!b->refcnt && b->tick < min_tick) {
                    min_tick = b->tick;
                    t = b;
                }
            release(&bcache.HT.locks[i]);
        }

        if(t) {
            // if the bucket not same, delete from old and insert new bucket
            // pay attention to the lock!
            if(TSH(t->blockno) != TSH(blockno)) {
                acquire(&bcache.HT.locks[TSH(t->blockno)]);
                t->next->prev = t->prev;
                t->prev->next = t->next;
                release(&bcache.HT.locks[TSH(t->blockno)]);

                acquire(&bcache.HT.locks[TSH(blockno)]);
                t->next = bcache.HT.heads[TSH(blockno)].next;
                t->prev = &bcache.HT.heads[TSH(blockno)];
                bcache.HT.heads[TSH(blockno)].next->prev = t;
                bcache.HT.heads[TSH(blockno)].next = t;
                release(&bcache.HT.locks[TSH(blockno)]);
            }

            t->dev = dev;
            t->blockno = blockno;
            t->valid = 0;
            t->refcnt = 1;

            release(&bcache.lock);
            acquiresleep(&t->lock);
            return t;
        }

        panic("bget: no buffers");
    }
    ```

5. 修改函数brelse：

    ```c
    void
    brelse(struct buf *b)
    {
        if(!holdingsleep(&b->lock))
            panic("brelse");

        releasesleep(&b->lock);

        acquire(&bcache.HT.locks[TSH(b->blockno)]);
        b->refcnt--;
        if (b->refcnt == 0)
            // no one is waiting for it.
            b->tick = ticks; // update tick
        
        release(&bcache.HT.locks[TSH(b->blockno)]);
    }
    ```

6. 修改函数bpin、bunpin：

    ```c
    void
    bpin(struct buf *b) {
        acquire(&bcache.HT.locks[TSH(b->blockno)]);
        b->refcnt++;
        release(&bcache.HT.locks[TSH(b->blockno)]);
    }

    void
    bunpin(struct buf *b) {
        acquire(&bcache.HT.locks[TSH(b->blockno)]);
        b->refcnt--;
        release(&bcache.HT.locks[TSH(b->blockno)]);
    }
    ```
