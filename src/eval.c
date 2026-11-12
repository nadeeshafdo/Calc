#include "eval.h"
#include "funcs.h"
#include "stack.h"
#include "vars.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECURSION_DEPTH 64

char solve_output[BUFFER_SIZE * 4];

// --- Helper for Functions ---
// --- Helper for Functions ---
int is_function_name(const char *s) {
  return (strcmp(s, "sin") == 0 || strcmp(s, "cos") == 0 ||
          strcmp(s, "tan") == 0 || strcmp(s, "sqrt") == 0 ||
          strcmp(s, "log") == 0 || strcmp(s, "ln") == 0 ||
          strcmp(s, "log10") == 0 || strcmp(s, "asin") == 0 ||
          strcmp(s, "acos") == 0 || strcmp(s, "atan") == 0 ||
          strcmp(s, "abs") == 0 || strcmp(s, "floor") == 0 ||
          strcmp(s, "ceil") == 0 || strcmp(s, "round") == 0 ||
          strcmp(s, "exp") == 0);
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
    // Percentage: A % B = A * B / 100 (postfix unary handled in solve_math)
    return (a * b) / 100.0;
  default:
    *error = 2;
    return 0; // BadOp
  }
}

// Pre-processor to handle Implicit Multiplication
void normalize_expression(char *out, const char *in) {
  int j = 0;
  int cap = (BUFFER_SIZE * 2) - 1;
  for (int i = 0; in[i]; i++) {
    if (isspace((unsigned char)in[i]))
      continue; // Strip spaces

    if (j < cap) out[j++] = in[i];

    char curr = in[i];
    // Peek past spaces without mutating i
    int k2 = i + 1;
    while (in[k2] && isspace((unsigned char)in[k2]))
      k2++;
    char next = in[k2];

    if (next == 0)
      break;

    // Rule 1: ) followed by ( or number or letter -> Insert *
    if (curr == ')' && (next == '(' || isalnum((unsigned char)next))) {
      if (j < cap) out[j++] = '*';
    }
    // Rule 2: number followed by ( or letter -> Insert *
    else if (isdigit((unsigned char)curr) &&
             (next == '(' || isalpha((unsigned char)next))) {
      int do_insert = 1;
      if (next == '(') {
        int prev = i - 1;
        while (prev >= 0 && isspace((unsigned char)in[prev])) prev--;
        if (prev >= 0 && isalnum((unsigned char)in[prev])) {
          int start = i;
          while (start > 0 && isalnum((unsigned char)in[start - 1])) start--;
          char word[VAR_NAME_LEN];
          int wk = 0;
          for (int m = start; m <= i && wk < VAR_NAME_LEN - 1; m++) {
            if (!isspace((unsigned char)in[m])) word[wk++] = in[m];
          }
          word[wk] = '\0';
          if (is_function_name(word) || is_user_function_defined(word)) {
            do_insert = 0;
          }
        }
      }
      if (do_insert && j < cap) out[j++] = '*';
    }
    // Rule 2b: % (postfix percent) followed by number/letter/(/. -> Insert *
    // e.g. "500%25" -> "500%*25" so (500%) * 25 = 125
    else if (curr == '%' &&
             (next == '(' || isalnum((unsigned char)next) || next == '.')) {
      if (j < cap) out[j++] = '*';
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

      if (!is_function_name(word) && !is_user_function_defined(word)) {
        if (j < cap) out[j++] = '*';
      }
    }
  }
  out[j] = '\0';
}

CalcResult solve_math(const char *raw_expr) {
  // 1. Normalize (handle implicit multiplication)
  char expr[BUFFER_SIZE * 2]; // Double size to accommodate inserted *
  normalize_expression(expr, raw_expr);

  DoubleStack values = {.top = -1, .error = 0};
  CharStack ops = {.top = -1, .error = 0};
  CalcResult res = {0, 0};
  int i = 0;
  int can_be_unary = 1;

  while (expr[i]) {
    if (isdigit(expr[i]) || expr[i] == '.') {
      char *end_ptr;
      double val = strtod(&expr[i], &end_ptr);
      d_push(&values, val);
      i = (int)(end_ptr - expr);
      can_be_unary = 0;
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
        else if (strcmp(var_name, "log") == 0 || strcmp(var_name, "ln") == 0)
          c_push(&ops, 'l');
        else if (strcmp(var_name, "log10") == 0)
          c_push(&ops, 'g');
        else if (strcmp(var_name, "asin") == 0)
          c_push(&ops, 'a');
        else if (strcmp(var_name, "acos") == 0)
          c_push(&ops, 'o');
        else if (strcmp(var_name, "atan") == 0)
          c_push(&ops, 'n');
        else if (strcmp(var_name, "abs") == 0)
          c_push(&ops, 'b');
        else if (strcmp(var_name, "floor") == 0)
          c_push(&ops, 'f');
        else if (strcmp(var_name, "ceil") == 0)
          c_push(&ops, 'u');
        else if (strcmp(var_name, "round") == 0)
          c_push(&ops, 'r');
        else if (strcmp(var_name, "exp") == 0)
          c_push(&ops, 'x');
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
        /* User-function opcodes are signed negatives (-1 .. -MAX_FUNCS).
           Read them as unsigned so this works when plain char is unsigned. */
        unsigned char raw_op = (unsigned char)c_peek(&ops);
        int is_user_op = raw_op >= (unsigned char)(256 - MAX_FUNCS);
        int is_builtin_op = (raw_op < 128 && strchr("sctqlgabfurnxo", (char)raw_op) != NULL);
        if (is_builtin_op || is_user_op) {
          unsigned char func = (unsigned char)c_pop(&ops);
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
          } else if (func == 'g') {
            if (val <= 0) {
              res.error = 6;
              return res;
            }
            result = log10(val);
          } else if (func == 'a') {
            if (val < -1.0 || val > 1.0) {
              res.error = 6;
              return res;
            }
            result = asin(val);
          } else if (func == 'o') {
            if (val < -1.0 || val > 1.0) {
              res.error = 6;
              return res;
            }
            result = acos(val);
          } else if (func == 'n') {
            result = atan(val);
          } else if (func == 'b') {
            result = fabs(val);
          } else if (func == 'f') {
            result = floor(val);
          } else if (func == 'u') {
            result = ceil(val);
          } else if (func == 'r') {
            result = round(val);
          } else if (func == 'x') {
            result = exp(val);
          } else if (func >= 128) {
            // User function opcode stored as signed negative byte
            int idx = -1 - (int)(signed char)func;
            if (idx >= 0 && idx < func_count) {
              char *param = functions[idx].param;
              double old_val;
              int had_var = get_variable(param, &old_val);

              static int func_depth = 0;
              if (func_depth >= MAX_RECURSION_DEPTH) {
                res.error = 3;
                return res;
              }
              func_depth++;
              set_variable(param, val);
              CalcResult user_res = solve_math(functions[idx].expr);
              func_depth--;

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

      while (ops.top != -1 &&
             ((current_op == '^')
                  ? (precedence(c_peek(&ops)) > precedence(current_op))
                  : (precedence(c_peek(&ops)) >= precedence(current_op)))) {
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

  if (values.error || ops.error)
    res.error = 3;
  else if (values.top == -1)
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
      if (!is_function_name(name) && !is_user_function_defined(name)) {
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
    int len = (int)(eq_sign - expr);

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

/* ---------- Equation solvers (v3.0.0) ---------- */
#define SOL_PRINTF(...) snprintf(solve_output + strlen(solve_output), sizeof(solve_output) - strlen(solve_output), __VA_ARGS__)

static double eval_expr_at(const char *e, const char *v, double t, int *err) {
  double old; int had = get_variable((char*)v, &old);
  set_variable((char*)v, t);
  CalcResult r = solve_math(e);
  if (had) set_variable((char*)v, old); else unset_variable((char*)v);
  if (r.error) { *err = r.error; return 0; }
  *err = 0;
  return r.result;
}

static double eval_expr_at2(const char *e, const char *v1, double t1, const char *v2, double t2, int *err) {
  double o1,o2; int h1=get_variable((char*)v1,&o1), h2=get_variable((char*)v2,&o2);
  set_variable((char*)v1,t1); set_variable((char*)v2,t2);
  CalcResult r = solve_math(e);
  if (h1) set_variable((char*)v1,o1); else unset_variable((char*)v1);
  if (h2) set_variable((char*)v2,o2); else unset_variable((char*)v2);
  if (r.error) { *err=r.error; return 0; }
  *err=0; return r.result;
}



static int is_constant_name(const char *n) {
  return (strcmp(n,"pi")==0 || strcmp(n,"e")==0 || strcmp(n,"tau")==0 || strcmp(n,"phi")==0);
}

static int get_all_variables(const char *expr, int max_vars, char var_names[][VAR_NAME_LEN]) {
  char clean[BUFFER_SIZE*2];
  normalize_expression(clean, expr);
  int i=0, count=0;
  while (clean[i]) {
    if (isalpha((unsigned char)clean[i])) {
      char name[VAR_NAME_LEN];
      int k=0;
      while (isalnum((unsigned char)clean[i]) && k < VAR_NAME_LEN-1) name[k++]=clean[i++];
      name[k]=0;
      if (!is_function_name(name) && !is_user_function_defined(name) && !is_constant_name(name)) {
        int found=0;
        for (int j=0;j<count;j++) if (strcmp(var_names[j],name)==0){found=1;break;}
        if (!found) {
          if (count < max_vars) strcpy(var_names[count++], name);
          else return count+1;
        }
      }
    } else i++;
  }
  return count;
}

int solve_equation(const char *expr) {
  solve_output[0]=0;
  const char *eq = strchr(expr, '=');
  if (!eq) return 1;
  char lhs[BUFFER_SIZE], rhs[BUFFER_SIZE];
  int ll = (int)(eq - expr);
  if (ll >= BUFFER_SIZE) ll = BUFFER_SIZE - 1;
  strncpy(lhs, expr, ll); lhs[ll] = 0;
  strncpy(rhs, eq + 1, BUFFER_SIZE - 1); rhs[BUFFER_SIZE - 1] = 0;

  char unknowns[8][VAR_NAME_LEN];
  int nunk = get_unknown_variables(expr, 8, unknowns);
  if (nunk != 1) {
    nunk = get_all_variables(expr, 8, unknowns);
  }
  if (nunk != 1) {
    SOL_PRINTF("Error: expected 1 unknown variable.\n");
    return 1;
  }
  const char *v = unknowns[0];
  int ea = 0, eb = 0;
  double l0 = eval_expr_at(lhs, v, 0, &ea);
  double r0 = eval_expr_at(rhs, v, 0, &eb);
  double l1 = eval_expr_at(lhs, v, 1, &ea);
  double r1s = eval_expr_at(rhs, v, 1, &eb);
  double lm = eval_expr_at(lhs, v, -1, &ea);
  double rm = eval_expr_at(rhs, v, -1, &eb);
  double l2 = eval_expr_at(lhs, v, 2, &ea);
  double r2s = eval_expr_at(rhs, v, 2, &eb);
  if (ea || eb) {
    SOL_PRINTF("Error: Invalid expression.\n");
    return 1;
  }
  double f0 = l0 - r0;
  double f1 = l1 - r1s;
  double fm1 = lm - rm;
  double f2 = l2 - r2s;
  double c = f0;
  double a = (f1 + fm1) / 2.0 - c;
  double b = (f1 - fm1) / 2.0;
  /* verify */
  double f2pred = 4*a + 2*b + c;
  if (fabs(f2 - f2pred) > 1e-6) {
    SOL_PRINTF("Error: not a linear/quadratic equation.\n");
    return 1;
  }
  if (fabs(a) < 1e-12) {
    if (fabs(b) < 1e-12) {
      if (fabs(c) < 1e-12) SOL_PRINTF("Infinite solutions.\n");
      else SOL_PRINTF("No solution.\n");
      return 0;
    }
    double root = -c / b;
    SOL_PRINTF("%s = %.6g\n", v, root);
    return 0;
  }
  double disc = b*b - 4*a*c;
  if (disc < -1e-9) {
    SOL_PRINTF("No real roots.\n");
    return 0;
  }
  if (disc < 1e-9) {
    double root = -b / (2*a);
    SOL_PRINTF("%s = %.6g\n", v, root);
    return 0;
  }
  double root1 = (-b + sqrt(disc)) / (2*a);
  double root2 = (-b - sqrt(disc)) / (2*a);
  SOL_PRINTF("%s = %.6g, %.6g\n", v, root1, root2);
  return 0;
}

int solve_system(const char *line) {
  solve_output[0]=0;
  /* split equations by ; */
  char eq1[BUFFER_SIZE], eq2[BUFFER_SIZE];
  eq1[0]=eq2[0]=0;
  /* find first ; */
  const char *semi = strchr(line, ';');
  if (!semi) return 1;
  int l1 = (int)(semi - line);
  if (l1 >= BUFFER_SIZE) l1 = BUFFER_SIZE-1;
  strncpy(eq1, line, l1); eq1[l1]=0;
  strncpy(eq2, semi+1, BUFFER_SIZE-1); eq2[BUFFER_SIZE-1]=0;

  char unknowns[4][VAR_NAME_LEN];
  int nunk = get_unknown_variables(line, 4, unknowns);
  if (nunk != 2) {
    nunk = get_all_variables(line, 4, unknowns);
  }
  if (nunk != 2) {
    SOL_PRINTF("Error: expected 2 unknowns.\n");
    return 1;
  }
  const char *u = unknowns[0];
  const char *vv = unknowns[1];

  /* parse each equation into lhs/rhs */
  char l1e[BUFFER_SIZE], r1e[BUFFER_SIZE], l2e[BUFFER_SIZE], r2e[BUFFER_SIZE];
  /* eq1 */
  char *e = strchr(eq1, '=');
  if (!e) return 1;
  int a = (int)(e - eq1); if (a>=BUFFER_SIZE) a=BUFFER_SIZE-1;
  strncpy(l1e, eq1, a); l1e[a]=0;
  strncpy(r1e, e+1, BUFFER_SIZE-1); r1e[BUFFER_SIZE-1]=0;
  e = strchr(eq2, '=');
  if (!e) return 1;
  a = (int)(e - eq2); if (a>=BUFFER_SIZE) a=BUFFER_SIZE-1;
  strncpy(l2e, eq2, a); l2e[a]=0;
  strncpy(r2e, e+1, BUFFER_SIZE-1); r2e[BUFFER_SIZE-1]=0;

  /* evaluate coefficients: f = lhs - rhs = A*u + B*v + C */
  int err = 0;
  double C1 = eval_expr_at2(l1e,u,0,vv,0,&err) - eval_expr_at2(r1e,u,0,vv,0,&err);
  if (err) {
    SOL_PRINTF("Error: Invalid expression.\n");
    return 1;
  }
  double A1 = eval_expr_at2(l1e,u,1,vv,0,&err) - eval_expr_at2(r1e,u,1,vv,0,&err) - C1;
  double B1 = eval_expr_at2(l1e,u,0,vv,1,&err) - eval_expr_at2(r1e,u,0,vv,1,&err) - C1;
  double C2 = eval_expr_at2(l2e,u,0,vv,0,&err) - eval_expr_at2(r2e,u,0,vv,0,&err);
  double A2 = eval_expr_at2(l2e,u,1,vv,0,&err) - eval_expr_at2(r2e,u,1,vv,0,&err) - C2;
  double B2 = eval_expr_at2(l2e,u,0,vv,1,&err) - eval_expr_at2(r2e,u,0,vv,1,&err) - C2;
  if (err) {
    SOL_PRINTF("Error: Invalid expression.\n");
    return 1;
  }

  double det = A1*B2 - A2*B1;
  if (fabs(det) < 1e-12) {
    SOL_PRINTF("No unique solution.\n");
    return 0;
  }
  double uval = (-(C1)*B2 - B1*(-(C2))) / det; /* solve A1 u + B1 v = -C1 */
  /* Actually using Cramer's: u = (-C1*B2 - B1*(-C2))/det */
  double vval = (A1*(-(C2)) - (-(C1))*A2) / det;
  SOL_PRINTF("%s = %.6g, %s = %.6g\n", u, uval, vv, vval);
  return 0;
}

static int span_is_identifier(const char *s, const char *end) {
  while (s < end && isspace((unsigned char)*s)) s++;
  if (s >= end || !isalpha((unsigned char)*s)) return 0;
  s++;
  while (s < end && isalnum((unsigned char)*s)) s++;
  while (s < end && isspace((unsigned char)*s)) s++;
  return s == end;
}

int line_is_linear_system(const char *line) {
  const char *semi = strchr(line, ';');
  if (!semi || strchr(semi + 1, ';')) return 0;
  const char *eq1 = memchr(line, '=', (size_t)(semi - line));
  const char *eq2 = strchr(semi + 1, '=');
  if (!eq1 || !eq2) return 0;
  /* Plain assignments (x=1; y=2) stay separate statements. */
  if (span_is_identifier(line, eq1) || span_is_identifier(semi + 1, eq2)) return 0;
  return 1;
}

static int parse_function_def(const char *token, char *name, char *param, const char **expr_out) {
  const char *po = strchr(token, '(');
  const char *pc = strchr(token, ')');
  const char *eq = strchr(token, '=');
  if (!po || !pc || !eq || !(po < pc && pc < eq)) return 0;
  int n = 0;
  const char *p = token;
  while (p < po) {
    if (!isspace((unsigned char)*p)) {
      if (!isalnum((unsigned char)*p) || n >= VAR_NAME_LEN - 1) return 0;
      name[n++] = *p;
    }
    p++;
  }
  name[n] = '\0';
  if (n == 0 || isdigit((unsigned char)name[0]) || is_function_name(name)) return 0;
  int m = 0;
  p = po + 1;
  while (p < pc) {
    if (!isspace((unsigned char)*p)) {
      if (!isalnum((unsigned char)*p) || m >= VAR_NAME_LEN - 1) return 0;
      param[m++] = *p;
    }
    p++;
  }
  param[m] = '\0';
  if (m == 0 || isdigit((unsigned char)param[0])) return 0;
  *expr_out = eq + 1;
  return 1;
}

static void format_result(CalcResult res) {
  if (res.error == 1) SOL_PRINTF("Error: Division by zero.\n");
  else if (res.error == 2) SOL_PRINTF("Error: Invalid Operator.\n");
  else if (res.error == 3) SOL_PRINTF("Error: Invalid expression.\n");
  else if (res.error == 4) SOL_PRINTF("Error: Unknown variable.\n");
  else if (res.error == 5) SOL_PRINTF("Error: Invalid assignment.\n");
  else if (res.error == 6) SOL_PRINTF("Error: Invalid Argument.\n");
  else if (!isfinite(res.result)) SOL_PRINTF("Error: Invalid Argument.\n");
  else SOL_PRINTF("= %.6g\n", res.result);
}

void eval_dispatch(const char *token) {
  solve_output[0] = '\0';
  char name[VAR_NAME_LEN], param[VAR_NAME_LEN];
  const char *expr = NULL;
  if (parse_function_def(token, name, param, &expr)) {
    set_function(name, param, (char *)expr);
    SOL_PRINTF("Defined function %s(%s)\n", name, param);
    return;
  }
  const char *eq = strchr(token, '=');
  if (eq && span_is_identifier(token, eq)) {
    format_result(evaluate_expression(token));
    return;
  }
  if (eq) {
    solve_equation(token);
    return;
  }
  format_result(evaluate_expression(token));
}
