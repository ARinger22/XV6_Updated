#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_history(char *buffer, int historyId)
{
  if (argptr(0, &buffer, sizeof(char *)) < 0 || argint(1, &historyId) < 0)
    return -1;

  return getFromHistory(buffer, historyId); // getcommandfromhistory
}
int sys_cps(void)
{
  return cps();
}

int sys_wait2(void) {
  int res;
  int stime = 0;
  int retime = 0;
  int rutime = 0;
  struct proc *curproc = myproc();
  argint(0, &retime);
  argint(1, &rutime);
  argint(2, &stime);
  res = wait();
  *(int*)stime = curproc->stime;
  *(int*)retime = curproc->retime;
  *(int*)rutime = curproc->rutime;
  return res;
}
