/*** includes ***/
#include <ctype.h>
#include <stdio.h>

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** globals ***/
struct termios og_terminal_settings;

/*** terminal setup and takedown functions ***/
void failure(const char *fail_code) {
  perror(fail_code);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_terminal_settings) == -1) {
    failure("tcsetattr failed on disable raw mode");
  }
}

/*  turn off canoical input mode in terminal
    stop Echo to terminal
*/
void enableRawMode() {

  // tcgetattr() fucntion gets parameters from termianal referenced by
  // STDIN_FILENO and stores them in the termios structure
  if (tcgetattr(STDIN_FILENO, &og_terminal_settings) == -1) {
    failure("tcgetattr failed to get terminal settings");
  }

  // create settigns struct for editing and set program to reapply base settings
  // on exit
  struct termios terminal_settings_struct = og_terminal_settings;
  atexit(disableRawMode);

  // local flags edit via bitwise operators
  // c_lflag ex[1110] AND inverted ECHO-[1000]
  // [1110] AND [0111] = [0110] so only the ECHO bit(s) are changed within
  // c_lflag

  // lflags turn off echo and cannonical(read char by char not line by line)
  // in flags ctrl c,z,s,q
  // out flags stop enter from returning to start of next line automatically
  // meaning from now on \r\n is required to return carrage to start of line.
  terminal_settings_struct.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  terminal_settings_struct.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK);
  terminal_settings_struct.c_oflag &= ~(OPOST);
  terminal_settings_struct.c_cflag &= ~(CS8);

  terminal_settings_struct.c_cc[VMIN] = 0;
  terminal_settings_struct.c_cc[VTIME] = 1;

  // apply the changed settings to the terminal and flush buffer of leftover
  // chars
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_settings_struct) == -1) {
    failure("tcsetattr failed to set new terminal settings");
  }
}

int main() {
  enableRawMode();

  while (1) {

    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1) {
      failure("read failed");
    }
    // If control character print the value
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } // else print the value and the char it represents
    else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q')
      break;
  }

  return 0;
}
