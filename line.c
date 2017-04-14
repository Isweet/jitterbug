#include <linux/module.h>
#include <linux/kernel.h>

#include "line.h"

static unsigned char keymap[128] = 
{
  0,    // UNDEF (Error Code)
  0,    // UNDEF (Escape)
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
  0,    // UNDEF (Left Control)
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
  '`',
  0,    // UNDEF (Left Shift)
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
  0    // UNDEF (non-password keys)
};

static unsigned char skeymap[128] = 
{ 
  0,    // UNDEF (Error Code)
  0,    // UNDEF (Escape)
  '!',
  '@',
  '#',
  '$',
  '%',
  '^',
  '&',
  '*',
  '(',
  ')',
  '_',
  '+',
  '\b', // Backspace
  '\t', // Tab
  'Q',
  'W',
  'E',
  'R',
  'T',
  'Y',
  'U',
  'I',
  'O',
  'P',
  '{',
  '}',
  '\n', // Enter
  0,    // UNDEF (Left Control)
  'A',
  'S',
  'D',
  'F',
  'G',
  'H',
  'J',
  'K',
  'L',
  ':',
  '\"', 
  '~',
  0,    // UNDEF (Left Shift)
  '|',
  'Z',
  'X',
  'C',
  'V',
  'B',
  'N',
  'M',
  '<',
  '>',
  '?',
  0,   // UNDEF (non-password keys)
};

static bool shiftPressed = false;

void initialize_line(line_t *l) {
  memset(l->contents, '\0', LINE_SZ);
  l->size = 0;

  return;
}

void copy_line(line_t *dst, line_t *src) {
  int i;

  for (i = 0; i < LINE_SZ; i++) {
    dst->contents[i] = src->contents[i];
  }

  dst->size = src->size;

  return;
}

unsigned char line_put(unsigned char scancode, line_t *l, line_t *copy) {
  unsigned char mapped_key;
  
  if (scancode == SHIFT) {
    shiftPressed = true;
    return SPECIAL_KEY;
  }

  if (scancode == (SHIFT | RELEASE_MASK)) {
    shiftPressed = false;
    return SPECIAL_KEY;
  }

  if (scancode & RELEASE_MASK) {
    return SPECIAL_KEY;
  }

  mapped_key = shiftPressed ? skeymap[scancode] : keymap[scancode];
  
  // If we got a newline, nuke the current line (possibly saving it into copy)
  if (mapped_key == '\n') {
    if (copy != NULL) {
      copy_line(copy, l);
    }

    printk(KERN_INFO "Got here!\n");

    initialize_line(l);
  } else {
    // If we got backspace, and line has at least one character
    if (mapped_key == '\b' && 0 < l->size) {
      l->contents[--l->size] = '\0';
    } else {
      l->contents[l->size++] = mapped_key;
    }
  }

  return mapped_key;
}
