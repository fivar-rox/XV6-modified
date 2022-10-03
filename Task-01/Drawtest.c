#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
	// created a buffer of 4000 bytes enough to hold ascii image.
	char *buffer = malloc(4000);

	// gets the size of image
	int imgsize = draw(buffer,4000);

	if(imgsize == -1){
		// if size is less 
		printf(1, "Size of buffer is too small.\n");
	}
	
	else {
		// print buffer size
		printf(1,"Image Size = %d Bytes\n", imgsize);
		
		// print image
		printf(1, "%s\n", buffer);
	}
	
	return 0;
}
