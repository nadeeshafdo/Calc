#include "defs.h"
#include "eval.h"
#include "funcs.h"
#include "ui.h"
#include "vars.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define IS_TTY() _isatty(_fileno(stdin))
#else
#include <unistd.h>
#define IS_TTY() isatty(STDIN_FILENO)
#endif

static void process_token_noninteractive(char *token);

int main(void) {
  if (!IS_TTY()) {
    char input[BUFFER_SIZE];
    while (fgets(input, sizeof(input), stdin)) {
      /* strip trailing newline */
      input[strcspn(input, "\r\n")] = '\0';
      if (strlen(input) == 0) continue;
      if (strchr(input, '=') && strchr(input, ';')) {
        /* treat whole line as a simultaneous system */
        process_token_noninteractive(input);
      } else {
      char work[BUFFER_SIZE];
      strncpy(work, input, BUFFER_SIZE - 1);
      work[BUFFER_SIZE - 1] = '\0';
      char *tok = strtok(work, ";");
      while (tok != NULL) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
        if (strlen(tok) > 0) process_token_noninteractive(tok);
        tok = strtok(NULL, ";");
      }
      } /* close else */
    } /* close while fgets */
    return 0;
  }
  run_tui();
  return 0;
}

static void process_token_noninteractive(char *token) {
  if (strcmp(token, "vars") == 0) {
    list_variables();
  } else if (strcmp(token, "funcs") == 0) {
    list_functions();
  } else if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
    exit(0);
  } else if (strncmp(token, "unset ", 6) == 0) {
    char var_name[VAR_NAME_LEN];
    int i = 6;
    while (token[i] && isspace((unsigned char)token[i])) i++;
    int k = 0;
    while (token[i] && !isspace((unsigned char)token[i]) && k < VAR_NAME_LEN - 1) var_name[k++] = token[i++];
    var_name[k] = '\0';
    if (k > 0) {
      if (is_variable_defined(var_name)) {
        unset_variable(var_name);
        printf("Variable '%s' unset.\n", var_name);
      } else if (is_user_function_defined(var_name)) {
        unset_function(var_name);
        printf("Function '%s' unset.\n", var_name);
      } else {
        printf("Error: Variable/Function '%s' not found.\n", var_name);
      }
    }
  } else if (strncmp(token, "plot(", 5) == 0) {
    /* ignore plotting in noninteractive */
    printf("Plot not available in non-interactive mode.\n");
  } else {
    char *po = strchr(token, '(');
    char *pc = strchr(token, ')');
    char *eq = strchr(token, '=');
    if (po && pc && eq && po < pc && pc < eq) {
      char name[VAR_NAME_LEN], param[VAR_NAME_LEN];
      int valid = 1, n = 0, m = 0;
      char *p = token;
      while (p < po) {
        if (!isspace((unsigned char)*p)) {
          if (!isalnum((unsigned char)*p)) valid = 0;
          if (n < VAR_NAME_LEN - 1) name[n++] = *p; else valid = 0;
        }
        p++;
      }
      name[n] = 0;
      if (n == 0) valid = 0;
      if (n > 0 && isdigit((unsigned char)name[0])) valid = 0;
      if (valid && is_function_name(name)) valid = 0;
      p = po + 1;
      while (p < pc) {
        if (!isspace((unsigned char)*p)) {
          if (!isalnum((unsigned char)*p)) valid = 0;
          if (m < VAR_NAME_LEN - 1) param[m++] = *p; else valid = 0;
        }
        p++;
      }
      param[m] = 0;
      if (m == 0) valid = 0;
      if (m > 0 && isdigit((unsigned char)param[0])) valid = 0;
      if (valid) {
        set_function(name, param, eq + 1);
        printf("Defined function %s(%s)\n", name, param);
      } else {
        printf("Error: Invalid function definition.\n");
      }
    } else if (eq) {
      int len = eq - token;
      char lhs[VAR_NAME_LEN];
      int k = 0, j = 0, valid = 1;
      while (j < len && isspace((unsigned char)token[j])) j++;
      if (j < len && isalpha((unsigned char)token[j])) {
        while (j < len && isalnum((unsigned char)token[j]) && k < VAR_NAME_LEN - 1) lhs[k++] = token[j++];
        lhs[k] = 0;
        while (j < len && isspace((unsigned char)token[j])) j++;
        if (j != len) valid = 0;
      } else valid = 0;
      if (valid) {
        CalcResult rhs = evaluate_expression(eq + 1);
        if (rhs.error == 0) set_variable(lhs, rhs.result);
        if (rhs.error == 1) printf("Error: Division by zero.\n");
        else if (rhs.error == 2) printf("Error: Invalid Operator.\n");
        else if (rhs.error == 3) printf("Error: Invalid expression.\n");
        else if (rhs.error == 4) printf("Error: Unknown variable.\n");
        else if (rhs.error == 5) printf("Error: Invalid assignment.\n");
        else if (rhs.error == 6) printf("Error: Invalid Argument.\n");
        else printf("= %.6g\n", rhs.result);
      } else {
        if (strchr(token, ';'))
          solve_system(token);
        else
          solve_equation(token);
      }
    } else {
      CalcResult res = evaluate_expression(token);
      if (res.error == 1) printf("Error: Division by zero.\n");
      else if (res.error == 2) printf("Error: Invalid Operator.\n");
      else if (res.error == 3) printf("Error: Invalid expression.\n");
      else if (res.error == 4) printf("Error: Unknown variable.\n");
      else if (res.error == 5) printf("Error: Invalid assignment.\n");
      else if (res.error == 6) printf("Error: Invalid Argument.\n");
      else printf("= %.6g\n", res.result);
    }
  }
}
