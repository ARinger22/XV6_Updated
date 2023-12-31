// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

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
#include "get_history.h"

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
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
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

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

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

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

// history command starts from here
char oldBuf[INPUT_BUF];
uint oldLength;
char buf2[INPUT_BUF];

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  uint tempIndex;
  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case UP_ARROW:
      if (history_buffer.current < history_buffer.memCommand - 1)
      {
        eraseLine();
        if (history_buffer.current == -1)
          oldBuffer();

        eraseInput();
        history_buffer.current++;
        tempIndex = (history_buffer.lastIndex + history_buffer.current) % MAX_HISTORY;
        copyBuffer(history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
        copyInput(history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
      }
      break;
    case DOWN_ARROW:
      switch (history_buffer.current)
      {
      case -1:
        // does nothing
        break;
      case 0: // get string from old buf
        eraseLine();
        copyInput(oldBuf, oldLength);
        copyBuffer(oldBuf, oldLength);
        history_buffer.current--;
        break;
      default:
        eraseLine();
        history_buffer.current--;
        tempIndex = (history_buffer.lastIndex + history_buffer.current) % MAX_HISTORY;
        copyBuffer(history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
        copyInput(history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
        break;
      }
      break;

    default:
      if (c != 0 && input.e - input.r < INPUT_BUF)
      {
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          saveCommand();
          input.w = input.e;
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

void saveCommand() // savecommandinhistory
{
  uint len = input.e - input.r - 1;
  if (len == 0)  return;                              
  history_buffer.current = -1;
  if (history_buffer.memCommand < MAX_HISTORY){
    history_buffer.memCommand++;
  }
  history_buffer.lastIndex = (history_buffer.lastIndex - 1) % MAX_HISTORY;
  history_buffer.lengthsArr[history_buffer.lastIndex] = len;
  for (uint i = 0; i < len; i++){
    history_buffer.bufferArr[history_buffer.lastIndex][i] = input.buf[(input.r + i) % INPUT_BUF];
  }
  // cprintf("%d - History copied to the buffer properly", 0);
}

int getFromHistory(char *buffer, int historyId)
{
  // historyId != index of command in history_buffer.bufferArr
  if (historyId < 0 || historyId > MAX_HISTORY - 1){
    return 2;
  }
  if (historyId >= history_buffer.memCommand){
    return 1;
  }
  memset(buffer, '\0', INPUT_BUF);
  int tempIndex = (history_buffer.lastIndex + historyId) % MAX_HISTORY;
  memmove(buffer, history_buffer.bufferArr[tempIndex], history_buffer.lengthsArr[tempIndex]);
  return 0;
}

void eraseLine(void)
{
  while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n')
  {
    input.e--;
    consputc(BACKSPACE);
  }
}

void oldBuffer(void)
{
  oldLength = input.e - input.r; // doubt
  for (uint i = 0; i < oldLength; i++)
  {
    oldBuf[i] = input.buf[(input.r + i) % INPUT_BUF];
  }
}

void eraseInput()
{
  input.e = input.r;
}

void copyBuffer(char *bufToPrintOnScreen, uint length)
{
  uint i = 0;
  while (length--)
  {
    consputc(bufToPrintOnScreen[i]);
    i++;
  }
}

void copyInput(char *bufToSaveInInput, uint length)
{
  for (uint i = 0; i < length; i++)
  {
    input.buf[(input.r + i) % INPUT_BUF] = bufToSaveInInput[i];
  }

  input.e = input.r + length;
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
  history_buffer.memCommand = 0;
  history_buffer.lastIndex = 0;
  ioapicenable(IRQ_KBD, 0);
}

