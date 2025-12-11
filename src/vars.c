#include <string.h>
#include <stdio.h>
#include "vars.h"

// --- Global Symbol Table ---
Variable variables[MAX_VARS];
int var_count = 0;

void set_variable(char *name, double val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            variables[i].value = val;
            return;
        }
    }
    if (var_count < MAX_VARS) {
        strncpy(variables[var_count].name, name, VAR_NAME_LEN - 1);
        variables[var_count].name[VAR_NAME_LEN - 1] = '\0';
        variables[var_count].value = val;
        var_count++;
    }
}

int get_variable(char *name, double *val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            *val = variables[i].value;
            return 1;
        }
    }
    return 0;
}

int is_variable_defined(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void unset_variable(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            // Shift remaining variables left
            for (int j = i; j < var_count - 1; j++) {
                variables[j] = variables[j + 1];
            }
            var_count--;
            return;
        }
    }
}
