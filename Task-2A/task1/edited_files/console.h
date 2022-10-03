//constants used in console.c
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
#define INPUT_BUF 128
#define UP_ARROW 226
#define DOWN_ARROW 227
#define LEFT_ARROW 228
#define RIGHT_ARROW 229
#define MAX_HISTORY 16

#include "types.h"


void
earaseCurrentLineOnScreen(void);


void
copybuffToBeShiftedToOldBuf(void);



void
earaseContentOnInputBuf();

void
copyBufferToScreen(char * bufToPrintOnScreen, uint length);

void
copyBufferToInputBuf(char * bufToSaveInInput, uint length);


void
saveCMDinHistoryMem();

int history(char *buffer, int historyId);