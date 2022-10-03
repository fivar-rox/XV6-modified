#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "ASCII_image.h"
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


// Implementing a sys_draw function
int sys_draw(void)
{

  char *buffer;
  int size;

  // Feches the 1st 32 bit int argument which is the max buffer sizeand assigns it to the size
  if (argint(1, &size) == -1)
  {
    // Invalid address is accessed
    return -1;
  }

  // Check that the buffer pointer in first argument
  // lies within the process address space or not till size bytes, if it does not then return -1.
  if (argptr(0, (char **)&buffer, size) == -1)
  {
    // does not lie in the process address space.
    return -1;
  }
  
  // copying macro wolfi from ASCII_image.h
  char *draw = wolfi;
  
  int drawsize = 0;
  while (draw[drawsize] != '\0')
  {
    drawsize++;
  }

  if (drawsize > size)
  {
    //buffer size is insufficient to draw the wolf picture.
    return -1;
  }
  
  //copying the wolf picture into the buffer.
  for (int i = 0; i < drawsize; i++)
  {
    buffer[i] = draw[i];
  }
  
  //return the size of draw pictue
  return drawsize;
}

int sys_history(void) {
  char *buffer;
  int historyId;
  
  argptr(0, &buffer, 1);
  
  argint(1, &historyId);

  return history(buffer, historyId);
}
