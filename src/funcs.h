#ifndef FUNCS_H
#define FUNCS_H

#include "defs.h"

// --- Global Symbol Table for Functions ---
extern Function functions[MAX_FUNCS];
extern int func_count;

// --- Functions ---
void set_function(char *name, char *param, char *expr);
int get_function_expr(char *name, char *param_out, char *expr_out);
int is_user_function_defined(const char *name);
void unset_function(const char *name);
void list_functions(void);

#endif
