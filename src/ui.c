#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include "defs.h"
#include "eval.h"
#include "funcs.h"
#include "ui.h"
#include "vars.h"
#include <ctype.h>
#include <math.h>

// --- History Storage ---
static char history[HISTORY_MAX][BUFFER_SIZE];
static int history_count = 0;

// --- Terminal Settings ---
#ifdef _WIN32
static DWORD orig_console_mode;
static HANDLE hConsole;
#else
static struct termios orig_termios;
#endif

void disableRawMode() {
#ifdef _WIN32
  SetConsoleMode(hConsole, orig_console_mode);
#else
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
}

void enableRawMode() {
#ifdef _WIN32
  hConsole = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hConsole, &orig_console_mode);
  atexit(disableRawMode);

  DWORD mode = orig_console_mode;
  // Disable LINE_INPUT (canonical) and ECHO_INPUT
  mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
  SetConsoleMode(hConsole, mode);
#else
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  // Disable ECHO (typing), ICANON (line buffering), and ISIG (signals)
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

void add_to_history(const char *cmd) {
  if (strlen(cmd) == 0)
    return;
  if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0)
    return;

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
  printf("Calc " VERSION " (c) 2025 nadeeshafdo\n");
  printf("Type CTRL-C to quit, CTRL-H for help.\n\n");
}

void print_help() {
  printf("\nCalc Help\n");
  printf("Commands:\n");
  printf("  CTRL-C            Quit\n");
  printf("  CTRL-H            Show this help\n");
  printf("  exit, quit        Exit the calculator\n");
  printf("  unset <var>       Unset variable or function\n");
  printf("  vars              List all defined variables\n");
  printf("  funcs             List all defined functions\n");
  printf("  plot(expr)        Plot an expression or function\n");
  printf("\nOperators:\n");
  printf("  + - * / ^         Add, sub, mul, div, power\n");
  printf("  %%                 Percent: N%% = N/100, A%%B = A*B/100\n");
  printf("\nExamples:\n");
  printf("  2 + 3 * 4         Evaluate basic arithmetic\n");
  printf("  (10 - 2) / 4      Use parentheses for grouping\n");
  printf("  500%%               Percentage (500/100 = 5)\n");
  printf("  500%%25             Percent of (500*25/100 = 125)\n");
  printf("  100 * 10%%          10%% of 100 = 10\n");
  printf("  x = 42            Assign a value to a variable\n");
  printf("  f(x) = x^2        Define a function\n");
  printf("  plot(f)           Plot the function f\n");
  printf("  plot(sin(x))      Plot an expression\n");
  printf("  vars              Show all currently stored variables\n");
  printf("\n");
}

// The custom input function
void get_input_raw(char *buffer) {
  int pos = 0; // Cursor position
  int len = 0; // String length
  int history_index = history_count;
  int c; // int to handle EOF or special chars
  buffer[0] = '\0';

  printf(COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET);
  fflush(stdout);

  while (1) {
#ifdef _WIN32
    // Windows Input
    if (_kbhit()) {
      c = _getch();
      if (c == 0 || c == 0xE0) { // Special key
        int next = _getch();
        // Map to ANSI sequences logic for shared handling or direct handle
        if (next == 72) { // UP
          if (history_index > 0) {
            history_index--;
            strcpy(buffer, history[history_index]);
            len = strlen(buffer);
            pos = len;
            printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K",
                   buffer);
          }
        } else if (next == 80) { // DOWN
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
            printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K",
                   buffer);
          }
        } else if (next == 75) { // LEFT
          if (pos > 0) {
            pos--;
            printf("\b");
          }
        } else if (next == 77) { // RIGHT
          if (pos < len) {
            pos++;
            printf("\033[C");
          } // If ANSI enabled, else need cursor API
        }
        continue;
      }
    } else {
      Sleep(10);
      continue;
    }
#else
    // POSIX Input
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == -1)
      break;
    c = (unsigned char)ch;
#endif

    if (c == 3) { // CTRL-C
      printf("\n");
      disableRawMode();
      exit(0);
    } else if (c == 8 || c == 127) { // CTRL-H or Backspace
      // Check for explicit CTRL-H on POSIX (ASCII 8) or if user meant Help
#ifdef _WIN32
      // Windows logic for Backspace (8)
      if (pos > 0) {
        memmove(&buffer[pos - 1], &buffer[pos], len - pos);
        pos--;
        len--;
        buffer[len] = '\0';
        printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s \b", buffer);
        // Move cursor back to correct position is tricky without ANSI
        // We printed the whole line again.
        // Correct way: use \r then print prompt + buffer + space + backspace
        // makes sense for end of line But complex for middle edits. Simple
        // hack: The above redraws line and puts cursor at end. Correct cursor
        // position:
        int i;
        for (i = 0; i < (len - pos); i++)
          printf("\b");
      }
      continue;
#else
// POSIX logic handled below
#endif
    }

#ifndef _WIN32
    if (c == 8) { // Explicit CTRL-H on POSIX (Backspace usually 127)
      print_help();
      printf(COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s", buffer);
      fflush(stdout);
      continue;
    }

    if (c == '\n' || c == '\r') {
      buffer[len] = '\0';
      printf("\r\n");
      break;
    } else if (c == 127) { // POSIX Backspace
      if (pos > 0) {
        memmove(&buffer[pos - 1], &buffer[pos], len - pos);
        pos--;
        len--;
        buffer[len] = '\0';
        printf("\b%s \033[%dD", &buffer[pos], len - pos + 1);
        fflush(stdout);
      }
    } else if (c == '\033') { // Escape Sequence
      char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) == -1)
        continue;
      if (read(STDIN_FILENO, &seq[1], 1) == -1)
        continue;

      if (seq[0] == '[') {
        if (seq[1] == 'A') { // UP
          if (history_index > 0) {
            history_index--;
            strcpy(buffer, history[history_index]);
            len = strlen(buffer);
            pos = len;
            printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K",
                   buffer);
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
            printf("\r" COLOR_BOLD COLOR_CYAN "calc> " COLOR_RESET "%s\033[K",
                   buffer);
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
      memmove(&buffer[pos + 1], &buffer[pos], len - pos);
      buffer[pos] = c;
      len++;
      pos++;
      buffer[len] = '\0';
      printf("%s", &buffer[pos - 1]);
      if (len > pos) {
        printf("\033[%dD", len - pos);
      }
      fflush(stdout);
    }
#else
    // Windows Simple Insert (Non-POSIX block)
    if (c == '\r' || c == '\n') {
      buffer[len] = '\0';
      printf("\n");
      break;
    } else if (isprint(c) && len < BUFFER_SIZE - 1) {
      buffer[len++] = c;
      buffer[len] = '\0';
      pos++;
      printf("%c", c);
    }
#endif
  }
}
// --- Plotting ---
void plot_function(const char *func_ref) {
  // Determine if it is a function name or expression
  // For now, assume it's a function name like f(x) or just f
  // Or an expression f(x).

  // Simplified: Expect "f" or "f(x)" and find the function 'f'.
  // Or if valid expression with 'x', plot it.

  char expr[BUFFER_SIZE];
  char name[VAR_NAME_LEN];

  // Check if func_ref matches a function name
  // Strip (x) if present
  int len = strlen(func_ref);
  int k = 0;
  for (int i = 0; i < len; i++) {
    if (isalnum(func_ref[i]))
      name[k++] = func_ref[i];
    else
      break;
  }
  name[k] = '\0';

  char param[VAR_NAME_LEN];

  if (is_user_function_defined(name)) {
    // Use the function's expression
    get_function_expr(name, param, expr);
    printf(COLOR_CYAN "Plotting function %s(%s) = %s\n" COLOR_RESET, name,
           param, expr);
  } else {
    // Treat as expression with param 'x'
    strcpy(expr, func_ref);
    strcpy(param, "x");
    printf(COLOR_CYAN "Plotting expression %s (param: %s)\n" COLOR_RESET, expr,
           param);
  }

  // Plotting Config
  double min_x = -10;
  double max_x = 10;
  double min_y = -10;
  double max_y = 10;
  int width = 60;
  int height = 20;

  double old_val_param;
  int had_param = get_variable(param, &old_val_param);
  // If param is not 'x', we might need to save 'x' too if we standardized on
  // it, but here we just use 'param' variable for evaluation.

  for (int row = 0; row < height; row++) {
    // y goes from max_y to min_y
    double y = max_y - (row * (max_y - min_y) / (height - 1));

    for (int col = 0; col < width; col++) {
      double x = min_x + (col * (max_x - min_x) / (width - 1));

      // Evaluate
      set_variable(param, x);
      CalcResult res = evaluate_expression(expr);

      // Check if y is close to res.result
      if (res.error == 0) {
        double val = res.result;
        double y_step = (max_y - min_y) / (height - 1);
        if (fabs(val - y) < y_step / 2.0) {
          printf("*");
        } else if (fabs(x) < 0.1 && fabs(y) < 0.1) {
          printf("+"); // Origin
        } else if (fabs(x) < 0.1) {
          printf("|"); // Y-axis
        } else if (fabs(y) < 0.1) {
          printf("-"); // X-axis
        } else {
          printf(" ");
        }
      } else {
        // If error (e.g. sqrt(-1)), just print space or specific char?
        // printf("?");
        printf(" ");
      }
    }
    printf("\n");
  }

  // Restore
  if (had_param)
    set_variable(param, old_val_param);
  else
    unset_variable(param);
}
