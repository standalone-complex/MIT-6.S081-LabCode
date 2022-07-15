// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define TSH(x) (x%NBUCKET)

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

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i<NBUCKET; ++i) {
    initlock(&bcache.HT.locks[i], "bucket");

    bcache.HT.heads[i].prev = &bcache.HT.heads[i];
    bcache.HT.heads[i].next = &bcache.HT.heads[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; ++b) {
    initsleeplock(&b->lock, "buf");

    b->next = bcache.HT.heads[TSH(b->blockno)].next;
    b->prev = &bcache.HT.heads[TSH(b->blockno)];
    bcache.HT.heads[TSH(b->blockno)].next->prev = b;
    bcache.HT.heads[TSH(b->blockno)].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
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
  // Is the block already cached?
  for(b = bcache.HT.heads[TSH(blockno)].next; b != &bcache.HT.heads[TSH(blockno)]; b = b->next)
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.HT.locks[TSH(blockno)]);
      acquiresleep(&b->lock);
      return b;
    }
  release(&bcache.HT.locks[TSH(blockno)]);

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

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
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
    b->tick = ticks;
  
  release(&bcache.HT.locks[TSH(b->blockno)]);
}

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


