#define MAP_PRIVATE 0   // No write back
#define MAP_SHARED  1   // write back to mapped file

// Protection for memory mapping
#define PROT_WRITE 0x1
#define PROT_READ  0x2

#define NVMA 20

struct vma {
  uint64 addr;
  int len;
  int prot;
  int flags;
  
  struct file *fp;
  int offset; 
  
  struct proc *pc;
  struct sleeplock lock;
  int refcnt;
};
extern struct vma vmas[NVMA];

void mmapinit(void);
struct vma *mmap_find_vma(uint64 addr);
struct vma *mmap_alloc_vma();
void mmap_unmap(struct vma *, uint64 addr, int len);
