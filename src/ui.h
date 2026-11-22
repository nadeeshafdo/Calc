#ifndef UI_H
#define UI_H

#include "defs.h"

void enableRawMode();
void disableRawMode();
void get_input_raw(char *buffer);
void add_to_history(const char *cmd);
void print_banner();
void print_help();

// Tab completion: collect keywords/variables/functions starting with prefix.
// Returns number of matches (up to max_out). Pure logic, testable.
int collect_completions(const char *prefix, char out[][VAR_NAME_LEN],
                        int max_out);

// --- Plotting ---
void plot_function(const char *func_name_or_expr);

#endif
