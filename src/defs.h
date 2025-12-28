#ifndef DEFS_H
#define DEFS_H

// --- Constants ---
#ifndef VERSION
    #define VERSION "v1.0.0-dev"
#endif

#define HISTORY_MAX 50
#define BUFFER_SIZE 256
#define MAX_VARS 50
#define VAR_NAME_LEN 32

// --- Colors ---
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_BOLD "\033[1m"

// --- Structs ---
typedef struct {
    double result;
    int error; // 0=OK, 1=DivZero, 2=BadOp, 3=BadExpression, 4=UnknownVar, 5=SyntaxError, 6=InvalidArgument
} CalcResult;

typedef struct {
    char name[VAR_NAME_LEN];
    double value;
} Variable;

#endif
