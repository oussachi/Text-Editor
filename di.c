// ***************** includes ****************** //
#include<unistd.h>
#include<termios.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdio.h>
#include<errno.h>


// ***************** defines ****************** //
// This defines a Ctrl+key key stroke, since Ctrl+key returns ints from 1 to 26
#define CTRL_KEY(k) ((k) & 0x1f)


// ***************** data ****************** //
struct termios orig_termios; // contains the original terminal attributes


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
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}


// A function used to set the terminal in raw mode
void enableRawMode() {
    // tcgetattr() retrieves the terminal's attributes and places them in a termios struct
    // We retrieve the terminal's original attributes and 
    // save them in orig_termios to restore them later
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
    // disable raw mode at exit
    atexit(disableRawMode);

    struct termios raw = orig_termios;

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


// ***************** output ****************** //
// A function that clears the screen 
void editorRefreshScreen() {
    // We are writing an escape sequence to the terminal
    // escape sequences always start with "\x1b["
    // the escape sequence used is J with the argument 2, which will clear the entire screen
    // for more info on escape sequences : https://en.wikipedia.org/wiki/VT100
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // The escape sequence "\x1b[H" places the cursor on the top left corner
    write(STDOUT_FILENO, "\x1b[H", 3);
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
int main() {
    enableRawMode();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}