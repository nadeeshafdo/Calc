#include "defs.h"
#include "eval.h"
#include "funcs.h"
#include "ui.h"
#include "vars.h"
#include "manatui.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static Application *app;
static TextArea *output;
static TextInput *input;

static void out_line(const char *fmt, ...) {
  char buf[BUFFER_SIZE * 4];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  textarea_add_line(output, buf);
}

static void process_token(char *token);

static void handle_input_key(int c, void *ctx) {
  (void)ctx;
  if (c == '\n' || c == '\r' || c == KEY_ENTER) {
    /* evaluate current input */
    char buf[BUFFER_SIZE];
    strncpy(buf, input->content, BUFFER_SIZE - 1);
    buf[BUFFER_SIZE - 1] = '\0';

    /* trim */
    char *t = buf;
    while (isspace((unsigned char)*t)) t++;
    if (strlen(t) > 0) {
      out_line("calc> %s", t);
      /* tokenize by ';' */
      char work[BUFFER_SIZE];
      strncpy(work, t, BUFFER_SIZE - 1);
      work[BUFFER_SIZE - 1] = '\0';
      char *tok = strtok(work, ";");
      while (tok != NULL) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
        if (strlen(tok) > 0) {
          process_token(tok);
        }
        tok = strtok(NULL, ";");
      }
    }

    /* clear input */
    input->content[0] = '\0';
    input->content_size = 0;
    input->cursor_pos = 0;
    textinput_render(app, input);
    textarea_render(app, output);
  }
}

static void process_token(char *token) {
  if (strcmp(token, "vars") == 0) {
    if (var_count == 0) {
      out_line("No variables defined.");
    } else {
      out_line("Defined Variables:");
      for (int i = 0; i < var_count; i++) {
        out_line("  %s = %.6g", variables[i].name, variables[i].value);
      }
    }
  } else if (strcmp(token, "funcs") == 0) {
    if (func_count == 0) {
      out_line("No functions defined.");
    } else {
      out_line("Defined Functions:");
      for (int i = 0; i < func_count; i++) {
        out_line("  %s(%s) = %s", functions[i].name, functions[i].param,
                 functions[i].expr);
      }
    }
  } else if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
    manatui_end(app);
    exit(0);
  } else if (strncmp(token, "unset ", 6) == 0) {
    char var_name[VAR_NAME_LEN];
    int i = 6;
    while (token[i] && isspace((unsigned char)token[i])) i++;
    int k = 0;
    while (token[i] && !isspace((unsigned char)token[i]) && k < VAR_NAME_LEN - 1) {
      var_name[k++] = token[i++];
    }
    var_name[k] = '\0';
    if (k > 0) {
      if (is_variable_defined(var_name)) {
        unset_variable(var_name);
        out_line("Variable '%s' unset.", var_name);
      } else if (is_user_function_defined(var_name)) {
        unset_function(var_name);
        out_line("Function '%s' unset.", var_name);
      } else {
        out_line("Error: Variable/Function '%s' not found.", var_name);
      }
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
    content[k] = '\0';

    /* plot into output */
    char expr[BUFFER_SIZE];
    char name[VAR_NAME_LEN];
    int len = strlen(content);
    int nk = 0;
    for (int j = 0; j < len && nk < VAR_NAME_LEN - 1; j++) {
      if (isalnum((unsigned char)content[j])) name[nk++] = content[j];
      else break;
    }
    name[nk] = '\0';

    char param[VAR_NAME_LEN];
    if (is_user_function_defined(name)) {
      get_function_expr(name, param, expr);
      out_line("Plotting function %s(%s) = %s", name, param, expr);
    } else {
      strcpy(expr, content);
      strcpy(param, "x");
      out_line("Plotting expression %s (param: %s)", expr, param);
    }

    double min_x = -10, max_x = 10, min_y = -10, max_y = 10;
    int width = 60, height = 20;
    double old_val;
    int had = get_variable(param, &old_val);

    for (int row = 0; row < height; row++) {
      double y = max_y - (row * (max_y - min_y) / (height - 1));
      char line[128];
      int li = 0;
      for (int col = 0; col < width; col++) {
        double x = min_x + (col * (max_x - min_x) / (width - 1));
        set_variable(param, x);
        CalcResult res = evaluate_expression(expr);
        char ch = ' ';
        if (res.error == 0) {
          double val = res.result;
          double y_step = (max_y - min_y) / (height - 1);
          if (fabs(val - y) < y_step / 2.0) ch = '*';
          else if (fabs(x) < 0.1 && fabs(y) < 0.1) ch = '+';
          else if (fabs(x) < 0.1) ch = '|';
          else if (fabs(y) < 0.1) ch = '-';
        }
        if (li < (int)sizeof(line) - 1) line[li++] = ch;
      }
      line[li] = '\0';
      out_line("%s", line);
    }
    if (had) set_variable(param, old_val);
    else unset_variable(param);
  } else {
    char *paren_open = strchr(token, '(');
    char *paren_close = strchr(token, ')');
    char *eq_sign = strchr(token, '=');

    if (paren_open && paren_close && eq_sign && paren_open < paren_close &&
        paren_close < eq_sign) {
      char name[VAR_NAME_LEN];
      char param[VAR_NAME_LEN];
      int valid = 1;
      int n = 0, m = 0;
      char *p = token;
      while (p < paren_open) {
        if (!isspace((unsigned char)*p)) {
          if (n < VAR_NAME_LEN - 1) name[n++] = *p;
        }
        p++;
      }
      name[n] = '\0';
      if (n == 0) valid = 0;

      p = paren_open + 1;
      while (p < paren_close) {
        if (!isspace((unsigned char)*p)) {
          if (!isalnum((unsigned char)*p)) valid = 0;
          if (m < VAR_NAME_LEN - 1) param[m++] = *p;
        }
        p++;
      }
      param[m] = '\0';
      if (m == 0) valid = 0;
      if (m > 0 && isdigit((unsigned char)param[0])) valid = 0;

      if (valid) {
        set_function(name, param, eq_sign + 1);
        out_line("Defined function %s(%s)", name, param);
      } else {
        out_line("Error: Invalid function definition.");
      }
    } else {
      CalcResult res = evaluate_expression(token);
      if (res.error == 1) out_line("Error: Division by zero.");
      else if (res.error == 2) out_line("Error: Invalid Operator.");
      else if (res.error == 3) out_line("Error: Invalid expression.");
      else if (res.error == 4) out_line("Error: Unknown variable.");
      else if (res.error == 5) out_line("Error: Invalid assignment (Var must be letters).");
      else if (res.error == 6) out_line("Error: Invalid Argument (e.g. sqrt(-1)).");
      else out_line("= %.6g", res.result);
    }
  }
}

int main(void) {
  app = manatui_init();

  int H = LINES > 0 ? LINES : 24;
  int W = COLS > 0 ? COLS : 80;

  output = textarea_create(stdscr, H - 6, W - 2, 1, 1, "Calc Output", NULL);
  output->disabled = TRUE;
  output->show_line_numbers = FALSE;
  output->base.has_border = TRUE;
  output->content_color = "#d0d0d0";
  textarea_render(app, output);

  input = textinput_create(stdscr, W - 2, H - 4, 1, "calc> ", handle_input_key);
  input->base.user_data = input;
  textinput_render(app, input);

  app_focus_on(app, input);

  out_line("Calc " VERSION " (c) 2025 nadeeshafdo");
  out_line("TUI mode. Enter expression, press Enter. Ctrl-Q to quit.");
  textarea_render(app, output);

  manatui_loop(app);
  return 0;
}