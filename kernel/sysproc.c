#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 base, dst;
  int len;
  unsigned int bmask = 0; // 32-bit mask

  argaddr(0, &base);
  argint(1, &len);
  argaddr(2, &dst);

  if(len<0 || len > 32){
    printf("len must be in range [0, 32]\n");
    return -1;
  }

  pagetable_t ptbl = myproc()->pagetable;
  base=PGROUNDDOWN(base);
  for(int i=0; i<len; i++){
    pte_t * pte = walk(ptbl, base+i*PGSIZE, 0);
    unsigned int mask = (1<<i);
    if(*pte & PTE_A){
      bmask |= mask;
    }

    // clear PTE_A
    *pte &= (~(PTE_A));
  }

  if(copyout(ptbl, dst, (char*)&bmask, sizeof(bmask))<0){
    return -1;
  }

  return 0;
}
#endif

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
