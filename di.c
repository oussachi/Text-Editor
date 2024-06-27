// ***************** includes ****************** //
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>


// ***************** defines ****************** //
// This defines a Ctrl+key key stroke, since Ctrl+key returns ints from 1 to 26
#define CTRL_KEY(k) ((k) & 0x1f)


// ***************** data ****************** //
struct editor_config { // contains the terminal's global state
    struct termios orig_termios; // contains the original terminal attributes
    int screen_rows; // number of rows in the terminal
    int screen_cols; // number of columns in the terminal
};

struct editor_config E;

// ***************** terminal ****************** //
// error printing + exit function
void die(const char * s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// a function to disable raw mode 
void disableRawMode() {
    // We set the terminal's attributes to their original values
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}


// A function used to set the terminal in raw mode
void enableRawMode() {
    // tcgetattr() retrieves the terminal's attributes and places them in a termios struct
    // We retrieve the terminal's original attributes and 
    // save them in orig_termios to restore them later
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    // disable raw mode at exit
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    // Here we are disabling :
        // echoing in the terminal -ECHO- (keystrokes aren't printed back to us)
        // Canonical mode -ICANON-, allowing us to read byte-by-byte instead of line-by-line 
        // Ctrl+C (SIGINT) and Ctrl+Z (SIGSTP) -ISIG-
        // Ctrl+S and Ctrl+Q (software flow control) -IXON-
        // Ctrl+V -IEXTEN- and Ctrl+M -ICNRL-
        // Output processing -OPOST- :
            // \r\n are automatically added after pressing enter
            // \r or carriage return puts the cursor at the beginning of the line
            // \n or newline brings the cursor down a line
    // The c_lflag field is a field of miscellaneous flags
    // The c_iflag field is a field of input flags
    // The c_iflag field is a field of ouput flags
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // Adding a timeout for read()
    // VMIN : min number of bytes before read() can return
    // VTIME : max time for read() to wait before returning
    // c_cc field stands for control characters, contains terminal settings
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 10;


    // tcsetattr() sets the termios strcut as the terminal's attributes 
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

// A function that waits for one keypress and returns it
char editorReadKey() {
    char c;
    int nread;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

// A function that returns the current cursor position, used in case ioctl() fails
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    // The escape sequence "\x1b[6n" or the n command (used to get terminal's status info) 
    // with the parameter 6 returns the cursor position
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    // We store the returned string of the escape sequence in a buffer
    while(i < sizeof(buf)) {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    // We pass the value in the buffer to rows and cols
    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}


// A function that gets the size of the terminal (nb of rows and cols) using ioctl() and TIOCGWINSZ request
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // 999C sends the cursor 999 positions to the right, 999B sends it 999 positions down
        // This is to ensure we reach the bottom right corner
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) 
            return -1;
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}


// ***************** append buffer ****************** //
struct abuf {
    char *b;
    int len; 
};

#define ABUF_INIT {NULL, 0}

// A function that appends a string s to the string of an abuf struct ab
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

// A function that frees the string of a abuf struct
void abFree(struct abuf *ab) {
    free(ab->b);
}

// ***************** output ****************** //
// A function that draws a tilde(~) at each line after the end of the file being edited
void editorDrawRows(struct abuf *ab) {
    int y;
    for(y = 0; y < E.screen_rows; y++) {
        abAppend(ab, "~", 1);

        if(y < E.screen_rows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}



// A function that clears the screen 
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // We are writing an escape sequence to the terminal
    // escape sequences always start with "\x1b["
    // the escape sequence used is J with the argument 2, which will clear the entire screen
    // for more info on escape sequences : https://en.wikipedia.org/wiki/VT100
    abAppend(&ab, "\x1b[2J", 4);
    // The escape sequence "\x1b[H" places the cursor on the top left corner
    abAppend(&ab, "\x1b[H", 3);
    // Drawing the tildes
    editorDrawRows(&ab);
    // Repositioning the cursor at the top-left corner
    abAppend(&ab, "\x1b[H", 3);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}



// ***************** input ****************** //
// A function that handles one keypress
void editorProcessKeypress() {
    char c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}


// ***************** init ****************** //


void initEditor() {
    if(getWindowSize(&E.screen_rows, &E.screen_cols) == -1)
        die("getWindowSize");
}


int main() {
    enableRawMode();
    initEditor();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}