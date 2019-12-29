// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct page_info {
  int ref_cnt;
} *pages;
#define PA2PAGE(pa) (((char *)(pa) - (char *)(pages)) / (PGSIZE))

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p = (char*)PGROUNDUP((uint64)pa_start);
  pages = (struct page_info *)p;
  int npages = ((char *)pa_end - p) / PGSIZE + 1;
  int nbytes = npages * sizeof(struct page_info);
  memset(pages, 0, nbytes);
  
  p = (char *)PGROUNDUP((uint64)(p + nbytes));
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  struct page_info *pg = pages + PA2PAGE(pa);
  if (pg->ref_cnt > 0)
    pg->ref_cnt--; 
  if (pg->ref_cnt > 0) {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    pages[PA2PAGE(r)].ref_cnt = 1;
  }
  release(&kmem.lock);

  if (r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void *
kref(void *pa)
{
  acquire(&kmem.lock);
  pages[PA2PAGE(pa)].ref_cnt++;
  release(&kmem.lock);

  return pa;
}
