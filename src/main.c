#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "eval.h"
#include "ui.h"
#include "vars.h"

int main() {
    char input[BUFFER_SIZE];

    enableRawMode();
    print_banner();

    while (1) {
        get_input_raw(input);

        if (strlen(input) == 0) continue;
        
        if (strncmp(input, "unset ", 6) == 0) {
            char var_name[BUFFER_SIZE];
            // Skip "unset " (6 chars) and spaces
            int i = 6;
            while (input[i] && isspace(input[i])) i++;
            
            // Extract var name
            int k = 0;
            while (input[i] && !isspace(input[i]) && k < BUFFER_SIZE - 1) {
                var_name[k++] = input[i++];
            }
            var_name[k] = '\0';
            
            if (k > 0) {
                if (is_variable_defined(var_name)) {
                    unset_variable(var_name);
                    printf(COLOR_GREEN "Variable '%s' unset.\n" COLOR_RESET, var_name);
                } else {
                    printf(COLOR_RED "Error: Variable '%s' not found.\n" COLOR_RESET, var_name);
                }
            } else {
                 printf(COLOR_RED "Usage: unset <variable>\n" COLOR_RESET);
            }
            continue;
        }

        add_to_history(input);

        CalcResult res = evaluate_expression(input);

        if (res.error == 1) printf(COLOR_RED "Error: Division by zero.\n" COLOR_RESET);
        else if (res.error == 2) printf(COLOR_RED "Error: Invalid Operator.\n" COLOR_RESET);
        else if (res.error == 3) printf(COLOR_RED "Error: Invalid expression.\n" COLOR_RESET);
        else if (res.error == 4) printf(COLOR_RED "Error: Unknown variable.\n" COLOR_RESET);
        else if (res.error == 5) printf(COLOR_RED "Error: Invalid assignment (Var must be letters).\n" COLOR_RESET);
        else if (res.error == 6) printf(COLOR_RED "Error: Invalid Argument (e.g. sqrt(-1)).\n" COLOR_RESET);
        else {
            printf(COLOR_GREEN "= %.6g\n" COLOR_RESET, res.result);
        }

    }

    return 0;
}
