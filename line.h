#define LINE_SZ       256

#define SHIFT         0x2a

#define SHIFT_PRESS   0x2a
#define SHIFT_RELEASE 0xaa

#define SPECIAL_KEY   0x00

#define RELEASE_MASK  0x80

typedef struct line_s {
  unsigned char contents[LINE_SZ];
  int size;
} line_t;

void initialize_line(line_t *l);

void copy_line(line_t *dst, line_t *src);

unsigned char line_put(unsigned char scancode, line_t *l, line_t *copy);
