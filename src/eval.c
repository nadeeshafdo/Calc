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
int get_unknown_variables(const char *expr, int max_vars, char var_names[][VAR_NAME_LEN]) {
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
    // Check for Simultaneous Equations (separated by ;)
    char *semi = strchr(expr, ';');
    if (semi) {
        // Two equations
        int len1 = semi - expr;
        char eq1[BUFFER_SIZE], eq2[BUFFER_SIZE];
        strncpy(eq1, expr, len1); eq1[len1] = '\0';
        strcpy(eq2, semi + 1);
        
        // Formulate LHS - RHS for both
        char eq1_comb[BUFFER_SIZE*2], eq2_comb[BUFFER_SIZE*2];
        char *eq1_eq = strchr(eq1, '=');
        char *eq2_eq = strchr(eq2, '=');
        
        if (!eq1_eq || !eq2_eq) {
            CalcResult err = {0, 3}; return err; // Syntax Error
        }
        
        *eq1_eq = '\0';
        snprintf(eq1_comb, sizeof(eq1_comb), "(%s) - (%s)", eq1, eq1_eq + 1);
        *eq1_eq = '='; // Restore just in case
        
        *eq2_eq = '\0';
        snprintf(eq2_comb, sizeof(eq2_comb), "(%s) - (%s)", eq2, eq2_eq + 1);
        *eq2_eq = '=';

        // Find unknowns
        char unknowns[5][VAR_NAME_LEN];
        char combined_all[BUFFER_SIZE*6];
        snprintf(combined_all, sizeof(combined_all), "%s + %s", eq1_comb, eq2_comb);
        
        int count = get_unknown_variables(combined_all, 5, unknowns);
        
        if (count == 2) {
            // Solve Linear System for unknowns[0] (u) and unknowns[1] (v)
            char *u = unknowns[0];
            char *v = unknowns[1];
            
            // Model: A*u + B*v + C = 0
            // C = f(0,0)
            // A = f(1,0) - C
            // B = f(0,1) - C
            
            double a1, b1, c1, a2, b2, c2;
            
            // Eq 1
            set_variable(u, 0); set_variable(v, 0);
            c1 = solve_math(eq1_comb).result;
            set_variable(u, 1); set_variable(v, 0);
            a1 = solve_math(eq1_comb).result - c1;
            set_variable(u, 0); set_variable(v, 1);
            b1 = solve_math(eq1_comb).result - c1;
            
            // Eq 2
            set_variable(u, 0); set_variable(v, 0);
            c2 = solve_math(eq2_comb).result;
            set_variable(u, 1); set_variable(v, 0);
            a2 = solve_math(eq2_comb).result - c2;
            set_variable(u, 0); set_variable(v, 1);
            b2 = solve_math(eq2_comb).result - c2;
            
            // Solve:
            // a1*u + b1*v = -c1
            // a2*u + b2*v = -c2
            
            double det = a1*b2 - a2*b1;
            if (fabs(det) < 1e-9) {
                 unset_variable(u); unset_variable(v);
                 CalcResult err = {0, 6}; return err; // No unique solution
            }
            
            double val_u = ((-c1)*b2 - (-c2)*b1) / det;
            double val_v = (a1*(-c2) - a2*(-c1)) / det;
            
            set_variable(u, val_u);
            set_variable(v, val_v);
            
            printf(COLOR_GREEN "Solved %s = %.6g\n" COLOR_RESET, u, val_u);
            printf(COLOR_GREEN "Solved %s = %.6g\n" COLOR_RESET, v, val_v);
            
            CalcResult res = {val_u, 0}; // Return one of them?
            return res;
        } else {
             CalcResult err = {0, 6}; return err; // Need 2 unknowns
        }
    }


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
            
            char vars[5][VAR_NAME_LEN];
            int count = get_unknown_variables(combined, 5, vars);
            
            if (count == 1) {
                char *unknown_var = vars[0];
                
                // Check Linearity/Quadratic content
                set_variable(unknown_var, 0.0);
                double f0 = solve_math(combined).result;
                
                set_variable(unknown_var, 1.0);
                double f1 = solve_math(combined).result;
                
                set_variable(unknown_var, 2.0);
                double f2 = solve_math(combined).result;
                
                double slope1 = f1 - f0;
                double slope2 = f2 - f1;
                
                if (fabs(slope2 - slope1) < 1e-9) {
                    // Linear: f(x) = ax + b
                    // b = f0, a = slope1
                    double a = slope1;
                    double b = f0;
                    
                    if (fabs(a) < 1e-9) {
                         unset_variable(unknown_var);
                         // Check b roughly 0
                         if (fabs(b) < 1e-9) { printf(COLOR_GREEN "True\n" COLOR_RESET); CalcResult r = {0,0}; return r; }
                         else { CalcResult r = {b, 0}; printf(COLOR_RED "False\n" COLOR_RESET); return r; }
                    }
                    
                    double root = -b / a;
                    set_variable(unknown_var, root);
                    printf(COLOR_GREEN "Solved %s = %.6g\n" COLOR_RESET, unknown_var, root);
                    CalcResult res = {root, 0};
                    return res;
                } else {
                    // Quadratic: Ax^2 + Bx + C = 0
                    // C = f0
                    // A + B = f1 - C
                    // 4A + 2B = f2 - C
                    double C = f0;
                    double eq1_rhs = f1 - C;
                    double eq2_rhs = f2 - C;
                    
                    // 2(A+B) = 2A + 2B = 2*eq1_rhs
                    // (4A+2B) - (2A+2B) = 2A
                    double A = (eq2_rhs - 2*eq1_rhs) / 2.0;
                    double B = eq1_rhs - A;
                    
                    // Roots
                    double disc = B*B - 4*A*C;
                    if (disc < 0) {
                        unset_variable(unknown_var);
                        printf(COLOR_RED "No real solutions.\n" COLOR_RESET);
                         CalcResult err = {0, 6}; return err; 
                    }
                    
                    double r1 = (-B + sqrt(disc)) / (2*A);
                    double r2 = (-B - sqrt(disc)) / (2*A);
                    
                    printf(COLOR_GREEN "Solved %s = %.6g, %.6g\n" COLOR_RESET, unknown_var, r1, r2);
                    // Use r1
                    set_variable(unknown_var, r1);
                    CalcResult res = {r1, 0};
                    return res;
                }
                
            } else if (count == 0) {
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
