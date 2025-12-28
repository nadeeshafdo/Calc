#ifndef VARS_H
#define VARS_H

#include "defs.h"

// --- Global Symbol Table ---
extern Variable variables[MAX_VARS];
extern int var_count;

// --- Functions ---
void set_variable(char *name, double val);
int get_variable(char *name, double *val);
int is_variable_defined(const char *name);
void unset_variable(const char *name);
void list_variables(void);

#endif
