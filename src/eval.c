#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "eval.h"
#include "stack.h"
#include "vars.h"

// --- Helper for Functions ---
int is_function_name(const char *s) {
    return (strcmp(s, "sin") == 0 || strcmp(s, "cos") == 0 ||
            strcmp(s, "tan") == 0 || strcmp(s, "sqrt") == 0 ||
            strcmp(s, "log") == 0);
}

// --- Logic ---
int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/' || op == '%') return 2;
    if (op == '^') return 3;
    return 0;
}

double apply_op(double a, double b, char op, int *error) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': if (b == 0) { *error = 1; return 0; } return a / b;
        case '^': return pow(a, b);
        case '%': if (b == 0) { *error = 1; return 0; } return fmod(a, b);
        default: *error = 2; return 0; // BadOp
    }
}

// Pre-processor to handle Implicit Multiplication
void normalize_expression(char *out, const char *in) {
    int j = 0;
    for (int i = 0; in[i]; i++) {
        if (isspace(in[i])) continue; // Strip spaces

        out[j++] = in[i];

        char curr = in[i];
        char next = in[i+1];
        while (next && isspace(next)) next = in[i++ + 2]; // Peek past spaces

        if (next == 0) break;

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
             while (start > 0 && isalpha(in[start-1])) start--;
             
             // Copy the word
             for (int m = start; m <= i; m++) { // Include current char
                 if (k < 31) word[k++] = in[m];
             }
             word[k] = '\0';
             
             if (!is_function_name(word)) {
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

    DoubleStack values = { .top = -1 };
    CharStack ops = { .top = -1 };
    CalcResult res = {0, 0};
    int i = 0;

    while (expr[i]) {
        if (isdigit(expr[i]) || expr[i] == '.') {
            char *end_ptr;
            double val = strtod(&expr[i], &end_ptr);
            d_push(&values, val);
            i = end_ptr - expr;
        }
        else if (isalpha(expr[i])) {
            char var_name[VAR_NAME_LEN];
            int k = 0;
            while (isalnum(expr[i]) && k < VAR_NAME_LEN - 1) {
                var_name[k++] = expr[i++];
            }
            var_name[k] = '\0';

            if (is_function_name(var_name)) {
                // Determine which function it is and push a special token
                if (strcmp(var_name, "sin") == 0) c_push(&ops, 's');
                else if (strcmp(var_name, "cos") == 0) c_push(&ops, 'c');
                else if (strcmp(var_name, "tan") == 0) c_push(&ops, 't');
                else if (strcmp(var_name, "sqrt") == 0) c_push(&ops, 'q'); // q for sqrt
                else if (strcmp(var_name, "log") == 0) c_push(&ops, 'l');
            } else {
                double val;
                if (get_variable(var_name, &val)) {
                    d_push(&values, val);
                } else {
                    res.error = 4; // Unknown Var
                    return res;
                }
            }
        }
        else if (expr[i] == '(') { c_push(&ops, '('); i++; }
        else if (expr[i] == ')') {
            while (ops.top != -1 && c_peek(&ops) != '(') {
                double val2 = d_pop(&values);
                double val1 = d_pop(&values);
                char op = c_pop(&ops);
                d_push(&values, apply_op(val1, val2, op, &res.error));
                if (res.error) return res;
            }
            if (ops.top != -1) c_pop(&ops); // Pop '('
            
            // Check if there is a function waiting
            if (ops.top != -1) {
                char potential_func = c_peek(&ops);
                if (strchr("sctql", potential_func)) {
                    char func = c_pop(&ops);
                    double val = d_pop(&values);
                    double result = 0;
                    
                    if (func == 's') result = sin(val);
                    else if (func == 'c') result = cos(val);
                    else if (func == 't') result = tan(val);
                    else if (func == 'q') {
                        if (val < 0) { res.error = 6; return res; } // Invalid Argument
                        result = sqrt(val);
                    }
                    else if (func == 'l') {
                         if (val <= 0) { res.error = 6; return res; }
                         result = log(val);
                    }
                    d_push(&values, result);
                }
            }
            i++;
        }
        else {
            char current_op = expr[i];
            while (ops.top != -1 && precedence(c_peek(&ops)) >= precedence(current_op)) {
                double val2 = d_pop(&values);
                double val1 = d_pop(&values);
                char op = c_pop(&ops);
                d_push(&values, apply_op(val1, val2, op, &res.error));
                if (res.error) return res;
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
        if (res.error) return res;
    }

    if (values.top == -1) res.error = 3;
    else res.result = d_pop(&values);

    return res;
}


// Helper to find unknown variables in an equation
int get_unknown_variables(const char *expr, char *unknown_out) {
    char clean_expr[BUFFER_SIZE * 2];
    normalize_expression(clean_expr, expr);
    
    int i = 0;
    int unknown_count = 0;
    char found_var[VAR_NAME_LEN] = "";

    while (clean_expr[i]) {
        if (isalpha(clean_expr[i])) {
            char var_name[VAR_NAME_LEN];
            int k = 0;
            while (isalnum(clean_expr[i]) && k < VAR_NAME_LEN - 1) {
                var_name[k++] = clean_expr[i++];
            }
            var_name[k] = '\0';

            // Ignore functions
            if (!is_function_name(var_name)) {
                // Check if defined
                if (!is_variable_defined(var_name)) {
                    // Check if unique
                    if (strcmp(found_var, var_name) != 0) {
                        if (strlen(found_var) == 0) {
                             strcpy(found_var, var_name);
                             unknown_count++;
                        } else {
                             // Different unknown variable found
                             return 2; // Multiple unknowns
                        }
                    }
                }
            }
        } else {
            i++;
        }
    }

    if (unknown_count == 1) {
        strcpy(unknown_out, found_var);
        return 1;
    }
    return unknown_count;
}

CalcResult evaluate_expression(const char *expr) {
    char *eq_sign = strchr(expr, '=');
    
    if (eq_sign) {
        // ASSIGNMENT OR EQUATION MODE
        int len = eq_sign - expr;
        
        // Check LHS for validity as a simple variable assignment
        char lhs_clean[BUFFER_SIZE];
        int k = 0, j = 0;
        int valid_assignment = 1;
        while (j < len && isspace(expr[j])) j++;
        if (j < len && isalpha(expr[j])) {
             while (j < len && isalnum(expr[j]) && k < VAR_NAME_LEN - 1) lhs_clean[k++] = expr[j++];
             lhs_clean[k] = '\0';
             while (j < len && isspace(expr[j])) j++;
             if (j != len) valid_assignment = 0; // Garbage like "x + 2 ="
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
            // Equation Solving Mode: LHS = RHS -> LHS - (RHS) = 0
            // Construct expression: LHS - (RHS)
            char combined[BUFFER_SIZE * 2];
            
            // LHS: expr to eq_sign
            char lhs[BUFFER_SIZE];
            strncpy(lhs, expr, len);
            lhs[len] = '\0';
            
            snprintf(combined, sizeof(combined), "(%s) - (%s)", lhs, eq_sign + 1);
            
            char unknown_var[VAR_NAME_LEN];
            int unknown_res = get_unknown_variables(combined, unknown_var);
            
            if (unknown_res == 1) {
                // Solve for unknown_var
                // We use linear interpolation: f(x) = ax + b
                // f(0) = b
                // f(1) = a + b
                // f(1) - f(0) = a (slope)
                // root x = -b / a = -f(0) / (f(1) - f(0))
                
                // Save old value if it (unlikely) exists, though `is_variable_defined` said no.
                // Just in case, to be safe (or if we relax logic later).
                
                set_variable(unknown_var, 0.0);
                CalcResult f0 = solve_math(combined);
                if (f0.error) return f0;
                
                set_variable(unknown_var, 1.0);
                CalcResult f1 = solve_math(combined);
                if (f1.error) return f1;
                
                double slope = f1.result - f0.result;
                if (fabs(slope) < 1e-9) {
                     CalcResult err = {0, 6}; return err; // No solution or infinite solutions (slope 0)
                }
                
                double root = -f0.result / slope;
                
                // Set the result
                set_variable(unknown_var, root);
                
                printf(COLOR_GREEN "Solved %s = %.6g\n" COLOR_RESET, unknown_var, root);
                
                CalcResult res = {root, 0};
                return res;
                
            } else if (unknown_res == 0) {
                // All vars defined. Check if true/false?
                CalcResult val = solve_math(combined);
                if (val.error) return val;
                if (fabs(val.result) < 1e-9) {
                    printf(COLOR_GREEN "True\n" COLOR_RESET);
                    return val;
                } else {
                    printf(COLOR_RED "False (Diff: %.6g)\n" COLOR_RESET, val.result);
                    return val;
                }
            } else {
                CalcResult err = {0, 6}; return err; // Too many unknowns
            }
        }
    } else {
        // CALCULATION MODE
        return solve_math(expr);
    }
}
