/*** includes ***/
#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** preprocesser directives ***/
// macro to tell compiler to replace CTRL_KEY(k) with bitwise and between key
// value and ctrl value [00011111] https://en.wikipedia.org/wiki/Control_key for
// more info
#define CTRL_KEY(key) ((key) & 0x1f)

/*** globals ***/
struct editorConfig {
  struct termios og_terminal_settings;
  int screenrows, screencols;
};

struct editorConfig E;

/*** terminal setup and takedown functions ***/
void failure(const char *fail_code) {
  // clear display
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // cursor to top left
  write(STDOUT_FILENO, "\x1b[H", 4);

  perror(fail_code);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.og_terminal_settings) == -1) {
    failure("tcsetattr failed on disable raw mode");
  }
}

/*  turn off canoical input mode in terminal
    stop Echo to terminal
*/
void enableRawMode() {

  // tcgetattr() fucntion gets parameters from termianal referenced by
  // STDIN_FILENO and stores them in the termios structure
  if (tcgetattr(STDIN_FILENO, &E.og_terminal_settings) == -1) {
    failure("tcgetattr failed to get terminal settings");
  }

  // create settigns struct for editing and set program to reapply base settings
  // on exit
  struct termios terminal_settings_struct = E.og_terminal_settings;
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

char editorReadKey() {
  int nread;
  char c;

  // check for input until error or sucessfull read
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      failure("read failed in editorReadKey()");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buffer[32];
  unsigned int i = 0;

  // get cursor position report in format "\x1b[xx;xxR"
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  while (i < sizeof(buffer) - 1) {
    if (read(STDOUT_FILENO, &buffer[i], 1) != 1) {
      break;
    }
    if (buffer[i] == 'R') {
      break; // end of data in buffer
    }
    i++;
  }
  // convert 'R' to '\0';
  buffer[i] = 0;
  //  printf("\r\n&buffer[1]: '%s'\r\n", &buffer[1]);
  getchar();
  // parse for values
  if (buffer[0] != '\x1b' || buffer[1] != '[')
    return -1;
  if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  // all from sys/ioctl.h
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // move cursor to bottom left without leaving the screen
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return getCursorPosition(rows, cols);
  } else {
    //  printf("col %i, row %i", ws.ws_col, ws.ws_row);
    *cols = ws.ws_col;
    *rows = ws.ws_row;
  }
  return 0;
}

/*** append buffer						***/
// STEP36
/*** Inputs						       ***/
void editorProcessKeyPress() {
  char c = editorReadKey();

  // add special key cases here
  switch (c) {
  case CTRL_KEY('q'):
    // Erase all in display
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // cursor to top left
    write(STDOUT_FILENO, "\x1b[H", 4);
    exit(0);
    break;
  }
}

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~", 1);
    if (y < E.screenrows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}
/*** Outputs							***/
void editorRefreshScreen() {
  // write 4 bytes to clear display more info below
  write(STDOUT_FILENO, "\x1b[2J", 4);
  /* ESC [ Ps J 	default value: 0

    This sequence erases some or all of the characters in the display according
    to the parameter. Any complete line erased by this sequence will return that
    line to single width mode. Editor Function Parameter 	Parameter
    Meaning 0 	Erase from the active position to the end of the screen,
    inclusive (default) 1 	Erase from start of the screen to the active
    position, inclusive 2 	Erase all of the display – all lines are erased,
    changed to single-width, and the cursor does not move.
  */

  // moves cursor to top left
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** Initialize ***/
void init() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    failure("getWindowSize failure");
}

int main() {
  enableRawMode();
  init();

  while (1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  }

  return 0;
}
