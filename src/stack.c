#include "stack.h"

void d_push(DoubleStack *s, double val) { if (s->top < MAX_STACK - 1) s->data[++(s->top)] = val; }
double d_pop(DoubleStack *s) { return (s->top >= 0) ? s->data[(s->top)--] : 0.0; }

void c_push(CharStack *s, char val) { if (s->top < MAX_STACK - 1) s->data[++(s->top)] = val; }
char c_pop(CharStack *s) { return (s->top >= 0) ? s->data[(s->top)--] : 0; }
char c_peek(CharStack *s) { return (s->top >= 0) ? s->data[s->top] : 0; }
