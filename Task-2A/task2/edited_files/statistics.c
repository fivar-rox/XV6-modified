#include "types.h"
#include  "stat.h"
#include "user.h"

int main() {

    int retime=0, rutime=0, stime=0, pid=0;
    uint i;
    for(i=0; i<3; i++) {
         pid = fork();
         if(pid && wait2(&retime, &rutime, &stime) == pid) {
             printf(1,"pid: %d, retime: %d, rutime: %d, stime: %d\n", pid, retime, rutime, stime);
         }
    }
    exit();
}