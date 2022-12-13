#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
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
  backtrace();
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

uint64
sys_sigalarm(void)
{
  int nticks;
  uint64 hdlr;

  argint(0, &nticks);
  argaddr(1, &hdlr);
  
  struct proc* p = myproc();

  p->nticks=nticks;
  p->hdlr=hdlr;

  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc* p = myproc();
  p->trapframe->ra =p->contextAlarm.ra;
  p->trapframe->sp=p->contextAlarm.sp;
  p->trapframe->s0=p->contextAlarm.s0;
  p->trapframe->s1=p->contextAlarm.s1;
  p->trapframe->s2=p->contextAlarm.s2;
  p->trapframe->s3=p->contextAlarm.s3;
  p->trapframe->s4=p->contextAlarm.s4;
  p->trapframe->s5=p->contextAlarm.s5;
  p->trapframe->s6=p->contextAlarm.s6;
  p->trapframe->s7=p->contextAlarm.s7;
  p->trapframe->s8=p->contextAlarm.s8;
  p->trapframe->s9=p->contextAlarm.s9;
  p->trapframe->s10=p->contextAlarm.s10;
  p->trapframe->s11=p->contextAlarm.s11;
  p->trapframe->a0=p->contextAlarm.a0;
  p->trapframe->a1=p->contextAlarm.a1;
  p->trapframe->a2=p->contextAlarm.a2;
  p->trapframe->a3=p->contextAlarm.a3;
  p->trapframe->a4=p->contextAlarm.a4;
  p->trapframe->a5=p->contextAlarm.a5;
  p->trapframe->a6=p->contextAlarm.a6;
  p->trapframe->a7=p->contextAlarm.a7;
  p->trapframe->epc=p->contextAlarm.epc;
  // printf("restore %p\n", p->trapframe->a0);
  p->hdlrinprocess=0;
  return 0;
}