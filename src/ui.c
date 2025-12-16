#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include "ui.h"
#include "defs.h"

// --- History Storage ---
static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;

// --- Terminal Settings ---
static struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    // Disable ECHO (typing) and ICANON (line buffering)
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void add_to_history(const char *cmd) {
    if (strlen(cmd) == 0) return;
    if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0) return;

    if (history_count < HISTORY_MAX) {
        strcpy(history[history_count++], cmd);
    } else {
        for (int i = 1; i < HISTORY_MAX; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_MAX - 1], cmd);
    }
}

void print_banner() {
    printf(COLOR_BOLD COLOR_CYAN);
    printf("   ______      __      \n");
    printf("  / ____/___ _/ /____  \n");
    printf(" / /   / __ `/ / ___/  \n");
    printf("/ /___/ /_/ / / /__    \n");
    printf("\\____/\\__,_/_/\\___/    \n");
    printf(COLOR_RESET);
    printf("CalcPro v" VERSION " (c) 2025\n");
    printf("Type " COLOR_BOLD "'exit'" COLOR_RESET " to quit, " COLOR_BOLD "'?'" COLOR_RESET " for help.\n\n");
}

void print_help() {
    printf(COLOR_BOLD "--- CLI Calculator (Fixed) ---\n" COLOR_RESET);
    printf("Math: " COLOR_CYAN "10 + 5" COLOR_RESET ", " COLOR_CYAN "(2)(3)" COLOR_RESET ", " COLOR_CYAN "5x\n" COLOR_RESET);
    printf("Vars: " COLOR_GREEN "x=10" COLOR_RESET ", " COLOR_GREEN "y=5" COLOR_RESET ", " COLOR_GREEN "x+y\n" COLOR_RESET);
    printf("Func: " COLOR_CYAN "sin(0)" COLOR_RESET ", " COLOR_CYAN "sqrt(4)" COLOR_RESET "\n");
    printf(COLOR_BOLD "----------------------------\n" COLOR_RESET);
}

// The custom input function
void get_input_raw(char *buffer) {
    int pos = 0;      // Cursor position
    int len = 0;      // String length
    int history_index = history_count;
    char c;
    buffer[0] = '\0';

    printf(COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET);
    fflush(stdout);

    while (1) {
        if (read(STDIN_FILENO, &c, 1) == -1) break;

        if (c == '\n' || c == '\r') {
            buffer[len] = '\0';
            printf("\r\n");
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                // Shift left
                memmove(&buffer[pos - 1], &buffer[pos], len - pos);
                pos--;
                len--;
                buffer[len] = '\0';
                
                // Update display: move back, print remainder, print space, move back enough
                printf("\b%s \033[%dD", &buffer[pos], len - pos + 1);
                fflush(stdout);
            }
        } else if (c == '\033') { // Escape Sequence
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == -1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == -1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') { // UP
                    if (history_index > 0) {
                        history_index--;
                        strcpy(buffer, history[history_index]);
                        len = strlen(buffer);
                        pos = len;
                        printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K", buffer);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { // DOWN
                    if (history_index < history_count) {
                        history_index++;
                        if (history_index == history_count) {
                            buffer[0] = '\0';
                            len = 0;
                            pos = 0;
                        } else {
                            strcpy(buffer, history[history_index]);
                            len = strlen(buffer);
                            pos = len;
                        }
                        printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K", buffer);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'D') { // LEFT
                    if (pos > 0) {
                        pos--;
                        printf("\033[D");
                        fflush(stdout);
                    }
                } else if (seq[1] == 'C') { // RIGHT
                    if (pos < len) {
                        pos++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                }
            }
        } else if (!iscntrl(c) && len < BUFFER_SIZE - 1) {
            // Insert character
            memmove(&buffer[pos + 1], &buffer[pos], len - pos);
            buffer[pos] = c;
            len++;
            pos++;
            buffer[len] = '\0';
            
            // Print from cursor
            printf("%s", &buffer[pos - 1]);
            
            // Restore cursor if needed
            if (len > pos) {
                printf("\033[%dD", len - pos);
            }
            fflush(stdout);
        }
    }
}
 
