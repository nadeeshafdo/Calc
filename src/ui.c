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

void disableRawMode(void) {
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
    strncpy(history[history_count], cmd, BUFFER_SIZE - 1);
    history[history_count][BUFFER_SIZE - 1] = 0;
    history_count++;
  } else {
    for (int i = 1; i < HISTORY_MAX; i++) {
      strncpy(history[i - 1], history[i], BUFFER_SIZE - 1);
      history[i - 1][BUFFER_SIZE - 1] = 0;
    }
    strncpy(history[HISTORY_MAX - 1], cmd, BUFFER_SIZE - 1);
    history[HISTORY_MAX - 1][BUFFER_SIZE - 1] = 0;
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
            len = (int)strlen(buffer);
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
              len = (int)strlen(buffer);
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
            len = (int)strlen(buffer);
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
              len = (int)strlen(buffer);
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
  int len = (int)strlen(func_ref);
  int k = 0;
  for (int i = 0; i < len && k < VAR_NAME_LEN - 1; i++) {
    if (isalnum((unsigned char)func_ref[i]))
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
    strncpy(expr, func_ref, BUFFER_SIZE - 1);
    expr[BUFFER_SIZE - 1] = 0;
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

/* ================= Native TUI (v3.0.0) ================= */

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static int tui_rows = 24, tui_cols = 80;
static char **tui_log = NULL;
static int tui_log_count = 0, tui_log_cap = 0;
static char tui_input[BUFFER_SIZE];
static int tui_input_len = 0, tui_input_pos = 0;
static int tui_help = 0;
static int tui_graph_nav = 0;
static char tui_graph_expr[BUFFER_SIZE] = "sin(x)";
static int tui_has_graph = 1;
static double tui_gxmin = -10, tui_gxmax = 10, tui_gymin = -10, tui_gymax = 10;

static char **tui_history = NULL;
static int tui_hist_count = 0, tui_hist_cap = 0, tui_hist_idx = 0;

static void tui_log_add(const char *s) {
  if (tui_log_count >= tui_log_cap) {
    int nc = tui_log_cap ? tui_log_cap * 2 : 64;
    char **tmp = realloc(tui_log, nc * sizeof(char *));
    if (!tmp) return;
    tui_log = tmp;
    tui_log_cap = nc;
  }
  tui_log[tui_log_count] = malloc(strlen(s) + 1);
  if (tui_log[tui_log_count]) strcpy(tui_log[tui_log_count], s);
  tui_log_count++;
}

static void tui_history_add(const char *s) {
  if (tui_hist_count >= tui_hist_cap) {
    int nc = tui_hist_cap ? tui_hist_cap * 2 : 64;
    char **tmp = realloc(tui_history, nc * sizeof(char *));
    if (!tmp) return;
    tui_history = tmp;
    tui_hist_cap = nc;
  }
  if (tui_hist_count > 0 && strcmp(tui_history[tui_hist_count - 1], s) == 0) return;
  tui_history[tui_hist_count] = malloc(strlen(s) + 1);
  if (tui_history[tui_hist_count]) strcpy(tui_history[tui_hist_count], s);
  tui_hist_count++;
}

static void tui_get_size(void) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (GetConsoleScreenBufferInfo(h, &csbi)) {
    tui_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    tui_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  } else {
    tui_cols = 80; tui_rows = 24;
  }
#else
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    tui_cols = ws.ws_col;
    tui_rows = ws.ws_row;
  } else {
    tui_cols = 80; tui_rows = 24;
  }
#endif
  if (tui_cols < 40) tui_cols = 40;
  if (tui_rows < 10) tui_rows = 10;
}

static void tui_move(int r, int c) {
  printf("\033[%d;%dH", r, c);
}

static void tui_clear(void) {
  printf("\033[2J\033[H");
}

/* process a single command token, log output */
static void tui_process_token(char *token);

static void tui_process_line(const char *line) {
  char work[BUFFER_SIZE];
  strncpy(work, line, BUFFER_SIZE - 1);
  work[BUFFER_SIZE - 1] = 0;
  char *tok = strtok(work, ";");
  while (tok) {
    while (isspace((unsigned char)*tok)) tok++;
    char *end = tok + strlen(tok) - 1;
    while (end > tok && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    if (strlen(tok) > 0) {
      char logline[BUFFER_SIZE + 16];
      snprintf(logline, sizeof(logline), "calc> %s", tok);
      tui_log_add(logline);
      tui_process_token(tok);
    }
    tok = strtok(NULL, ";");
  }
}

static void tui_process_token(char *token) {
  if (strcmp(token, "vars") == 0) {
    tui_log_add("Defined Variables:");
    for (int i = 0; i < var_count; i++) {
      char buf[BUFFER_SIZE];
      snprintf(buf, sizeof(buf), "  %s = %.6g", variables[i].name, variables[i].value);
      tui_log_add(buf);
    }
    if (var_count == 0) tui_log_add("  (none)");
  } else if (strcmp(token, "funcs") == 0) {
    tui_log_add("Defined Functions:");
    for (int i = 0; i < func_count; i++) {
      char buf[BUFFER_SIZE * 2];
      snprintf(buf, sizeof(buf), "  %s(%s) = %s", functions[i].name, functions[i].param, functions[i].expr);
      tui_log_add(buf);
    }
    if (func_count == 0) tui_log_add("  (none)");
  } else if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
    /* handled by caller */
  } else if (strncmp(token, "unset ", 6) == 0) {
    char var_name[VAR_NAME_LEN];
    int i = 6;
    while (token[i] && isspace((unsigned char)token[i])) i++;
    int k = 0;
    while (token[i] && !isspace((unsigned char)token[i]) && k < VAR_NAME_LEN - 1) var_name[k++] = token[i++];
    var_name[k] = 0;
    if (k > 0) {
      if (is_variable_defined(var_name)) { unset_variable(var_name); 
        char buf[BUFFER_SIZE]; snprintf(buf,sizeof(buf),"Variable '%s' unset.",var_name); tui_log_add(buf);}
      else if (is_user_function_defined(var_name)) { unset_function(var_name);
        char buf[BUFFER_SIZE]; snprintf(buf,sizeof(buf),"Function '%s' unset.",var_name); tui_log_add(buf);}
      else { char buf[BUFFER_SIZE]; snprintf(buf,sizeof(buf),"Error: '%s' not found.",var_name); tui_log_add(buf);}
    }
  } else if (strncmp(token, "plot(", 5) == 0) {
    char content[BUFFER_SIZE];
    int i = 5, k = 0, depth = 1;
    while (token[i] && depth > 0) {
      if (token[i] == '(') depth++;
      if (token[i] == ')') depth--;
      if (depth > 0 && k < BUFFER_SIZE - 1) content[k++] = token[i++];
      else i++;
    }
    content[k] = 0;
    /* parse optional ranges */
    /* split by top-level commas */
    /* first part expr */
    char expr[BUFFER_SIZE];
    expr[0] = 0;
    double xmin=-10,xmax=10,ymin=-10,ymax=10;
    /* simple parse: find commas */
    int commas[4], nc=0, d=0;
    for (int j=0; content[j]; j++) {
      if (content[j]=='(') d++;
      else if (content[j]==')') d--;
      else if (content[j]==',' && d==0) { if (nc<4) commas[nc++]=j; }
    }
    int start=0;
    int end = nc>0 ? commas[0] : (int)strlen(content);
    strncpy(expr, content+start, end-start); expr[end-start]=0;
    /* trim expr */
    /* parse numbers */
    if (nc>=1) {
      char numbuf[64]; int idx=commas[0]+1; int e2 = nc>=2?commas[1]:(int)strlen(content);
      int len=e2-idx; if(len>63) len=63; strncpy(numbuf,content+idx,len); numbuf[len]=0; xmin=atof(numbuf);
    }
    if (nc>=2) {
      char numbuf[64]; int idx=commas[1]+1; int e2 = nc>=3?commas[2]:(int)strlen(content);
      int len=e2-idx; if(len>63) len=63; strncpy(numbuf,content+idx,len); numbuf[len]=0; xmax=atof(numbuf);
    }
    if (nc>=3) {
      char numbuf[64]; int idx=commas[2]+1; int e2 = nc>=4?commas[3]:(int)strlen(content);
      int len=e2-idx; if(len>63) len=63; strncpy(numbuf,content+idx,len); numbuf[len]=0; ymin=atof(numbuf);
    }
    if (nc>=4) {
      char numbuf[64]; int idx=commas[3]+1; int e2=(int)strlen(content);
      int len=e2-idx; if(len>63) len=63; strncpy(numbuf,content+idx,len); numbuf[len]=0; ymax=atof(numbuf);
    }
    /* trim expr spaces */
    char *p=expr; while(isspace((unsigned char)*p)) p++;
    /* store */
    strncpy(tui_graph_expr, p, BUFFER_SIZE-1); tui_graph_expr[BUFFER_SIZE-1]=0;
    /* if expr is just a function name, keep; plotting uses tui_graph_expr */
    tui_gxmin=xmin; tui_gxmax=xmax; tui_gymin=ymin; tui_gymax=ymax;
    tui_has_graph=1;
    char buf[BUFFER_SIZE+64];
    snprintf(buf,sizeof(buf),"Plotting: %s  x:[%.3g,%.3g] y:[%.3g,%.3g]", tui_graph_expr, xmin,xmax,ymin,ymax);
    tui_log_add(buf);
  } else {
    /* function def? */
    char *po = strchr(token, '(');
    char *pc = strchr(token, ')');
    char *eq = strchr(token, '=');
    if (po && pc && eq && po < pc && pc < eq) {
      char name[VAR_NAME_LEN], param[VAR_NAME_LEN];
      int valid=1, n=0,m=0;
      char *p=token;
      while (p < po) {
        if (!isspace((unsigned char)*p)) {
          if (!isalnum((unsigned char)*p)) valid=0;
          if (n < VAR_NAME_LEN-1) name[n++]=*p; else valid=0;
        }
        p++;
      }
      name[n]=0;
      if (n==0) valid=0;
      if (n>0 && isdigit((unsigned char)name[0])) valid=0;
      if (valid && is_function_name(name)) valid=0;
      p=po+1;
      while (p < pc) {
        if (!isspace((unsigned char)*p)) {
          if (!isalnum((unsigned char)*p)) valid=0;
          if (m < VAR_NAME_LEN-1) param[m++]=*p; else valid=0;
        }
        p++;
      }
      param[m]=0;
      if (m==0) valid=0;
      if (m>0 && isdigit((unsigned char)param[0])) valid=0;
      if (valid) {
        set_function(name, param, eq+1);
        char buf[BUFFER_SIZE]; snprintf(buf,sizeof(buf),"Defined function %s(%s)",name,param);
        tui_log_add(buf);
      } else {
        tui_log_add("Error: Invalid function definition.");
      }
    } else if (eq) {
      /* assignment or equation */
      /* simple var assignment? */
      int len = (int)(eq - token);
      char lhs[BUFFER_SIZE];
      int k=0,j=0,valid=1;
      while (j < len && isspace((unsigned char)token[j])) j++;
      if (j < len && isalpha((unsigned char)token[j])) {
        while (j < len && isalnum((unsigned char)token[j]) && k < VAR_NAME_LEN-1) lhs[k++]=token[j++];
        lhs[k]=0;
        while (j < len && isspace((unsigned char)token[j])) j++;
        if (j != len) valid=0;
      } else valid=0;
      if (valid) {
        CalcResult rhs = evaluate_expression(eq+1);
        if (rhs.error==0) set_variable(lhs, rhs.result);
        char buf[BUFFER_SIZE];
        if (rhs.error==1) snprintf(buf,sizeof(buf),"Error: Division by zero.");
        else if (rhs.error==2) snprintf(buf,sizeof(buf),"Error: Invalid Operator.");
        else if (rhs.error==3) snprintf(buf,sizeof(buf),"Error: Invalid expression.");
        else if (rhs.error==4) snprintf(buf,sizeof(buf),"Error: Unknown variable.");
        else if (rhs.error==5) snprintf(buf,sizeof(buf),"Error: Invalid assignment.");
        else if (rhs.error==6) snprintf(buf,sizeof(buf),"Error: Invalid Argument.");
        else snprintf(buf,sizeof(buf),"= %.6g", rhs.result);
        tui_log_add(buf);
      } else {
        if (strchr(token, ';')) solve_system(token);
        else solve_equation(token);
        char *line = strtok(solve_output, "\n");
        while (line) {
          tui_log_add(line);
          line = strtok(NULL, "\n");
        }
        if (solve_output[0] == 0) tui_log_add("No output.");
      }
    } else {
      CalcResult res = evaluate_expression(token);
      char buf[BUFFER_SIZE];
      if (res.error==1) snprintf(buf,sizeof(buf),"Error: Division by zero.");
      else if (res.error==2) snprintf(buf,sizeof(buf),"Error: Invalid Operator.");
      else if (res.error==3) snprintf(buf,sizeof(buf),"Error: Invalid expression.");
      else if (res.error==4) snprintf(buf,sizeof(buf),"Error: Unknown variable.");
      else if (res.error==5) snprintf(buf,sizeof(buf),"Error: Invalid assignment.");
      else if (res.error==6) snprintf(buf,sizeof(buf),"Error: Invalid Argument.");
      else snprintf(buf,sizeof(buf),"= %.6g", res.result);
      tui_log_add(buf);
    }
  }
}

/* Braille graph rendering */
static void tui_draw_graph(int r0, int c0, int w, int h) {
  if (w < 4 || h < 4) return;
  int Wp = w * 2;
  int Hp = h * 4;
  /* pixel buffer */
  char *pix = calloc(Wp * Hp, 1);
  if (!pix) return;
#define SETPX(px,py) do { if ((px)>=0 && (px)<Wp && (py)>=0 && (py)<Hp) pix[(py)*Wp+(px)] = 1; } while(0)
  /* determine param/expr */
  char expr[BUFFER_SIZE];
  char param[VAR_NAME_LEN];
  char name[VAR_NAME_LEN];
  /* extract leading alnum name */
  int nk=0;
  for (int i=0; tui_graph_expr[i] && nk<VAR_NAME_LEN-1; i++) {
    if (isalnum((unsigned char)tui_graph_expr[i])) name[nk++]=tui_graph_expr[i];
    else break;
  }
  name[nk]=0;
  if (is_user_function_defined(name)) {
    get_function_expr(name, param, expr);
  } else {
    strcpy(expr, tui_graph_expr);
    strcpy(param, "x");
  }
  /* sample */
  int prev_py = -1;
  int prev_valid = 0;
  for (int px = 0; px < Wp; px++) {
    double x = tui_gxmin + (double)px / (double)(Wp - 1) * (tui_gxmax - tui_gxmin);
    set_variable(param, x);
    CalcResult res = evaluate_expression(expr);
    if (res.error) { prev_valid = 0; continue; }
    double yval = res.result;
    int py = (int)((tui_gymax - yval) / (tui_gymax - tui_gymin) * (Hp - 1));
    if (py < 0) py = 0;
    if (py >= Hp) py = Hp - 1;
    SETPX(px, py);
    if (prev_valid) {
      int a = prev_py, b = py;
      if (a > b) { int t=a; a=b; b=t; }
      for (int yy = a; yy <= b; yy++) SETPX(px-1 < 0 ? 0 : px-1, yy);
      /* vertical line at current column between prev and cur */
      for (int yy = a; yy <= b; yy++) SETPX(px, yy);
    }
    prev_py = py;
    prev_valid = 1;
  }
  /* axes */
  if (tui_gxmin <= 0 && tui_gxmax >= 0) {
    int ax = (int)((0 - tui_gxmin) / (tui_gxmax - tui_gxmin) * (Wp - 1));
    if (ax < 0) ax = 0;
    if (ax >= Wp) ax = Wp - 1;
    for (int py = 0; py < Hp; py++) SETPX(ax, py);
  }
  if (tui_gymin <= 0 && tui_gymax >= 0) {
    int ay = (int)((tui_gymax - 0) / (tui_gymax - tui_gymin) * (Hp - 1));
    if (ay < 0) ay = 0;
    if (ay >= Hp) ay = Hp - 1;
    for (int px = 0; px < Wp; px++) SETPX(px, ay);
  }
  /* restore param if needed */
  /* render braille cells */
  for (int cy = 0; cy < h; cy++) {
    tui_move(r0 + cy, c0);
    for (int cx = 0; cx < w; cx++) {
      int mask = 0;
      for (int dy = 0; dy < 4; dy++) {
        for (int dx = 0; dx < 2; dx++) {
          int px = cx*2+dx;
          int py = cy*4+dy;
          if (px < Wp && py < Hp && pix[py*Wp+px]) {
            if (dx == 0) {
              static const int m0[4] = {0x01,0x02,0x04,0x40};
              mask |= m0[dy];
            } else {
              static const int m1[4] = {0x08,0x10,0x20,0x80};
              mask |= m1[dy];
            }
          }
        }
      }
      unsigned char b1 = 0xE2;
      unsigned char b2 = 0xA0 | (mask >> 6);
      unsigned char b3 = 0x80 | (mask & 0x3F);
      char cell[4] = { (char)b1, (char)b2, (char)b3, 0 };
      fputs(cell, stdout);
    }
  }
  free(pix);
  /* corner labels */
  tui_move(r0, c0);
  printf("x:[%.2g,%.2g]", tui_gxmin, tui_gxmax);
  tui_move(r0 + h - 1, c0);
  printf("y:[%.2g,%.2g]", tui_gymin, tui_gymax);
}

static void tui_render(void) {
  tui_get_size();
  tui_clear();
  /* status bar */
  tui_move(1, 1);
  printf("\033[7mCalc %s\033[0m", VERSION);
  /* keybinds right */
  const char *hints = "Ctrl-H:Help | Ctrl-L:Clear | Ctrl-G:Graph | Ctrl-C:Quit";
  int hl = (int)strlen(hints);
  tui_move(1, tui_cols - hl + 1);
  printf("\033[7m%s\033[0m", hints);

  int left_w = tui_cols * 45 / 100;
  int right_w = tui_cols - left_w - 1;
  int main_h = tui_rows - 3; /* status + input + maybe */
  if (main_h < 5) main_h = 5;

  /* left pane log */
  int log_lines_avail = main_h - 4;
  if (log_lines_avail < 1) log_lines_avail = 1;
  int start = tui_log_count - log_lines_avail;
  if (start < 0) start = 0;
  for (int i = 0; i < log_lines_avail; i++) {
    int idx = start + i;
    tui_move(2 + i, 1);
    if (idx < tui_log_count && tui_log[idx]) {
      /* truncate */
      char buf[256];
      strncpy(buf, tui_log[idx], left_w - 1);
      buf[left_w - 1] = 0;
      printf("%s", buf);
    }
  }
  /* vars/funcs compact panel */
  int panel_row = 2 + log_lines_avail;
  tui_move(panel_row, 1);
  printf("vars:%d funcs:%d", var_count, func_count);
  /* list a few */
  for (int i = 0; i < var_count && i < 3; i++) {
    tui_move(panel_row + 1 + i, 1);
    printf("%s=%.4g", variables[i].name, variables[i].value);
  }

  /* right pane */
  int right_c0 = left_w + 2;
  if (tui_help) {
    /* help overlay */
    const char *help[] = {
      "Calc Help",
      "Ctrl-C quit, Ctrl-H help, Ctrl-L clear",
      "Ctrl-G graph nav: +/- zoom, arrows pan, r reset, Esc exit",
      "plot(expr) or plot(expr,xmin,xmax,ymin,ymax)",
      "vars, funcs, unset x, f(x)=expr",
    };
    for (int i = 0; i < 5; i++) {
      tui_move(2 + i, right_c0);
      printf("%s", help[i]);
    }
  } else if (tui_has_graph) {
    tui_draw_graph(2, right_c0, right_w, main_h);
  } else {
    tui_move(2, right_c0);
    printf("No graph. Use plot(expr).");
  }

  /* input bar */
  tui_move(tui_rows, 1);
  printf("calc> ");
  /* print input */
  /* simple: print buffer */
  printf("%s", tui_input);
  /* move cursor */
  tui_move(tui_rows, 7 + tui_input_pos);
  fflush(stdout);
}

static void tui_enable_raw(void) {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(hOut, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
  }
  SetConsoleOutputCP(CP_UTF8);
  /* input */
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (GetConsoleMode(hIn, &mode)) {
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(hIn, mode);
  }
#else
  /* reuse enableRawMode? call termios */
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

static void tui_disable_raw(void) {
#ifdef _WIN32
  /* no-op restore minimal */
#else
  /* restore cooked not tracked; exit anyway */
#endif
}

void run_tui(void) {
  tui_enable_raw();
  /* alternate screen */
  fputs("\033[?1049h\033[?25l", stdout);
  fflush(stdout);
  tui_log_add("Calc " VERSION " (c) 2025 nadeeshafdo");
  tui_log_add("TUI mode. Ctrl-H help, Ctrl-G graph nav, Ctrl-C quit.");

  /* initial graph */
  tui_has_graph = 1;

  int running = 1;
  while (running) {
    tui_render();
#ifdef _WIN32
    int c = _getch();
#else
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) break;
    int c = ch;
    if (c == 27) {
      /* escape sequence */
      char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
        if (seq[0] == '[') {
          if (seq[1] == 'A') c = 1000; /* up */
          else if (seq[1] == 'B') c = 1001; /* down */
          else if (seq[1] == 'C') c = 1002; /* right */
          else if (seq[1] == 'D') c = 1003; /* left */
        }
      } else {
        c = 27;
      }
    }
#endif
    if (tui_graph_nav) {
      if (c == 27 || c == 7) { tui_graph_nav = 0; continue; } /* Esc or Ctrl-G */
      if (c == '+' || c == '=') { double cx=(tui_gxmin+tui_gxmax)/2; double hw=(tui_gxmax-tui_gxmin)*0.4; tui_gxmin=cx-hw; tui_gxmax=cx+hw; double ch2=(tui_gymax-tui_gymin)*0.4; double cy=(tui_gymin+tui_gymax)/2; tui_gymin=cy-ch2; tui_gymax=cy+ch2; }
      else if (c == '-') { double cx=(tui_gxmin+tui_gxmax)/2; double hw=(tui_gxmax-tui_gxmin)*1.25; tui_gxmin=cx-hw; tui_gxmax=cx+hw; double ch2=(tui_gymax-tui_gymin)*1.25; double cy=(tui_gymin+tui_gymax)/2; tui_gymin=cy-ch2; tui_gymax=cy+ch2; }
      else if (c == 1000 || c == 1003) { /* up/left pan */ 
        if (c == 1000) { double d=(tui_gymax-tui_gymin)*0.1; tui_gymin+=d; tui_gymax+=d; }
        else { double d=(tui_gxmax-tui_gxmin)*0.1; tui_gxmin-=d; tui_gxmax-=d; }
      }
      else if (c == 1001 || c == 1002) {
        if (c == 1001) { double d=(tui_gymax-tui_gymin)*0.1; tui_gymin-=d; tui_gymax-=d; }
        else { double d=(tui_gxmax-tui_gxmin)*0.1; tui_gxmin+=d; tui_gxmax+=d; }
      }
      else if (c == 'r') { tui_gxmin=-10; tui_gxmax=10; tui_gymin=-10; tui_gymax=10; }
      continue;
    }

    if (c == 3) { running = 0; break; } /* Ctrl-C */
    if (c == 7) { tui_graph_nav = 1; continue; } /* Ctrl-G */
    if (c == 8) { tui_help = !tui_help; continue; } /* Ctrl-H */
    if (c == 12) { /* Ctrl-L clear log */
      tui_log_count = 0;
      continue;
    }
    if (c == '\r' || c == '\n') {
      tui_input[tui_input_len] = 0;
      if (tui_input_len > 0) {
        tui_history_add(tui_input);
        if (strcmp(tui_input, "exit")==0 || strcmp(tui_input, "quit")==0) { running=0; break; }
        tui_process_line(tui_input);
      }
      tui_input_len = 0; tui_input_pos = 0;
      tui_input[0] = 0;
      continue;
    }
    if (c == 127 || c == 8) {
      if (tui_input_pos > 0) {
        memmove(tui_input + tui_input_pos - 1, tui_input + tui_input_pos, tui_input_len - tui_input_pos);
        tui_input_pos--; tui_input_len--;
        tui_input[tui_input_len] = 0;
      }
      continue;
    }
    if (c == 1003) { /* left */
      if (tui_input_pos > 0) tui_input_pos--;
      continue;
    }
    if (c == 1002) { /* right */
      if (tui_input_pos < tui_input_len) tui_input_pos++;
      continue;
    }
    if (c == 1000) { /* up history */
      if (tui_hist_count > 0) {
        if (tui_hist_idx > 0) tui_hist_idx--;
        strcpy(tui_input, tui_history[tui_hist_idx]);
        tui_input_len = (int)strlen(tui_input);
        tui_input_pos = tui_input_len;
      }
      continue;
    }
    if (c == 1001) { /* down history */
      if (tui_hist_idx < tui_hist_count - 1) {
        tui_hist_idx++;
        strcpy(tui_input, tui_history[tui_hist_idx]);
        tui_input_len = (int)strlen(tui_input);
        tui_input_pos = tui_input_len;
      } else {
        tui_hist_idx = tui_hist_count;
        tui_input[0]=0; tui_input_len=0; tui_input_pos=0;
      }
      continue;
    }
    /* printable */
    if (c >= 32 && c < 127 && tui_input_len < BUFFER_SIZE - 2) {
      memmove(tui_input + tui_input_pos + 1, tui_input + tui_input_pos, tui_input_len - tui_input_pos);
      tui_input[tui_input_pos] = (char)c;
      tui_input_pos++; tui_input_len++;
      tui_input[tui_input_len] = 0;
    }
  }

  /* restore */
  fputs("\033[?25h\033[?1049l", stdout);
  fflush(stdout);
  tui_disable_raw();
}
