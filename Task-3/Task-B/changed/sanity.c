#include "types.h"
#include "stat.h"
#include "user.h"

int allocnum(int n){
	return n*n - 4*n + 1;
}

int
main(int argc, char* argv[]){

	for(int i=0; i<20; i++){
		if(fork() == 0){
			printf(1, "Child %d\n", i+1);
			printf(1, "   S.no   Matched   Error\n");
			printf(1, "--------- ------- ---------\n\n");
			
			for(int j=0; j<10; j++){
				int *a = malloc(4096);
				for(int k=0; k<1024; k++) a[k] = allocnum(k);
				
				int Matched_B = 0;
				for(int k=0; k<1024; k++) if(a[k] == allocnum(k)) Matched_B += 4;
				
				if(j<9) printf(1, "    %d      %dB      %dB\n", j+1, Matched_B, 4096 - Matched_B);
				else printf(1, "   %d      %dB      %dB\n", j+1, Matched_B, 4096 - Matched_B);
				
			}
			printf(1, "\n");
			
			exit();
		}
	}

	while(wait()!=-1);
	exit();

}
