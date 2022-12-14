// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.
#include "console.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50

static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  switch(c) {
    case '\n':
      pos += 80 - pos%80;
      break;
    case BACKSPACE:
      if(pos > 0) --pos;
      break;
    case LEFT_ARROW:
      if(pos > 0) --pos;
      break;
    default:
      crt[pos++] = (c&0xff) | 0x0700;  // black on white
  }

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80); 
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  if (c == BACKSPACE)
    crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  switch (c) {
    case BACKSPACE:
      uartputc('\b'); uartputc(' '); uartputc('\b');  // uart is writing to the linux shell
      break;
    case LEFT_ARROW:
      uartputc('\b');
      break;
    default:
      uartputc(c);
  }
  cgaputc(c);
  // uartputc prints to Linux's terminal and cgaputc prints to QEMU's terminal
}

struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  uint rightmost; // the first empty char in the line
} input;

// stores the charactes in the input which have to be shifted while backspacing and typing while caret is not at the end.
char buffToBeShifted[INPUT_BUF]; 

// this struct stores the commands and its details.
struct {
  char CommandMemArr[MAX_HISTORY][INPUT_BUF];   // holds the actual command strings.
  uint lengthsArr[MAX_HISTORY];                 // this will hold the length of each command string.
  uint FinalCMdIndex;                          // the index of the last command entered to history.
  int TotalCMDsInMem;                           // total number of commands executed from the system boot.
  int currentPosition;                          // no. of skips in history array while toggling up and down arrow.
} HistoryMem;

char oldBuf[INPUT_BUF]; // this will hold the details of the command that was written before accessing the history
uint lengthOfOldBuf;

char buf2[INPUT_BUF];

#define C(x)  ((x)-'@')  // Control-x

// copies the contents which have to be shifted from input to bufftoshifted 
void copybuffToBeShifted() {
  uint n = input.rightmost - input.e; // contents after edit have to be shifted when typed or backspaced.
  uint i;
  for (i = 0; i < n; i++)
    buffToBeShifted[i] = input.buf[(input.e + i) % INPUT_BUF];
}

// shifts the input to right by one position and repaints it on the line on the screen from edit index and brings back caret to the original position
void shiftbufright() {
  uint n = input.rightmost - input.e;
  int i;
  for (i = 0; i < n; i++) {

    char c = buffToBeShifted[i];
    input.buf[(input.e + i) % INPUT_BUF] = c;
    consputc(c); // repaitning the screen.
  }
  // reset buffToBeShifted for future use
  memset(buffToBeShifted, '\0', INPUT_BUF);
  // return the caret to its correct position
  for (i = 0; i < n; i++) {
    consputc(LEFT_ARROW);
  }
}

// Shift input.buf one positon to the left, and repaint the chars on-screen. Used only when punching in BACKSPACE and the caret isn't at the end of the line.
void shiftbufleft() {
  uint n = input.rightmost - input.e;
  uint i;
  consputc(LEFT_ARROW);
  input.e--;
  for (i = 0; i < n; i++) {
    char c = input.buf[(input.e + i + 1) % INPUT_BUF];
    input.buf[(input.e + i) % INPUT_BUF] = c;
    consputc(c); // repainting the screen.
  }
  input.rightmost--;
  consputc(' '); // delete the last char in line
  for (i = 0; i <= n; i++) {
    consputc(LEFT_ARROW); // shift the caret back to the left
  }
}

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  uint tempIndex;
  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    	case C('P'):  // Process listing.
        doprocdump = 1;   // procdump() locks cons.lock indirectly; invoke later
        break;
      case C('U'):  // Kill line.
        if (input.rightmost > input.e) { // caret isn't at the end of the line
          uint numtoshift = input.rightmost - input.e;
          uint placestoshift = input.e - input.r;
          uint i;
          for (i = 0; i < placestoshift; i++) {
            consputc(LEFT_ARROW);
          }
          memset(buf2, '\0', INPUT_BUF);
          for (i = 0; i < numtoshift; i++) {
            buf2[i] = input.buf[(input.r + i + placestoshift) % INPUT_BUF];
          }
          for (i = 0; i < numtoshift; i++) {
            input.buf[(input.r + i) % INPUT_BUF] = buf2[i];
          }
          input.e -= placestoshift;
          input.rightmost -= placestoshift;
          for (i = 0; i < numtoshift; i++) { // repaint the chars
            consputc(input.buf[(input.e + i) % INPUT_BUF]);
          }
          for (i = 0; i < placestoshift; i++) { // erase the leftover chars
            consputc(' ');
          }
          for (i = 0; i < placestoshift + numtoshift; i++) { // move the caret back to the left
            consputc(LEFT_ARROW);
          }
        }
        else { // caret is at the end of the line -                                       ( deleting everything from both screen and inputbuf)
          while(input.e != input.r &&
                input.buf[(input.e - 1) % INPUT_BUF] != '\n'){ 
            input.e--;
            input.rightmost--;
            consputc(BACKSPACE);
          }
        }
        break;
      case C('H'): case '\x7f':  // Backspace
        if (input.rightmost != input.e && input.e != input.r) { // caret isn't at the end of the line
          shiftbufleft(); // shifting buffer to one position left.
          break;
        }
        if(input.e != input.r){ // caret is at the end of the line - deleting last char
          input.e--;
          input.rightmost--;
          consputc(BACKSPACE);
        }
        break;
      case LEFT_ARROW:
        if (input.e != input.r) {
          input.e--;
          consputc(c);
        }
        break;
      case RIGHT_ARROW:
        if (input.e < input.rightmost) {
          consputc(input.buf[input.e % INPUT_BUF]);
          input.e++;
        }
        else if (input.e == input.rightmost){ // This line add the cursor at the end ogf the line. 
          consputc(' ');
          consputc(LEFT_ARROW);
        }
        break;
      case UP_ARROW:
       if (HistoryMem.currentPosition < HistoryMem.TotalCMDsInMem-1 && HistoryMem.currentPosition < MAX_HISTORY-1 ){ 
          // current history means the oldest possible will be MAX_HISTORY-1
          earaseCurrentLineOnScreen(); // eraseing the whole line 
          earaseContentOnInputBuf();   // erasing in input.buf
          if (HistoryMem.currentPosition == -1) // if it is the first toggle we make then the our written command  should be stored.
              copybuffToBeShiftedToOldBuf();
          HistoryMem.currentPosition++; // toggling by increasing out current position.
          tempIndex = (HistoryMem.FinalCMdIndex + HistoryMem.currentPosition) %MAX_HISTORY; // gives us the index of currentposition'th index from the recent command.
          copyBufferToScreen(HistoryMem.CommandMemArr[ tempIndex]  , HistoryMem.lengthsArr[tempIndex]);
          copyBufferToInputBuf(HistoryMem.CommandMemArr[ tempIndex]  , HistoryMem.lengthsArr[tempIndex]);
        }
        break;
      case DOWN_ARROW:
        switch(HistoryMem.currentPosition){
          case -1:
            //does nothing
            break;
          case 0: // prints the string from oldbuff
            earaseCurrentLineOnScreen();
            copyBufferToInputBuf(oldBuf, lengthOfOldBuf);
            copyBufferToScreen(oldBuf, lengthOfOldBuf);
            HistoryMem.currentPosition--; // decreasing out current position.
            break;
          default:
            earaseCurrentLineOnScreen();
            HistoryMem.currentPosition--; // decreasing out current position.
            tempIndex = (HistoryMem.FinalCMdIndex + HistoryMem.currentPosition) % MAX_HISTORY;
            copyBufferToScreen(HistoryMem.CommandMemArr[ tempIndex]  , HistoryMem.lengthsArr[tempIndex]);
            copyBufferToInputBuf(HistoryMem.CommandMemArr[ tempIndex]  , HistoryMem.lengthsArr[tempIndex]);
            break;
        }
        break;
      case '\n':
      case '\r':
	  input.e = input.rightmost;
      default:
	if(c != 0 && input.e-input.r < INPUT_BUF){
	  c = (c == '\r') ? '\n' : c;
	  if (input.rightmost > input.e) { // caret isn't at the end of the line
	    copybuffToBeShifted();
	    input.buf[input.e++ % INPUT_BUF] = c;
	    input.rightmost++;
	    consputc(c);
	    shiftbufright();
	  }
	  else {
	    input.buf[input.e++ % INPUT_BUF] = c;
	    input.rightmost = input.e - input.rightmost == 1 ? input.e : input.rightmost;
	    consputc(c);
	  }
	  if(c == '\n' || c == C('D') || input.rightmost == input.r + INPUT_BUF){
	    saveCMDinHistoryMem(); // when enter is entered we saving that command to historyMem
	    input.w = input.rightmost;
	    wakeup(&input.r);
	  }
	}
	break;
      }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

// this method eareases the current line from screen
void
earaseCurrentLineOnScreen(void){
    uint numToEarase = input.rightmost - input.r;
    while (input.e < input.rightmost) { // taking caret to the end of the line.
          consputc(input.buf[input.e % INPUT_BUF]);
          input.e++;
        }
    uint i;
    for (i = 0; i < numToEarase; i++) {
      consputc(BACKSPACE); // backspacing the whole line.
    }
}

// this method copies the chars currently on display (and on Input.buf) to oldBuf and save its length on current_history_viewed.lengthOld
void
copybuffToBeShiftedToOldBuf(void){
    lengthOfOldBuf = input.rightmost - input.r;
    uint i;
    for (i = 0; i < lengthOfOldBuf; i++) {
        oldBuf[i] = input.buf[(input.r+i)%INPUT_BUF];
    }

}

// this method earase all the content of the current command on the inputbuf
void
earaseContentOnInputBuf(){
  input.rightmost = input.r;
  input.e = input.r;
}

/*
  this method will print the given buf on the screen
*/
void
copyBufferToScreen(char * bufToPrintOnScreen, uint length){
  uint i;
  for (i = 0; i < length; i++) {
    consputc(bufToPrintOnScreen[i]);
  }
}


// this method will copy the given buf to Input.buf will set the input.e and input.rightmost assumes input.r=input.w=input.rightmost=input.e
void
copyBufferToInputBuf(char * bufToSaveInInput, uint length){
  uint i;
  for (i = 0; i < length; i++) {
    input.buf[(input.r+i)%INPUT_BUF] = bufToSaveInInput[i];
  }
  input.e = input.r+length;
  input.rightmost = input.e;
}

// This method saves the current command into the historyMem
void
saveCMDinHistoryMem(){
  HistoryMem.TotalCMDsInMem++; // counting the total no.of commands executed till now.
  uint l = input.rightmost-input.r -1;
  HistoryMem.FinalCMdIndex = (HistoryMem.FinalCMdIndex - 1) % MAX_HISTORY; // this step stores the commands in a cyclic manner if the memory is full. 
  HistoryMem.lengthsArr[HistoryMem.FinalCMdIndex] = l;
  uint i;
  for (i = 0; i < l; i++) { //do not want to save in memory the last char '/n'
    HistoryMem.CommandMemArr[HistoryMem.FinalCMdIndex][i] =  input.buf[(input.r+i)%INPUT_BUF];
  }
  return;
}

/*
  this is the function that gets called by the sys_history and writes the requested command history in the buffer
*/
int history(char *buffer, int historyId) {
  // this function returns command which was executed at historID+1 position in the stored MAX_HISTORY commands.
  if (historyId < 0 || historyId > MAX_HISTORY - 1)
    return -2;
  if (historyId >= HistoryMem.TotalCMDsInMem )
    return -1;
  memset(buffer, '\0', INPUT_BUF);
  uint temp;
  if(HistoryMem.TotalCMDsInMem > MAX_HISTORY){
    temp = HistoryMem.FinalCMdIndex - 1;
  }
  else{
    temp = MAX_HISTORY - 1;
  }
  temp = (temp - historyId) % MAX_HISTORY;
  memmove(buffer, HistoryMem.CommandMemArr[temp], HistoryMem.lengthsArr[temp]);
  return 0;
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
  HistoryMem.TotalCMDsInMem = 0;
  HistoryMem.FinalCMdIndex = 0;
  HistoryMem.currentPosition = -1;
}

