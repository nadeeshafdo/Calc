#ifndef EVAL_H
#define EVAL_H

#include "defs.h"

int is_function_name(const char *s);
CalcResult evaluate_expression(const char *expression);
CalcResult solve_math(const char *raw_expr);
int get_equation_variables(const char *expr, int max_vars, char var_names[][VAR_NAME_LEN]);
int solve_equation(const char *expr);
int solve_system(const char *line);
int line_is_linear_system(const char *line);
void eval_dispatch(const char *token);
extern char solve_output[];

#endif
