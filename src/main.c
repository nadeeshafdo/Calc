#include "defs.h"
#include "eval.h"
#include "funcs.h"
#include "ui.h"
#include "vars.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int main() {
  char input[BUFFER_SIZE];

  enableRawMode();
  print_banner();

  while (1) {
    get_input_raw(input);

    // Tokenize by ';'
    char *token = strtok(input, ";");
    while (token != NULL) {
      // Trim whitespace
      while (isspace(*token))
        token++;
      char *end = token + strlen(token) - 1;
      while (end > token && isspace(*end))
        end--;
      *(end + 1) = '\0';

      if (strlen(token) == 0) {
        token = strtok(NULL, ";");
        continue;
      }

      add_to_history(token);

      if (strcmp(token, "vars") == 0) {
        list_variables();
      } else if (strcmp(token, "funcs") == 0) {
        list_functions();
      } else if (strncmp(token, "unset ", 6) == 0) {
        // ... same logic but on 'token' ...
        char var_name[BUFFER_SIZE];
        int i = 6;
        while (token[i] && isspace(token[i]))
          i++;
        int k = 0;
        while (token[i] && !isspace(token[i]) && k < BUFFER_SIZE - 1) {
          var_name[k++] = token[i++];
        }
        var_name[k] = '\0';
        if (k > 0) {
          if (is_variable_defined(var_name)) {
            unset_variable(var_name);
            printf(COLOR_GREEN "Variable '%s' unset.\n" COLOR_RESET, var_name);
          } else if (is_user_function_defined(var_name)) {
            unset_function(var_name);
            printf(COLOR_GREEN "Function '%s' unset.\n" COLOR_RESET, var_name);
          } else {
            printf(COLOR_RED
                   "Error: Variable/Function '%s' not found.\n" COLOR_RESET,
                   var_name);
          }
        }
      } else if (strncmp(token, "plot(", 5) == 0) {
        // Extact content inside plot(...)
        char content[BUFFER_SIZE];
        int i = 5;
        int k = 0;
        while (token[i] && token[i] != ')') {
          content[k++] = token[i++];
        }
        content[k] = '\0';
        plot_function(content);
      } else {
        // Check for Function Definition: f(x) = expr
        // Scan for ( and ) and =
        char *paren_open = strchr(token, '(');
        char *paren_close = strchr(token, ')');
        char *eq_sign = strchr(token, '=');

        if (paren_open && paren_close && eq_sign && paren_open < paren_close &&
            paren_close < eq_sign) {
          // Potential function def
          // Verify LHS is name(param)
          char name[VAR_NAME_LEN];
          char param[VAR_NAME_LEN];

          int valid = 1;
          // Name
          int n = 0;
          char *p = token;
          while (p < paren_open) {
            if (!isspace(*p))
              name[n++] = *p;
            p++;
          }
          name[n] = '\0';
          if (n == 0)
            valid = 0;

          // Param
          int m = 0;
          p = paren_open + 1;
          while (p < paren_close) {
            if (!isspace(*p))
              param[m++] = *p;
            p++;
          }
          param[m] = '\0';
          if (m == 0)
            valid = 0;

          if (valid) {
            set_function(name, param, eq_sign + 1);
            printf(COLOR_GREEN "Defined function %s(%s)\n" COLOR_RESET, name,
                   param);
          } else {
            printf(COLOR_RED
                   "Error: Invalid function definition.\n" COLOR_RESET);
          }

        } else {
          // Normal Evaluation
          CalcResult res = evaluate_expression(token);
          if (res.error == 1)
            printf(COLOR_RED "Error: Division by zero.\n" COLOR_RESET);
          else if (res.error == 2)
            printf(COLOR_RED "Error: Invalid Operator.\n" COLOR_RESET);
          else if (res.error == 3)
            printf(COLOR_RED "Error: Invalid expression.\n" COLOR_RESET);
          else if (res.error == 4)
            printf(COLOR_RED "Error: Unknown variable.\n" COLOR_RESET);
          else if (res.error == 5)
            printf(COLOR_RED "Error: Invalid assignment (Var must be "
                             "letters).\n" COLOR_RESET);
          else if (res.error == 6)
            printf(COLOR_RED
                   "Error: Invalid Argument (e.g. sqrt(-1)).\n" COLOR_RESET);
          else {
            printf(COLOR_GREEN "= %.6g\n" COLOR_RESET, res.result);
          }
        }
      }

      token = strtok(NULL, ";");
    }
  }

  return 0;
}
