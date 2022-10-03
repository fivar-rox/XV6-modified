#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
  
  #ifdef SML
  if (argc != 2){
    printf(1, "Use this format: SMLsanity <no.of.processes>\n");
    exit();
  }

  int n = 3*atoi(argv[1]); 
  int id, i, j;
  int c_time, wait_time, run_time, io_time;
	
  int pid;
  for (int ind=0; ind<n; ind++) { 
    pid = fork();
    if (pid == 0) {//child
      id = (getpid() - 4) % 3; // ensures independence from the first son's pid when gathering the results in the second part of the program
      switch(id) {
        case 0:
          set_prio(1);
          break;
	case 1:
          set_prio(2);
          break;
	case 2:
          set_prio(3);
          break;
      }
      for (i=0; i<100; i++) {
        j = 0;
        while (j<100000) j++;  
      }
      exit(); // child exit here
    }
    continue; // parent continues to fork the next child
  }
  
  for (int ind=0; ind<n; ind++) {
    pid = wait2(&c_time, &wait_time, &run_time, &io_time);
    id = (pid - 4) % 3; // correlates to j in the dispatching loop
    switch(id) {
      case 0: 
        printf(1, "Priority 1, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d, termination time = %d\n", pid, c_time, wait_time, run_time, io_time, wait_time + run_time + io_time, c_time + wait_time + run_time + io_time);
        break;
      case 1:
        printf(1, "Priority 2, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d, termination time = %d\n", pid, c_time, wait_time, run_time, io_time, wait_time + run_time + io_time, c_time + wait_time + run_time + io_time);
	break;
      case 2: 
	printf(1, "Priority 3, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d, termination time = %d\n", pid, c_time, wait_time, run_time, io_time, wait_time + run_time + io_time, c_time + wait_time + run_time + io_time);
	break;
    }
  } 
  
  #else
  printf(1, "Works only for SML\n");
  #endif
  exit();
}