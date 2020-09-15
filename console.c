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
#include "pid_ns.h"
#include "proc.h"
#include "x86.h"
#include "fcntl.h"
#include "console.h"

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory
static int panicked = 0;

typedef struct device_lock {
  struct spinlock lock;
  int locking;
} device_lock;

static device_lock cons;

static void consputc(int);

static inline void update_pos(int pos) 
{
  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

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

int dont_write = 0; /* Inidcates how many characters of input to skip from buffering, meaning they wont be saved or printed to screen. */

//This function returns 1 in case a special key is used while using qemu-nox (meaning qemu without graphics).
int
testSpecial(int xx, int base, int sign) 
{
    static char digits[] = "0123456789abcdef";
    char buf[16];
    int i;
    uint x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    /* When special key is used, always this sequence of numbers is returned. */
    if (buf[1] == 50 && buf[3] == -128) { 
        /* Special keys return 2 buffers, therefore, we dont want to display both buffers. 
        If we write here 1, it will return the second buffer. */
        dont_write = 2; 
        
        return 1;
    }
    return 0;
}

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

  update_pos(pos);
}

void consoleclear(void){
  int pos = 0;
  memset(crt, 0, sizeof(crt[0])*(24*80));
  update_pos(pos);
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


struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;


struct {
  char buf[INPUT_BUF]; // History buffer.
  int length;          // History buffer length.
} hist[HIST_BUF];

int hist_input = 0; /* History input index. */
int hist_save = 0; /* Current buffer index inside history. */
int hist_used = 1; /* How many history slots are used. Start at 1 to prevent errors. */
int current_index = 0; /* Index which is used to track history input. */
int special_key_used = 0; /* Was a special key used? */
int dont_save = 0; /* Indicates not to save history in case it already exsists. */
int extra_skip = 0; /*Skips one more character from being printed onto the console.
                    * Used in case of some special keys.*/
char used_char = 0; /*The last character used.
                    * Used in case of some special keys.*/
char temp_buff[INPUT_BUF]; /* Temporary buffer. */
int preload = 0; /* Pre-load history, only can occur once. */

void
consoleintr(int(*getc)(void))
{
  int c = 0;

  //-------Preload History--------------------------------------
  //All the commands which will be preloaded are defined in console.h under predefined history section.
  //Also note history preload can occur only once.
  if (preload == 0) {
    for (int i = 0; i < PREDEF_COUNT; i++) {
      strncpy(hist[i].buf, predef_hist[i], strlen(predef_hist[i]));
      hist[i].length = strlen(predef_hist[i]);
      hist_save++;
      hist_used++;
    }

    preload = 1;
  }
  //-------------------------------------------------------------
  acquire(&cons.lock);
  while ((c = getc()) >= 0) {
    switch (c) {
    case C('P'):  // Process listing.
                  // procdump() locks cons.lock indirectly
      release(&cons.lock);
      procdump();  // now call procdump() wo. cons.lock held
      cprintf("$ "); // After executing procdump() the shell sign '$' vanishes, but it can still accept new commands.
      acquire(&cons.lock);
      break;
    case C('U'):  // Kill line.
      while (input.e != input.w &&
        input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
        input.e--;
        consputc(BACKSPACE);
      }
      hist_input = 0; /* Mirrors line erasing in history input.*/
      break;
    case C('H'): case '\x7f':  // Backspace
      if (input.e != input.w) {
        input.e--;
        consputc(BACKSPACE);
      }
      /* Mirrors backspace effect in history buffer. */
      if (hist_input > 0) {
        hist_input--;
        if (special_key_used == 1) {
          current_index = (current_index + 1) % (hist_used - 1);
          special_key_used = 0;
        }
      }
      break;

    default:
      //-------------This part handles the console special keys when using qemu-----------------------------------------
      //Note all this part does is convert the input to match the nox case (meaning qemu-nox, in shortened case will be
      //reffered to as nox)
      if (c == ARROW_UP) {
        dont_write = 1;
        c = ARROW_UP_NOX; /*Convert to match nox code*/
      }

      if (c == ARROW_DOWN) {
        dont_write = 1;
        c = ARROW_DOWN_NOX;
      }
      //--------------------------------------------------------------------------------------------------------------------

      //-------------This part handles the console special keys when using qemu-nox-----------------------------------------
      if (dont_write > 0) { /* In case special key is used, don't type anything after it until output buffer is cleared */
        dont_write--;
        if (dont_write == 0) {
          if (c == ARROW_UP_NOX && hist_used > 1) {
            special_key_used = 1;

            /* Clear the previous command */
            while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
              input.e--;
              consputc(BACKSPACE);
            }

            /* Push history buffer into input buffer */
            for (int i = 0; i < hist[current_index].length; i++) {
              input.buf[input.e++ % INPUT_BUF] = hist[current_index].buf[i];
              consputc(hist[current_index].buf[i]);
              hist[hist_save].buf[i] = hist[current_index].buf[i];
              temp_buff[i] = hist[current_index].buf[i];
            }

            /* Need to return length of input to history length, else can only use this history once */
            hist_input = hist[current_index].length;
            current_index = (current_index - 1) % (hist_used - 1);
            if (current_index == -1)
              current_index = (hist_used - 2);
          }

          else if (c == ARROW_DOWN_NOX && hist_used > 1) {
            special_key_used = 1;

            /* Clear the previous command */
            while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
              input.e--;
              consputc(BACKSPACE);
            }

            /* Push history buffer into input buffer */
            for (int i = 0; i < hist[current_index].length; i++) {
              input.buf[input.e++ % INPUT_BUF] = hist[current_index].buf[i];
              consputc(hist[current_index].buf[i]);
              hist[hist_save].buf[i] = hist[current_index].buf[i];
              temp_buff[i] = hist[current_index].buf[i];
            }

            /* Need to return length of input to history length, else can only use this history once */
            hist_input = hist[current_index].length;
            current_index = (current_index + 1) % (hist_used - 1);
            if (current_index == hist_used)
              current_index = 0;
          }

          /* Here we handle the special keys which are extra long. */
          else if ((c == PAGEUP_NOX || c == PAGEDOWN_NOX ||
            c == INSERT_NOX || c == DELETE_NOX)
            && extra_skip == 0)
          {
            extra_skip = 1; /* Skip one more character from being displayed. */
            dont_write = 1;
            used_char = c; /* Remember which special character was used. */
          }

          if (extra_skip == 1) {
            extra_skip = 0;
            //------------------------------------------------------------------
            //Here you can program the following special keys:
            //Pageup, pagedown, insert, delete.
            //To do so, use used_char parameter, for example:
            //if (used_char == INSERT_NOX) {
            //  /*Which procedure to run*/
            //}
            //------------------------------------------------------------------
          }
        }
        break;
      }

      /*Test whether a special key was used*/
      if (testSpecial(c, 10, 1) == 1) {
        break;
      }

      //----------------In this section the output to console occurs + commands handle-----------------
      if (c != 0 && input.e - input.r < INPUT_BUF) {
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        //------History handle---------
        /* Don't save '\n'. */
        if (c != '\n') {
          temp_buff[hist_input] = c;
          hist_input++;
        }
        //-----------------------------
        consputc(c);
        if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) {
          /* We don't want to save empty commands or execute them. */
          if (hist_input > 0) {

            /*Support for xv6 shutdown*/
            if (strncmp("quit", temp_buff, hist_input) == 0) {
              outw(0x604, 0x2000);
            }

            /*Support for xv6 reboot*/
            if (strncmp("reboot", temp_buff, hist_input) == 0) {
              uint good = 0x02;
              while (good & 0x02)
                good = inb(0x64);
              outb(0x64, 0xFE);
            }

            /*Help section - if you expand the keys/commands avialable for user, make sure you document them here*/
            if (strncmp("help", temp_buff, hist_input) == 0) {
              /*Before we can use cprintf, need to release console lock, then reaquire it.*/
              release(&cons.lock);
              cprintf("Type 'ls' to see available programs.\n\
Type 'quit' to shutdown xv6.\n\
Type 'reboot' to reboot xv6.\n\
\nNotice the console supports the following special keys:\n\
Ctrl+p will display the current processes in the system.\n\
Ctrl+u will clear the current input line in the console.\n\
Arrow up/down to scroll through typed commands previously (history).\n\
\nIn addition, the console has the following predefined commands history:\n");
              for (int i = 0; i < PREDEF_COUNT; i++)
                cprintf("%s\n", predef_hist[i]);
              cprintf("These commands are available immediately in the console history.\n");
              acquire(&cons.lock);

              /*Prevent execution of command 'help'.
              * It is only a console command, not a program which can be executed by the system.
              */
              input.e -= (hist_input + 1);
              input.buf[input.e++ % INPUT_BUF] = '\n';
            }
            //------History handle----------------------------------------
            /* Do not save input which is already saved in history. */
            for (int i = 0; i < HIST_BUF; i++) {
              if (strncmp(hist[i].buf, temp_buff, hist[i].length) == 0 && hist_input == hist[i].length) {
                dont_save = 1;
                break;
              }
            }
            /* Save the input into history. */
            if (dont_save == 0) {
              /* Save the length of the input. */
              hist[hist_save].length = hist_input;
              strncpy(hist[hist_save].buf, temp_buff, hist_input);
              /* Advance save input to next history slot. */
              hist_save = (hist_save + 1) % HIST_BUF;
              if (hist_used < HIST_BUF) {
                hist_used++;
              }
              current_index = (current_index + 1) % (hist_used - 1);
            }
            dont_save = 0;
            hist_input = 0; /* Reset history input index back to 0. */
          }
          //-----------------------------
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
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
ttyread(struct inode *ip, char *dst, int n)
{
  if(devsw[ip->major].flags & DEV_CONNECT){
    return consoleread(ip,dst,n);
  }
  return -1;
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

int
ttywrite(struct inode *ip, char *buf, int n)
{
  if(devsw[ip->major].flags & DEV_CONNECT){
    return consolewrite(ip,buf,n);
  }
  //2DO: should return -1 when write to tty fails - filewrite panics.
  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = ttywrite;
  devsw[CONSOLE].read = ttyread;
  devsw[CONSOLE].flags = DEV_CONNECT;

  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

void
ttyinit(void)
{
  int i;
  for(i = CONSOLE+1; i <= CONSOLE+NTTY; i++){
     devsw[i].write = ttywrite;
     devsw[i].read = ttyread;
     devsw[i].flags = 0;
     ioapicenable(IRQ_KBD, 0);
  }

}



