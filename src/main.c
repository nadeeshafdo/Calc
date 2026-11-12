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
      if (line_is_linear_system(input)) {
        solve_system(input);
        fputs(solve_output, stdout);
        continue;
      }
      char work[BUFFER_SIZE];
      strncpy(work, input, BUFFER_SIZE - 1);
      work[BUFFER_SIZE - 1] = '\0';
      char *tok = strtok(work, ";");
      while (tok != NULL) {
        while (isspace((unsigned char)*tok)) tok++;
        if (*tok == '\0') {
          tok = strtok(NULL, ";");
          continue;
        }
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
        if (strlen(tok) > 0) process_token_noninteractive(tok);
        tok = strtok(NULL, ";");
      }
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
    printf("Plot not available in non-interactive mode.\n");
  } else {
    eval_dispatch(token);
    fputs(solve_output, stdout);
  }
}
