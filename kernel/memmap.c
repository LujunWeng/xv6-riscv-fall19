#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "syscall.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "memmap.h"

struct vma vmas[NVMA];

void
mmapinit(void)
{
  for (int i = 0; i < NVMA; ++i) {
    initsleeplock(&vmas[i].lock, "vma");
    vmas[i].addr = -1;
    vmas[i].refcnt = 0;
    vmas[i].pc = 0;
  }
}

struct vma *
mmap_alloc_vma()
{
  for (struct vma *vp = vmas; vp < &vmas[NVMA]; ++vp) {
    acquiresleep(&vp->lock);
    if (vp->refcnt == 0) {
      ++vp->refcnt;
      releasesleep(&vp->lock);
      return vp;
    }
    releasesleep(&vp->lock);
  }
  return 0;
}

struct vma *
mmap_find_vma(uint64 addr)
{
  for (struct vma *vp = vmas; vp < &vmas[NVMA]; vp++) {
    acquiresleep(&vp->lock);
    if (vp->refcnt > 0 && vp->pc == myproc() && addr >= vp->addr && addr < vp->addr + vp->len) {
      releasesleep(&vp->lock);
      return vp;
    }
    releasesleep(&vp->lock);
  }
  return 0; 
}

uint64
sys_mmap(void)
{
  printf("mmap-proc: %p\n", myproc());
  struct vma *vp = mmap_alloc_vma();
   
  if (vp == 0)
    panic("sys_mmap: no more vma");
  acquiresleep(&vp->lock);
 
  if (argaddr(0, &vp->addr) < 0 || argint(1, &vp->len) < 0 ||
      argint(2, &vp->prot) < 0 || argint(3, &vp->flags) < 0 ||
      argfd(4, 0, &vp->fp) < 0 || argint(5, &vp->offset) < 0) {
    goto mmap_err;
  }
  if (vp->len == 0)
    goto mmap_err;
  if (!vp->fp->writable && (vp->prot & PROT_WRITE) && vp->flags == MAP_SHARED)
    goto mmap_err;

  filedup(vp->fp);

  uint64 n_pa = (vp->len - 1) / PGSIZE + 1;
  struct proc *p = myproc();
  vp->addr = p->vma_start - n_pa * PGSIZE;
  p->vma_start = vp->addr;
  vp->pc = p;

  printf("mmap: %p: %p + %d\n", vp->pc, vp->addr, vp->len);
  releasesleep(&vp->lock);
  return vp->addr;

mmap_err:
  --vp->refcnt;
  releasesleep(&vp->lock);
  return -1;
}

void
mmap_unmap(struct vma *vp, uint64 addr, int len)
{
  printf("munmap-proc: %p\n", myproc());
  uint64 addr_end = addr + len;
  while (addr_end > addr && walkaddr(vp->pc->pagetable, addr_end) == 0)
    addr_end = PGROUNDDOWN(addr_end) - 1; 
  printf("munmap: actual %p + %d = %p\n", addr, addr_end - addr, addr_end);
  if (addr_end > addr) {
    int avail_len = addr_end - addr;
    if (vp->flags == MAP_SHARED && (vp->prot & PROT_WRITE)) {
      printf("munmap: write %p + %d\n", addr, avail_len);
      struct file *f = vp->fp;
      int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
      for (int i = 0, r; i < avail_len; i += r) {
        int nb = avail_len - i > max ? max : avail_len-i;
        begin_op(f->ip->dev);
        ilock(f->ip);
        r = writei(f->ip, 1, addr+i, vp->offset+i, nb);
        iunlock(f->ip);
        end_op(f->ip->dev);
        if (r < 0) break;
        if (r != nb) panic("munmap: short write");
      }
    } 
    printf("munmap: uvmunmap %p + %d\n", addr, avail_len);
    uvmunmap(myproc()->pagetable, addr, avail_len, 1);  
  }
  if (addr + len == vp->addr + vp->len) {
    --vp->refcnt;
    vp->addr = -1;
    fileclose(vp->fp);
    vp->fp = 0;
    vp->pc = 0;
  } else {
    vp->addr += len;
    vp->len  -= len;
    printf("update vma: %p + %d = %p\n", addr, len, addr + len);
  }
  printf("munmap: %p + %d\n", addr, len);
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int len;

  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0)
    return -1;
  
  struct vma *vp = mmap_find_vma(addr);
  acquiresleep(&vp->lock);
  if (addr >= vp->addr + vp->len)
    return 0;
  if (addr + len > vp->addr + vp->len) {
    len = vp->addr + vp->len - addr;   
  }
  mmap_unmap(vp, addr, len);
  releasesleep(&vp->lock);
  return 0;
}
