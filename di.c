#include<unistd.h>
#include<termios.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdio.h>

struct termios orig_termios; // contains the original terminal attributes

// a function to disable raw mode 
void disableRawMode() {
    // We set the terminal's attributes to their original values
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


// A function used to set the terminal in raw mode
void enableRawMode() {
    // tcgetattr() retrieves the terminal's attributes and places them in a termios struct
    // We retrieve the terminal's original attributes and 
    // save them in orig_termios to restore them later
    tcgetattr(STDIN_FILENO, &orig_termios);
    // disable raw mode at exit
    atexit(disableRawMode);

    struct termios raw = orig_termios;

    // Here we are disabling :
        // echoing in the terminal -ECHO- (keystrokes aren't printed back to us)
        // Canonical mode -ICANON-, allowing us to read byte-by-byte instead of line-by-line 
        // Ctrl+C (SIGINT) and Ctrl+Z (SIGSTP) -ISIG-
        // Ctrl+S and Ctrl+Q (software flow control) -IXON-
        // Ctrl+V -IEXTEN- and Ctrl+M -ICNRL-
    // The c_lflag field is a field of miscellaneous flags
    // The c_iflag field is a field of input flags
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // tcsetattr() sets the termios strcut as the terminal's attributes 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) { 
            //iscntr(c) checks if a character is a control character aka non-printable
            printf("%d\n", c);
        } else {
            printf("%d (%c)\n", c, c);
        }
    };
    return 0;
}