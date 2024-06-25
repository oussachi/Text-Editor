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
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
    enableRawMode();

    while(1) {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if (iscntrl(c)) { 
            //iscntr(c) checks if a character is a control character aka non-printable
            // we print \r to bring the cursor to the start of the line
            printf("%d\r\n", c);
        } else {
            printf("%d (%c)\r\n", c, c);
        }
        if (c == 'q') break;
    }
    return 0;
}