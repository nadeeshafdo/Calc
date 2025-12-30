#include "funcs.h"
#include <stdio.h>
#include <string.h>

// --- Global Symbol Table ---
Function functions[MAX_FUNCS];
int func_count = 0;

void set_function(char *name, char *param, char *expr) {
  for (int i = 0; i < func_count; i++) {
    if (strcmp(functions[i].name, name) == 0) {
      strncpy(functions[i].param, param, VAR_NAME_LEN - 1);
      functions[i].param[VAR_NAME_LEN - 1] = '\0';

      strncpy(functions[i].expr, expr, BUFFER_SIZE - 1);
      functions[i].expr[BUFFER_SIZE - 1] = '\0';
      return;
    }
  }
  if (func_count < MAX_FUNCS) {
    strncpy(functions[func_count].name, name, VAR_NAME_LEN - 1);
    functions[func_count].name[VAR_NAME_LEN - 1] = '\0';

    strncpy(functions[func_count].param, param, VAR_NAME_LEN - 1);
    functions[func_count].param[VAR_NAME_LEN - 1] = '\0';

    strncpy(functions[func_count].expr, expr, BUFFER_SIZE - 1);
    functions[func_count].expr[BUFFER_SIZE - 1] = '\0';

    func_count++;
  }
}

int get_function_expr(char *name, char *param_out, char *expr_out) {
  for (int i = 0; i < func_count; i++) {
    if (strcmp(functions[i].name, name) == 0) {
      if (param_out)
        strcpy(param_out, functions[i].param);
      if (expr_out)
        strcpy(expr_out, functions[i].expr);
      return 1;
    }
  }
  return 0;
}

int is_user_function_defined(const char *name) {
  for (int i = 0; i < func_count; i++) {
    if (strcmp(functions[i].name, name) == 0) {
      return 1;
    }
  }
  return 0;
}

void unset_function(const char *name) {
  for (int i = 0; i < func_count; i++) {
    if (strcmp(functions[i].name, name) == 0) {
      // Shift remaining variables left
      for (int j = i; j < func_count - 1; j++) {
        functions[j] = functions[j + 1];
      }
      func_count--;
      return;
    }
  }
}

void list_functions(void) {
  if (func_count == 0) {
    printf(COLOR_RED "No functions defined.\n" COLOR_RESET);
    return;
  }
  printf(COLOR_BOLD "Defined Functions:\n" COLOR_RESET);
  for (int i = 0; i < func_count; i++) {
    printf(COLOR_GREEN "  %s(%s)" COLOR_RESET " = %s\n", functions[i].name,
           functions[i].param, functions[i].expr);
  }
}
