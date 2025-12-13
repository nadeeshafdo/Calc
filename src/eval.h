#ifndef EVAL_H
#define EVAL_H

#include "defs.h"

int is_function_name(const char *s);
CalcResult evaluate_expression(const char *expression);

#endif
