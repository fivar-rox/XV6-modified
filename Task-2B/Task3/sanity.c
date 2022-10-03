#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
  
  if (argc != 2){
    printf(1, "Use this format: sanity <no.of.processes>\n");
    exit();
  }
  int n = 3*atoi(argv[1]);
  int id, i, j;
  int c_time, wait_time, run_time, io_time;
  int total[3][3];
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      total[i][j] = 0;
      
  int pid;
  for (int ind=0; ind<n; ind++) {
    pid = fork();
    if (pid == 0) {//child
      id = (getpid() - 4) % 3; // ensures independence from the first son's pid when gathering the results in the second part of the program
      switch(id) {
        case 0: // CPU
	  for (i=0; i<100; i++) {
	    j = 0;
            while (j<100000) j++;  
          }
	  break;
	case 1: // S-CPU
	  for (i=0; i<100; i++) {
            j = 0;
            while (j<100000) j++;
            yield2();
          }
	  break;
	case 2: // I/O 
        j=0;
        while (j<100) {
            j++;
            sleep(1);
          }
	  break;
      }

      exit(); // child exit here
    }
    continue; // parent continues to fork the next child
  }

  for (int ind=0; ind<n; ind++) {
    pid = wait2(&c_time, &wait_time, &run_time, &io_time);
    id = (pid - 4) % 3; // correlates to j in the dispatching loop
    switch(id) {
      case 0: // CPU 
        printf(1, "CPU, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d\n", pid,c_time, wait_time, run_time, io_time, wait_time + run_time + io_time);
	total[0][0] += wait_time;
	total[0][1] += run_time;
	total[0][2] += io_time;
        break;
      case 1: // S-CPU  
        printf(1, "S-CPU, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d\n", pid,c_time, wait_time, run_time, io_time, wait_time + run_time + io_time);
	total[1][0] += wait_time;
	total[1][1] += run_time;
	total[1][2] += io_time;
	break;
      case 2: // I/O 
	printf(1, "I/O, pid: %d, creation time:%d, wait time: %d, run time: %d, io time: %d, turnaround time: %d\n", pid,c_time, wait_time, run_time, io_time, wait_time + run_time + io_time);
	total[2][0] += wait_time;
	total[2][1] += run_time;
	total[2][2] += io_time;
	break;
    }
  } 
  
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      total[i][j] /= n/3;

  printf(1, "\nCPU\nAverage wait time: %d\nAverage run time: %d\nAverage io time: %d\nAverage turnaround time: %d\n\n", total[0][0], total[0][1], total[0][2], total[0][0]+total[0][1]+total[0][2]);
  printf(1, "S-CPU\nAverage wait time: %d\nAverage run time: %d\nAverage io time: %d\nAverage turnaround time: %d\n\n", total[1][0], total[1][1], total[1][2], total[1][0]+total[1][1]+total[1][2]);
  printf(1, "I/O\nAverage wait time: %d\nAverage run time: %d\nAverage io time: %d\nAverage turnaround time: %d\n\n", total[2][0], total[2][1], total[2][2], total[2][0]+total[2][1]+total[2][2]);
  exit();
}