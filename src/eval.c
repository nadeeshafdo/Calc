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
  if (op == '=')
    return 1; // Equality test (lowest, e.g. 1 = 1 -> 1)
  if (op == '+' || op == '-')
    return 2;
  if (op == '*' || op == '/' || op == '%')
    return 3;
  if (op == '^')
    return 4;
  return 0;
}

double apply_op(double a, double b, char op, int *error) {
  switch (op) {
  case '=':
    return (fabs(a - b) < 1e-9) ? 1.0 : 0.0;
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
    // Percentage: A % B = A * B / 100 (postfix unary handled in solve_math)
    return (a * b) / 100.0;
  default:
    *error = 2;
    return 0; // BadOp
  }
}

// Pre-processor to handle Implicit Multiplication
// Returns 0 on success, 1 if output was truncated (fail closed).
int normalize_expression(char *out, const char *in, size_t out_size) {
  if (out_size == 0)
    return 1;
  int j = 0;
  for (int i = 0; in[i]; i++) {
    if (isspace((unsigned char)in[i]))
      continue; // Strip spaces

    if (j + 1 >= (int)out_size) {
      out[out_size - 1] = '\0';
      return 1;
    }
    out[j++] = in[i];

    char curr = in[i];
    // Peek past spaces without mutating i
    int k2 = i + 1;
    while (in[k2] && isspace((unsigned char)in[k2]))
      k2++;
    char next = in[k2];

    if (next == 0)
      break;

    int need_star = 0;
    // Rule 1: ) followed by ( or number or letter/_ -> Insert *
    if (curr == ')' && (next == '(' || IS_VAR_CHAR(next))) {
      need_star = 1;
    }
    // Rule 2: number followed by ( or letter/_ -> Insert *
    // Exempt when the digit is part of an identifier (e.g. f1(x) or a2x):
    // if previous char is a var char, this digit continues a name.
    else if (isdigit((unsigned char)curr) &&
             (next == '(' || IS_VAR_START(next))) {
      if (!(i > 0 && IS_VAR_CHAR(in[i - 1]))) {
        need_star = 1;
      }
    }
    // Rule 2b: % (postfix percent) followed by number/letter/_/(/. -> Insert *
    // e.g. "500%25" -> "500%*25" so (500%) * 25 = 125
    else if (curr == '%' &&
             (next == '(' || IS_VAR_CHAR(next) || next == '.')) {
      need_star = 1;
    }
    // Rule 3: letter/_ followed by ( -> Insert * UNLESS it's a function
    else if (IS_VAR_CHAR(curr) && next == '(') {
      // Check if the current word is a function name
      int k = 0;
      int too_long = 0;
      char word[VAR_NAME_LEN];
      // Backtrack to find the start of the word
      int start = i;
      while (start > 0 && IS_VAR_CHAR(in[start - 1]))
        start--;

      // Copy the word
      for (int m = start; m <= i; m++) { // Include current char
        if (k < VAR_NAME_LEN - 1)
          word[k++] = in[m];
        else
          too_long = 1;
      }
      word[k] = '\0';

      if (too_long) {
        // Definitely not a known function (names are bounded); treat as var.
        need_star = 1;
      } else if (!is_function_name(word) && !is_user_function_defined(word)) {
        need_star = 1;
      }
    }
    if (need_star) {
      if (j + 1 >= (int)out_size) {
        out[out_size - 1] = '\0';
        return 1;
      }
      out[j++] = '*';
    }
  }
  out[j] = '\0';
  return 0;
}

CalcResult solve_math(const char *raw_expr) {
  // 1. Normalize (handle implicit multiplication)
  char expr[BUFFER_SIZE * 2]; // Double size to accommodate inserted *
  CalcResult res = {0, 0};
  if (normalize_expression(expr, raw_expr, sizeof(expr))) {
    res.error = 3;
    return res;
  }

  DoubleStack values = {.top = -1};
  CharStack ops = {.top = -1};
  int i = 0;
  int can_be_unary = 1;

  while (expr[i]) {
    if (isdigit((unsigned char)expr[i]) || expr[i] == '.') {
      char *end_ptr;
      double val = strtod(&expr[i], &end_ptr);
      d_push(&values, val);
      i = end_ptr - expr;
      can_be_unary = 0;
    } else if (IS_VAR_START(expr[i])) {
      char var_name[VAR_NAME_LEN];
      int k = 0;
      while (IS_VAR_CHAR(expr[i]) && k < VAR_NAME_LEN - 1) {
        var_name[k++] = expr[i++];
      }
      // Fail closed on over-long identifiers instead of silent split/alias.
      if (IS_VAR_CHAR(expr[i])) {
        res.error = 3;
        return res;
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
        can_be_unary = 1; // Function expects (
      } else if (is_user_function_defined(var_name)) {
        int idx = get_user_func_index(var_name);
        if (idx >= 0) {
          char op_code = (char)(-1 - idx);
          c_push(&ops, op_code);
        }
        can_be_unary = 1; // User function expects (
      } else {
        double val;
        if (get_variable(var_name, &val)) {
          d_push(&values, val);
        } else {
          res.error = 4; // Unknown Var
          return res;
        }
        can_be_unary = 0;
      }
    } else if (expr[i] == '(') {
      c_push(&ops, '(');
      i++;
      can_be_unary = 1;
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
              char *param = functions[idx].param;
              double old_val;
              int had_var = get_variable(param, &old_val);
              set_variable(param, val);
              CalcResult user_res = solve_math(functions[idx].expr);
              // Always restore shadowed param, even on error (no leak).
              if (had_var)
                set_variable(param, old_val);
              else
                unset_variable(param);
              if (user_res.error) {
                res.error = user_res.error;
                return res;
              }
              result = user_res.result;
            }
          }
          d_push(&values, result);
        }
      }
      i++;
      can_be_unary = 0;
    } else if (expr[i] == '%') {
      // Postfix percent: N% = N/100. Binary "A%B" is handled via
      // normalize inserting '*' -> "(A%) * B" = A*B/100.
      if (values.top == -1 || can_be_unary) {
        res.error = 3;
        return res;
      }
      double val = d_pop(&values);
      d_push(&values, val / 100.0);
      i++;
      can_be_unary = 0;
    } else {
      char current_op = expr[i];

      if (can_be_unary) {
        if (current_op == '-') {
          // Push 0 to treat as 0 - x
          d_push(&values, 0);
        } else if (current_op == '+') {
          // Ignore unary plus
          i++;
          continue;
        }
      }

      while (ops.top != -1 && c_peek(&ops) != '(' &&
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
      can_be_unary = 1;
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
  if (normalize_expression(clean_expr, expr, sizeof(clean_expr))) {
    return 0; // truncated input: fail closed, report none
  }

  int i = 0;
  int count = 0;

  while (clean_expr[i]) {
    if (IS_VAR_START(clean_expr[i])) {
      char name[VAR_NAME_LEN];
      int k = 0;
      int too_long = 0;
      while (IS_VAR_CHAR(clean_expr[i])) {
        if (k < VAR_NAME_LEN - 1)
          name[k++] = clean_expr[i];
        else
          too_long = 1;
        i++;
      }
      name[k] = '\0';
      if (too_long) {
        // Skip over-long identifiers instead of splitting/aliasing.
        continue;
      }

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
  // Find first '=' at paren depth 0 (ignore '=' inside parens).
  const char *eq_sign = NULL;
  int depth = 0;
  for (const char *p = expr; *p; p++) {
    if (*p == '(')
      depth++;
    else if (*p == ')') {
      if (depth > 0)
        depth--;
    } else if (*p == '=' && depth == 0) {
      eq_sign = p;
      break;
    }
  }

  if (eq_sign) {
    // Potential ASSIGNMENT (x = ...) or top-level equality (1 = 1).
    int len = eq_sign - expr;

    // Check LHS for validity as a simple variable assignment
    char lhs_clean[BUFFER_SIZE];
    int k = 0, j = 0;
    int valid_assignment = 1;
    int lhs_empty = 1;
    while (j < len && isspace((unsigned char)expr[j]))
      j++;
    if (j < len) {
      lhs_empty = 0;
      if (IS_VAR_START(expr[j])) {
        while (j < len && IS_VAR_CHAR(expr[j]) && k < VAR_NAME_LEN - 1)
          lhs_clean[k++] = expr[j++];
        lhs_clean[k] = '\0';
        while (j < len && isspace((unsigned char)expr[j]))
          j++;
        if (j != len)
          valid_assignment = 0;
      } else {
        valid_assignment = 0;
      }
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
    } else if (lhs_empty) {
      // Degenerate "= ..." with no LHS: keep historical error 5.
      CalcResult err = {0, 5};
      return err;
    }
    // Else: not a valid assignment target -> fall through to solve_math,
    // which treats '=' as an equality test (1 = 1 -> 1, 1 = 2 -> 0).
  }
  // CALCULATION MODE
  return solve_math(expr);
}
