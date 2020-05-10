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

#define NBUCKET 19

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct bucket buckets[NBUCKET];
} bcache;

static uint
naive_hash(uint blockno)
{
  return blockno % NBUCKET;
}

static void
bucket_add(struct bucket *bk, struct buf *b)
{
  b->next = bk->head.next;
  b->prev = &bk->head;
  bk->head.next->prev = b;
  bk->head.next = b; 
}

static void
bucket_rm(struct buf *b)
{
  b->prev->next = b->next;
  b->next->prev = b->prev;
  b->prev = b;
  b->next = b;
}

static struct buf *
rm_least_used()
{
  static uint p = 0;
  int i;
  for (i = 0; i < 2*NBUF; ++i) {
    struct buf *b = &bcache.buf[(p+i)%NBUF];
    struct bucket *bk = &bcache.buckets[naive_hash(b->blockno)];
    acquire(&bk->lock);
    // printf("lru: %d, clkbit: %d\n", b->refcnt, b->clkbit);
    if (b->refcnt == 0 && b->clkbit == 0) {
      bucket_rm(b);
      release(&bk->lock);
      p = (p + i) % NBUF;
      return b; 
    } else {
      b->clkbit = 0;
    }
    release(&bk->lock);
  }
  p = (p + i) % NBUF;
  return 0;
}

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; ++i) {
    struct bucket *bk = &bcache.buckets[i];
    initlock(&bk->lock, "bcache.bucket");
    bk->head.next = &bk->head;
    bk->head.prev = &bk->head;
  }   

  for (int i = 0; i < NBUF; ++i) {
    struct buf *b = bcache.buf + i;
    initsleeplock(&b->lock, "buffer");
    b->clkbit = 0;
    b->refcnt = 0;
    struct bucket *bk = &bcache.buckets[naive_hash(b->blockno)];
    bucket_add(bk, b);
  } 
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = naive_hash(blockno);
  struct bucket *bk = &bcache.buckets[hash];
  acquire(&bk->lock);
  for (b = bk->head.next; b != &bk->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->clkbit = 1;
      release(&bk->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  acquire(&bcache.lock);
  b = rm_least_used();
  if (b) {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->clkbit = 1;
    acquire(&bk->lock);
    bucket_add(bk, b);  
    release(&bk->lock);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  
  for (int i = 0; i < NBUCKET; ++i) {
    printf("%d\n", i);
    for (b = bcache.buckets[i].head.next; b != &bcache.buckets[i].head; b = b->next)
      printf("  no: %d, ref: %d, clk: %d\n", b->blockno, b->refcnt, b->clkbit);
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
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  struct bucket *bk = &bcache.buckets[naive_hash(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}

void
bpin(struct buf *b) {
  struct bucket *bk = &bcache.buckets[naive_hash(b->blockno)];
  acquire(&bk->lock);
  b->refcnt++;
  release(&bk->lock);
}

void
bunpin(struct buf *b) {
  struct bucket *bk = &bcache.buckets[naive_hash(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}

