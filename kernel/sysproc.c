#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// duplicated codes in sysfile.c
// copy directly since its defined as static
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

char checkaddr(uint64 addr, uint64 len){
  struct proc * p = myproc();
  for(int i=0;i<VMASIZE;i++){
    acquire(&p->lock);
    if(p->vma[i].free!=0){
      if(!(p->vma[i].addr+p->vma[i].length<addr || p->vma[i].addr>addr+len)){
        release(&p->lock);
        return 1;
      }
    }
    release(&p->lock);
  }
  return 0;
}

uint64
sys_mmap(void)
{
  struct proc * p = myproc();

  uint64 addr;
  int length, prot, flags, offset;
  struct file *f;


  argaddr(0, &addr);
  argint(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  if(argfd(4, 0, &f) < 0)
    return 0xffffffffffffffff;
  argint(5, &offset);

  // not implementing custom mapped address
  if(addr != 0){
    return 0xffffffffffffffff;
  }

  if(flags == MAP_SHARED){
    if((prot & PROT_READ) && !checkfile(0, f))
      return 0xffffffffffffffff;

    if((prot & PROT_WRITE) && !checkfile(1, f))
      return 0xffffffffffffffff;
  }

  int pagenum = length/4096 + (length % 4096 ? 1 : 0);
  
  int found=0, j=0, k=0, w=0, concnt1=0, concnt2=0, concnt3=0;
  pagetable_t pagetable_l1 = p->pagetable;
  for(j=5; j<512; j++){
    pte_t pte_l1 = pagetable_l1[j];
    if(!(pte_l1 & PTE_V)){
      ++concnt1;
      if((concnt1*512*512+concnt2*512+concnt3) >=pagenum && !checkaddr((j<<21)+(k<<13)+(w<<4), length)){
        found=1;
        goto end_loop;
      }
      continue;
    }
    else{
      concnt1=0;
    }

    for(k=0; k<512; k++){
      pagetable_t pagetable_l2 = ((pagetable_t)PTE2PA(pte_l1));
      pte_t pte_l2 = pagetable_l2[k];
      if(!(pte_l2 & PTE_V)){
        ++concnt2;
        if((concnt1*512*512+concnt2*512+concnt3) >=pagenum && !checkaddr((j<<21)+(k<<13)+(w<<4), length)){
          found=1;
          goto end_loop;
        }
        continue;
      } 
      else{
        concnt2=0;
      }
      for(w=0; w<512; w++){
        pagetable_t pagetable_l3 = ((pagetable_t)PTE2PA(pte_l2));
        pte_t pte_l3 = pagetable_l3[w];
        if(!(pte_l3 & PTE_V)){
          ++concnt3;
          if((concnt1*512*512+concnt2*512+concnt3) >=pagenum && !checkaddr((j<<21)+(k<<13)+(w<<4), length)){
            found=1;
            goto end_loop;
          }
          continue;
        }
        else{
          concnt3=0;
        }
      }
    }
  }

end_loop:

  if(found==0){
    return 0xffffffffffffffff;
  }

  int start = (j<<21)+(k<<13)+(w<<4);
  for(int i=0;i<pagenum;i++){
    walk(pagetable_l1, start+PGSIZE*i, 1);
  }

  addr = start;
  
  acquire(&p->lock);
  // find available vma entry
  int i=0;
  for(; i<VMASIZE; i++){
    if(p->vma[i].free == 0){
      p->vma[i].free = 1;
      break;
    }
  }

  release(&p->lock);

  if(i == VMASIZE){
    return 0xffffffffffffffff;
  }

  f = filedup(f);

  acquire(&p->lock);

  p->vma[i].addr = addr;
  p->vma[i].length = length;
  p->vma[i].prot = prot;
  p->vma[i].permission = flags;
  p->vma[i].f = f;
  p->vma[i].offset = offset;

  release(&p->lock);

  return addr;
}

uint64
munmap(uint64 addr, int length, int do_free, char locked)
{
  uint64 addr_end;
  struct proc * p = myproc();

  addr = PGROUNDDOWN(addr);
  addr_end = PGROUNDUP(addr+length);

  for(int i=0; i<VMASIZE; i++){
    if(p->vma[i].free==0){
      continue;
    }

    if(p->vma[i].addr<=addr || addr_end<=p->vma[i].addr+p->vma[i].length){
      int off_to_left = addr-p->vma[i].addr;
      int off_to_right = p->vma[i].addr+p->vma[i].length-addr_end, vma_mode=0;
      struct file * vma_fd;

      if(off_to_left==0 && off_to_right==0){
        // free whole page

        uint64 vma_length, vma_addr;

        if(!locked)
        acquire(&p->lock);

        vma_length = p->vma[i].length;
        p->vma[i].free = 0;
        vma_mode = p->vma[i].permission;
        vma_fd = p->vma[i].f;
        vma_addr=p->vma[i].addr;
        
        if(!locked)
        release(&p->lock);

        // not check dirty bit for convinience
        if(vma_mode&MAP_SHARED){
          filewrtiehelper(vma_fd, vma_addr, vma_length, 1);
          
        }

        if((walk(p->pagetable, vma_addr, 0)) != 0){
          uvmunmap(p->pagetable, vma_addr, vma_length/PGSIZE, do_free);
        }

        return vma_length;
      }
      else if(off_to_left==0 && off_to_right>0){
        // free left half

        uint64 vma_addr;
        int cut_size = PGROUNDDOWN(addr_end-addr);

        if(!locked)
        acquire(&p->lock);
        
        vma_fd = p->vma[i].f;
        vma_addr=p->vma[i].addr;
        // vma_length = p->vma[i].length;
        p->vma[i].length -= cut_size;
        
        vma_mode = p->vma[i].permission;
        p->vma[i].addr += cut_size;
        p->vma[i].offset += cut_size;

        if(!locked)
        release(&p->lock);

        // not check dirty bit for convinience
        if(vma_mode&MAP_SHARED){
          filewrtiehelper(vma_fd, vma_addr, cut_size, 0);
        }

        uvmunmap(p->pagetable, vma_addr, cut_size/PGSIZE, do_free);

        return cut_size;
      }
      else if(off_to_right==0 && off_to_left>0){
        // free right half

        uint64 vma_length, vma_addr;
        off_to_right = PGROUNDUP(off_to_right);

        if(!locked)
        acquire(&p->lock);
        
        vma_fd = p->vma[i].f;
        vma_addr=p->vma[i].addr;
        vma_length = p->vma[i].length;
        vma_mode = p->vma[i].permission;
        p->vma[i].length -= off_to_right;

        if(!locked)
        release(&p->lock);

        // not check dirty bit for convinience
        if(vma_mode&MAP_SHARED){
          filewrtiehelper(vma_fd, vma_length+vma_addr-off_to_right, off_to_right, 0);
        }

        uvmunmap(p->pagetable, vma_addr, off_to_right/PGSIZE, do_free);

        return off_to_right;
      }

    }
  }

  return 0xffffffffffffffff;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  argaddr(0, &addr);
  argint(1, &length);
  return munmap(addr, length, -1, 0);
}
