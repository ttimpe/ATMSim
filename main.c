#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdbool.h>
#include <ncurses.h>
#include <signal.h>
#include <locale.h>


#define MAX_LINE_LENGTH 256
#define LOG_WIDTH 40
#define STR_WINDOW_HEIGHT 2
#define STR_WINDOW_WIDTH 17


#define NUMPAD_HEIGHT 9
#define NUMPAD_WIDTH 15

// Function to read a line from serial port and remove checksum
int read_line(int fd, char *buffer, int max_length);

// Function to parse incoming message and extract checksum
int parse_message(char *message);

// Function to calculate and prepend checksum to a string
void sign_telegram(char *message);

// Function to send message over serial port with checksum
void send_message(int fd, const char *message);

// Function to write text to the specified line with optional blinking
void write_text(WINDOW *win, int line, int mode, int pos, char *text, bool blink);

// Signal handler for terminal resizing
void handle_resize(int sig);
void draw_buttons();

void draw_numpad();

WINDOW *log_window;
WINDOW *str_window;
WINDOW *numpad_window;
WINDOW *buttons_win;

bool is_in_number_mode = false;
char number_buffer[17] = "";

int main(int argc, char *argv[]) {
   	setlocale(LC_ALL, "");
   	if (argc != 2) {
        printf("Usage: %s <serial_port_name>\n", argv[0]);
        return 1;
    }

    const char *portname = argv[1];
    int serial_port = open(portname, O_RDWR);

    if (serial_port < 0) {
        perror("Error opening serial port");
        return 1;
    }

    timeout(0); // Set timeout to 0 to return ERR if no input is available
    nodelay(stdscr, TRUE);

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0); // Hide cursor
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(4, COLOR_BLACK, COLOR_BLUE);
    init_pair(5, COLOR_BLACK, COLOR_RED);
    init_pair(2, COLOR_BLACK, COLOR_WHITE);  // Gray background, black text
    init_pair(3, COLOR_BLACK, COLOR_YELLOW);

    // Create a window for the log
    log_window = newwin(LINES, LOG_WIDTH, 0, COLS - LOG_WIDTH);
    nodelay(log_window, TRUE);
    scrollok(log_window, TRUE); // Enable scrolling for the log window
    wborder(log_window, 0,0,0,0,0,0,0,0);
    wrefresh(log_window);

    // Create a window for the strings
    str_window = newwin(STR_WINDOW_HEIGHT, STR_WINDOW_WIDTH, 1, 1);
    nodelay(str_window, TRUE);
    wbkgd(str_window, COLOR_PAIR(1)); // Set background color for the display window
    wrefresh(str_window);
    WINDOW *label_window = newwin(1, 17, 3, 1);
    nodelay(label_window, TRUE);
    wbkgd(label_window, COLOR_PAIR(2));
    wattron(label_window, COLOR_PAIR(2));
    mvwprintw(label_window, 0,0, "LIKU Z  RHST ABW");
    wrefresh(label_window);

    // Draw the numpad
    numpad_window = newwin(NUMPAD_HEIGHT, NUMPAD_WIDTH, 0, 25);
    box(numpad_window, 0, 0);
    draw_numpad();
    draw_buttons();
    // Initialize two strings to display
    char string1[17] = "IBIS GESTOERT 1";
    char string2[17] = "              ";

    // Register the signal handler for terminal resizing
    signal(SIGWINCH, handle_resize);

    // Main loop
    while (1) {
        // Write strings to string window with green background
        write_text(str_window, 0, 0, 0, string1, false);
        write_text(str_window, 1, 0, 0, string2, false);
        if (is_in_number_mode == TRUE) {
            mvwprintw(str_window, 0, 16 - strlen(number_buffer), number_buffer);
            wrefresh(str_window);
        }
        // Read data from serial port
        char buf[MAX_LINE_LENGTH];
        int len = read_line(serial_port, buf, MAX_LINE_LENGTH);
        if (len > 0) {
            // Parse incoming message and update display accordingly
          parse_message(buf);
            // Print the received message to the log window
               
        //    if (checksum != -1) {
                // Remove checksum from the received message
                buf[len - 1] = '\0'; // Null-terminate the string after removing checksum
                len--; // Decrement the length to exclude the checksum
                wprintw(log_window, "in-> %s\n", buf);
                wrefresh(log_window);
                // Handle the message (without the checksum)
                if (strncmp(buf, "mC", 2) == 0 && len >= 6) {
                    int mode = buf[2] - '0';
                    int length = buf[3] - '0' + 1;
                    int line = buf[4] - '0';
                    int pos = buf[5] - '0';
                    if (mode == 3) {
                        is_in_number_mode = true;
                    } else {
                        is_in_number_mode = false;
                    }
                    if (line == 0) {
                        strncpy(string1 + pos, buf + 6, length);
                    } else if (line == 1) {
                        strncpy(string2 + pos, buf + 6, length);
                    }
                }
                if (strncmp(buf, "mS", 2) == 0) {
                    char status_response[4] = "mQ\r";
                    send_message(serial_port, status_response);
                }
            
              
        } else if (len == 0) {
            printf("End of file reached\n");
            break;
        } else {
            printf("Error reading line\n");
            break;
        }
        char ch = wgetch(str_window); // Use wgetch() instead of getch() to work with curses
        if (ch != ERR) {

        // Send pressed character to serial port with "m" prefix
        if (is_in_number_mode == TRUE) {
            // Check for ESC key, send delete
            if (ch == 27) {
                char message[4] = "m/\r";
                send_message(serial_port, message);
                is_in_number_mode = false;
                number_buffer[0] = '\0';
            }
            if (ch != 10) {
                if (strlen(number_buffer) < 16) {
                    strncat(number_buffer, &ch, 1);
                }
            } else {
                // is enter, send the characters

                char number_message[32] = "m?";
                int numlen = strlen(number_buffer);
                
                wprintw(log_window, "number length: %d\n", numlen);
                wrefresh(log_window);
                strncat(number_message, number_buffer, numlen);
                strncat(number_message, "\r", 1);

                send_message(serial_port, number_message);

                number_buffer[0] = '\0';
                is_in_number_mode = false;
            }
            // append to string array number_buffer
        } else {
            char message[4];
            message[0] = 'm';
            message[1] = ch;
            message[2] = '\r';
            message[3] = '\0';
            send_message(serial_port, message);
        }
    }
 usleep(10000); 
     }

    // End ncurses
    endwin();

    close(serial_port);
    //free(log_window);
    //free(str_window);
    return 0;
}


void draw_numpad() {
    

    wattron(numpad_window, COLOR_PAIR(2));


     // Draw the numpad buttons
    mvwprintw(numpad_window, 1, 2, "7");
    mvwprintw(numpad_window, 1, 7, "8");
    mvwprintw(numpad_window, 1, 12, "9");
    mvwprintw(numpad_window, 3, 2, "4");
    mvwprintw(numpad_window, 3, 7, "5");
    mvwprintw(numpad_window, 3, 12, "6");
    mvwprintw(numpad_window, 5, 2, "1");
    mvwprintw(numpad_window, 5, 7, "2");
    mvwprintw(numpad_window, 5, 12, "3");
    mvwprintw(numpad_window, 7, 7, "0");

    wattron(numpad_window, COLOR_PAIR(3));
    mvwprintw(numpad_window, 7, 2, "X");
    mvwprintw(numpad_window, 7, 12, "X");
    
    wrefresh(numpad_window);

}


void draw_buttons() {

      int buttons_height = 5;
    int buttons_width = 24;
    int buttons_starty = 4;
    int buttons_startx = 0;
    buttons_win = newwin(buttons_height, buttons_width, buttons_starty, buttons_startx);

    // Create the additional buttons window

    // Draw the border around the additional buttons window
    box(buttons_win, 0, 0);

    // Set colors for the additional buttons
    wattron(buttons_win, COLOR_PAIR(2));

    // Draw the additional buttons
    mvwaddch(buttons_win, 1,6, ACS_LARROW);
    mvwaddch(buttons_win, 1,10, ACS_UARROW);
    mvwaddch(buttons_win, 1,14, ACS_RARROW);



     mvwaddch(buttons_win, 1,2, ACS_UARROW);

    // Print a triangle pointing down at position (10, 10)
    mvwaddch(buttons_win, 3, 2, ACS_DARROW);
    wattron(buttons_win, COLOR_PAIR(5));

    mvwprintw(buttons_win, 3, 14, "U");

    


    // Blue buttons

    wattron(buttons_win, COLOR_PAIR(4));
        mvwprintw(buttons_win, 1, 18, "x");
    mvwprintw(buttons_win, 1, 21, "x");
mvwprintw(buttons_win, 3, 21, "x");
    mvwprintw(buttons_win, 3, 18, "0");
    mvwprintw(buttons_win, 3, 6, "7");
    mvwprintw(buttons_win, 3, 10, "8");

    // Refresh the additional buttons window to display changes
    wrefresh(buttons_win);

}

// Function to read a line from serial port and remove checksum
int read_line(int fd, char *buffer, int max_length) {
    int i = 0;
    char c;

    while (read(fd, &c, 1) > 0) {
        if (c == '\r') {
            buffer[i] = '\0';
            return i;
        } else {
            buffer[i++] = c;
            if (i >= max_length) {
                printf("Line exceeds maximum length\n");
                return -1;
            }
        }
    }

    return -1; // EOF reached
}

// Function to parse incoming message and extract checksum
int parse_message(char *message) {
    if (strlen(message) < 2) {
        return -1; // Message too short
    }

    int checksum = message[0];

    // Shift the message to remove the checksum
    memmove(message, message + 1, strlen(message));

    return checksum;
}

// Function to calculate and prepend checksum to a string
void sign_telegram(char *message) {
    int check_byte = 0x7F;
    size_t length = strlen(message);

    // Calculate checksum
    for (size_t i = 0; i < length; i++) {
        char byte = message[i];
        check_byte = check_byte ^ byte;
    }

    // Append checksum to the message
    message[length] = check_byte;
    message[length + 1] = '\0'; // Null-terminate the string after adding checksum
}

// Function to send message over serial port with checksum
void send_message(int fd, const char *message) {
    char signed_message[MAX_LINE_LENGTH];
    strcpy(signed_message, message);
    sign_telegram(signed_message);
    write(fd, signed_message, strlen(signed_message));
    wprintw(log_window, "out-> %s\n", signed_message);
    wrefresh(log_window);
}

void str_replace(char *target, const char *needle, const char *replacement)
{
    char buffer[1024] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1) {
        const char *p = strstr(tmp, needle);

        // walked past last occurrence of needle; copy remaining part
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        // copy part before needle
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        // copy replacement string
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;

        // adjust pointers, move on
        tmp = p + needle_len;
    }

    // write altered string back to target
    strcpy(target, buffer);
}



// Function to write text to the specified line with optional blinking
void write_text(WINDOW *win, int line, int mode, int pos, char *text, bool blink) {
    if (blink) {
        attron(A_BLINK);
    }

	// Replace special characters to UTF8
	str_replace(text, "{", "ä");
	str_replace(text, "|", "ö");
	str_replace(text, "}", "ü");
	str_replace(text, "~", "ß");
	str_replace(text, "[", "Ä");
	str_replace(text, "\\", "Ö");
	str_replace(text, "]", "Ü");



    mvwprintw(win, line, pos, "%s", text);
    if (blink) {
        attroff(A_BLINK);
    }
    wrefresh(win);
}

// Signal handler for terminal resizing
void handle_resize(int sig) {
    endwin(); // End curses mode
    refresh(); // Refresh the screen
    clear(); // Clear the screen
    refresh(); // Refresh the screen again to ensure it's cleared
    // Reinitialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_GREEN);
}

