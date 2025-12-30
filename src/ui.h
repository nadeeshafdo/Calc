#ifndef UI_H
#define UI_H

void enableRawMode();
void disableRawMode();
void get_input_raw(char *buffer);
void add_to_history(const char *cmd);
void print_banner();
void print_help();

// --- Plotting ---
void plot_function(const char *func_name_or_expr);

#endif
