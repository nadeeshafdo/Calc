#ifndef STACK_H
#define STACK_H

#define MAX_STACK 100

typedef struct { double data[MAX_STACK]; int top; } DoubleStack;
typedef struct { char data[MAX_STACK]; int top; } CharStack;

void d_push(DoubleStack *s, double val);
double d_pop(DoubleStack *s);
void c_push(CharStack *s, char val);
char c_pop(CharStack *s);
char c_peek(CharStack *s);

#endif
