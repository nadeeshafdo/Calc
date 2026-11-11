#include "stack.h"

void d_push(DoubleStack *s, double val) {
  if (s->top < MAX_STACK - 1) {
    s->data[++(s->top)] = val;
  } else {
    s->error = 1;
  }
}
double d_pop(DoubleStack *s) {
  if (s->top >= 0) {
    return s->data[(s->top)--];
  }
  s->error = 1;
  return 0.0;
}

void c_push(CharStack *s, char val) {
  if (s->top < MAX_STACK - 1) {
    s->data[++(s->top)] = val;
  } else {
    s->error = 1;
  }
}
char c_pop(CharStack *s) {
  if (s->top >= 0) {
    return s->data[(s->top)--];
  }
  s->error = 1;
  return 0;
}
char c_peek(CharStack *s) {
  return (s->top >= 0) ? s->data[s->top] : 0;
}