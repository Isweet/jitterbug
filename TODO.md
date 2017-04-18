#TODO

* Bug: doesn't properly reset after exit
  + I think this is fixed? Needs more testing

# DONE

* Bug: bits get flipped when you type at human speeds? Or maybe its something else?
  + This is because of keystrokes being sent that are marked as "special" in code (releases, shifts, etc.)
  + FIXED: keymap was wrong for space character, thus no jitter, but is valid packet
