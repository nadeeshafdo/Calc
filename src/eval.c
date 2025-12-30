#include "eval.h"
#include "funcs.h"
#include "stack.h"
#include "vars.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Helper for Functions ---
// --- Helper for Functions ---
int is_function_name(const char *s) {
  return (strcmp(s, "sin") == 0 || strcmp(s, "cos") == 0 ||
          strcmp(s, "tan") == 0 || strcmp(s, "sqrt") == 0 ||
          strcmp(s, "log") == 0);
}

int get_user_func_index(const char *name) {
  for (int i = 0; i < func_count; i++) {
    if (strcmp(functions[i].name, name) == 0)
      return i;
  }
  return -1;
}

// --- Logic ---
int precedence(char op) {
  if (op == '+' || op == '-')
    return 1;
  if (op == '*' || op == '/' || op == '%')
    return 2;
  if (op == '^')
    return 3;
  return 0;
}

double apply_op(double a, double b, char op, int *error) {
  switch (op) {
  case '+':
    return a + b;
  case '-':
    return a - b;
  case '*':
    return a * b;
  case '/':
    if (b == 0) {
      *error = 1;
      return 0;
    }
    return a / b;
  case '^':
    return pow(a, b);
  case '%':
    if (b == 0) {
      *error = 1;
      return 0;
    }
    return fmod(a, b);
  default:
    *error = 2;
    return 0; // BadOp
  }
}

// Pre-processor to handle Implicit Multiplication
void normalize_expression(char *out, const char *in) {
  int j = 0;
  for (int i = 0; in[i]; i++) {
    if (isspace(in[i]))
      continue; // Strip spaces

    out[j++] = in[i];

    char curr = in[i];
    char next = in[i + 1];
    while (next && isspace(next))
      next = in[i++ + 2]; // Peek past spaces

    if (next == 0)
      break;

    // Rule 1: ) followed by ( or number or letter -> Insert *
    if (curr == ')' && (next == '(' || isalnum(next))) {
      out[j++] = '*';
    }
    // Rule 2: number followed by ( or letter -> Insert *
    else if (isdigit(curr) && (next == '(' || isalpha(next))) {
      out[j++] = '*';
    }
    // Rule 3: letter followed by ( -> Insert * UNLESS it's a function
    else if (isalpha(curr) && next == '(') {
      // Check if the current word is a function name
      int k = 0;
      char word[32];
      // Backtrack to find the start of the word
      int start = i;
      while (start > 0 && isalpha(in[start - 1]))
        start--;

      // Copy the word
      for (int m = start; m <= i; m++) { // Include current char
        if (k < 31)
          word[k++] = in[m];
      }
      word[k] = '\0';

      word[k] = '\0';

      if (!is_function_name(word) && !is_user_function_defined(word)) {
        out[j++] = '*';
      }
    }
  }
  out[j] = '\0';
}

CalcResult solve_math(const char *raw_expr) {
  // 1. Normalize (handle implicit multiplication)
  char expr[BUFFER_SIZE * 2]; // Double size to accommodate inserted *
  normalize_expression(expr, raw_expr);

  DoubleStack values = {.top = -1};
  CharStack ops = {.top = -1};
  CalcResult res = {0, 0};
  int i = 0;

  while (expr[i]) {
    if (isdigit(expr[i]) || expr[i] == '.') {
      char *end_ptr;
      double val = strtod(&expr[i], &end_ptr);
      d_push(&values, val);
      i = end_ptr - expr;
    } else if (isalpha(expr[i])) {
      char var_name[VAR_NAME_LEN];
      int k = 0;
      while (isalnum(expr[i]) && k < VAR_NAME_LEN - 1) {
        var_name[k++] = expr[i++];
      }
      var_name[k] = '\0';

      if (is_function_name(var_name)) {
        // Determine which function it is and push a special token
        if (strcmp(var_name, "sin") == 0)
          c_push(&ops, 's');
        else if (strcmp(var_name, "cos") == 0)
          c_push(&ops, 'c');
        else if (strcmp(var_name, "tan") == 0)
          c_push(&ops, 't');
        else if (strcmp(var_name, "sqrt") == 0)
          c_push(&ops, 'q'); // q for sqrt
        else if (strcmp(var_name, "log") == 0)
          c_push(&ops, 'l');
        else if (strcmp(var_name, "sqrt") == 0)
          c_push(&ops, 'q'); // q for sqrt
        else if (strcmp(var_name, "log") == 0)
          c_push(&ops, 'l');
      } else if (is_user_function_defined(var_name)) {
        int idx = get_user_func_index(var_name);
        if (idx >= 0) {
          // Store as negative value: -1 - index
          // index 0 -> -1
          // index 1 -> -2
          char op_code = (char)(-1 - idx);
          c_push(&ops, op_code);
        }
      } else {
        double val;
        if (get_variable(var_name, &val)) {
          d_push(&values, val);
        } else {
          res.error = 4; // Unknown Var
          return res;
        }
      }
    } else if (expr[i] == '(') {
      c_push(&ops, '(');
      i++;
    } else if (expr[i] == ')') {
      while (ops.top != -1 && c_peek(&ops) != '(') {
        double val2 = d_pop(&values);
        double val1 = d_pop(&values);
        char op = c_pop(&ops);
        d_push(&values, apply_op(val1, val2, op, &res.error));
        if (res.error)
          return res;
      }
      if (ops.top != -1)
        c_pop(&ops); // Pop '('

      // Check if there is a function waiting
      if (ops.top != -1) {
        char potential_func = c_peek(&ops);
        if (strchr("sctql", potential_func) || potential_func < 0) {
          char func = c_pop(&ops);
          double val = d_pop(&values);
          double result = 0;

          if (func == 's')
            result = sin(val);
          else if (func == 'c')
            result = cos(val);
          else if (func == 't')
            result = tan(val);
          else if (func == 'q') {
            if (val < 0) {
              res.error = 6;
              return res;
            } // Invalid Argument
            result = sqrt(val);
          } else if (func == 'l') {
            if (val <= 0) {
              res.error = 6;
              return res;
            }
            result = log(val);
          } else if (func < 0) {
            // User function
            int idx = -1 - func;
            if (idx >= 0 && idx < func_count) {
              // Save existing variable if any
              char *param = functions[idx].param;
              double old_val;
              int had_var = get_variable(param, &old_val);

              // Set param
              set_variable(param, val);

              // Eval
              CalcResult user_res = solve_math(functions[idx].expr);
              if (user_res.error) {
                res.error = user_res.error;
                return res;
              }
              result = user_res.result;

              // Restore
              if (had_var)
                set_variable(param, old_val);
              else
                unset_variable(param);
            }
          }
          d_push(&values, result);
        }
      }
      i++;
    } else {
      char current_op = expr[i];
      while (ops.top != -1 &&
             precedence(c_peek(&ops)) >= precedence(current_op)) {
        double val2 = d_pop(&values);
        double val1 = d_pop(&values);
        char op = c_pop(&ops);
        d_push(&values, apply_op(val1, val2, op, &res.error));
        if (res.error)
          return res;
      }
      c_push(&ops, current_op);
      i++;
    }
  }

  while (ops.top != -1) {
    double val2 = d_pop(&values);
    double val1 = d_pop(&values);
    char op = c_pop(&ops);
    d_push(&values, apply_op(val1, val2, op, &res.error));
    if (res.error)
      return res;
  }

  if (values.top == -1)
    res.error = 3;
  else
    res.result = d_pop(&values);

  return res;
}

// Helper to find unknown variables in an equation
int get_unknown_variables(const char *expr, int max_vars,
                          char var_names[][VAR_NAME_LEN]) {
  char clean_expr[BUFFER_SIZE * 2];
  normalize_expression(clean_expr, expr);

  int i = 0;
  int count = 0;

  while (clean_expr[i]) {
    if (isalpha(clean_expr[i])) {
      char name[VAR_NAME_LEN];
      int k = 0;
      while (isalnum(clean_expr[i]) && k < VAR_NAME_LEN - 1) {
        name[k++] = clean_expr[i++];
      }
      name[k] = '\0';

      // Ignore functions
      if (!is_function_name(name)) {
        // Check if defined
        if (!is_variable_defined(name)) {
          // Check if already in list
          int found = 0;
          for (int j = 0; j < count; j++) {
            if (strcmp(var_names[j], name) == 0) {
              found = 1;
              break;
            }
          }
          if (!found) {
            if (count < max_vars) {
              strcpy(var_names[count++], name);
            } else {
              return count + 1; // Indicate overflow
            }
          }
        }
      }
    } else {
      i++;
    }
  }
  return count;
}

CalcResult verify_result(const char *expr) {
  CalcResult res = solve_math(expr);
  return res;
}

CalcResult evaluate_expression(const char *expr) {
  char *eq_sign = strchr(expr, '=');

  if (eq_sign) {
    // ASSIGNMENT
    int len = eq_sign - expr;

    // Check LHS for validity as a simple variable assignment
    char lhs_clean[BUFFER_SIZE];
    int k = 0, j = 0;
    int valid_assignment = 1;
    while (j < len && isspace(expr[j]))
      j++;
    if (j < len && isalpha(expr[j])) {
      while (j < len && isalnum(expr[j]) && k < VAR_NAME_LEN - 1)
        lhs_clean[k++] = expr[j++];
      lhs_clean[k] = '\0';
      while (j < len && isspace(expr[j]))
        j++;
      if (j != len)
        valid_assignment = 0;
    } else {
      valid_assignment = 0;
    }

    if (valid_assignment) {
      // Standard Assignment: x = ...
      CalcResult rhs = evaluate_expression(eq_sign + 1);
      if (rhs.error == 0) {
        set_variable(lhs_clean, rhs.result);
      }
      return rhs;
    } else {
      // Invalid assignment syntax for now
      CalcResult err = {0, 5};
      return err;
    }
  } else {
    // CALCULATION MODE
    return solve_math(expr);
  }
}
