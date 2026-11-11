#include <ctype.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>

#include "../manatui.h"
#include "./textarea_syntax_colors.h"


// --------------------------------------------------------------------------
// Colors 
// --------------------------------------------------------------------------
static int global_color_pair_counter = 1;
static int global_color_id_counter = 16; // avoid overwritting the default colors
static TextAreaActiveTheme textarea_active_theme; // color ids for the active theme
const TextareaTheme* active_theme = &default_theme;

// Converts a hex color to the ncurses format
int _register_hex_color(int color_id, const char* hex_str) {
  if (hex_str == NULL || hex_str[0] != '#') return -1;

  unsigned int r, g, b;

  // get the hex values from hex_str
  sscanf(hex_str + 1, "%02x%02x%02x", &r, &g, &b);

  // convert to what ncurses expects - from 0-255 scale to 0-1000 scale
  short ncurses_r = (short)((r * 1000) / 255);
  short ncurses_g = (short)((g * 1000) / 255);
  short ncurses_b = (short)((b * 1000) / 255);

  if (init_color(color_id, ncurses_r, ncurses_g, ncurses_b) != OK) return -1;
  return 0;
}


// --------------------------------------------------------------------------
// UTF-8 Helper Functions 
// --------------------------------------------------------------------------

// Returns the previous valid UTF-8 character start position
int _textarea_find_char_start(const char* str, int pos) {
  if (str == NULL || pos <= 0) return 0;
  
  // Move backward until we find a non-continuation byte
  while (pos > 0 && ((str[pos] & 0xC0) == 0x80)) {
    pos--;
  }
  return pos;
}

// Returns the next valid UTF-8 character start position
int _textarea_next_char_start(const char* str, int pos, int len) {
  if (str == NULL || pos >= len) return len;
  
  pos++;
  while (pos < len && ((str[pos] & 0xC0) == 0x80)) {
    pos++;
  }
  return pos;
}


// --------------------------------------------------------------------------
// Starting and Ending 
// --------------------------------------------------------------------------

// Start app
Application* manatui_init() {
  setlocale(LC_ALL, "");
  
  initscr();
  noecho();
  raw();
  cbreak();
  keypad(stdscr, TRUE);

  if (has_colors()) {
    start_color();
  }
  
  Application* app = (Application*)malloc(sizeof(Application));
  if (app == NULL) exit(1);

  app->focused_container = NULL;
  app->total_containers = 0;
  app->defer_list = NULL;

  return app;
}

// Adds a new container to the focusable list
void app_add_container(Application* app, void* container) {
  if (app == NULL || container == NULL) return;
  Container* con = (Container*)container;
  if (app->total_containers < MAX_CONTAINERS) {
    app->container_list[app->total_containers] = con;
    app->total_containers++;
  }
}

// Move the focus to the specified container
void app_focus_on(Application* app, void* con) {
  if (app == NULL || con == NULL) return;
  Container* obj = (Container*)con;
  obj->is_focused = FALSE;
  app->focused_container = (Container*)con;
}

// register objects to be freed at the end of the app (manatui_end)
void app_defer_free(Application* app, void* ptr) {
  if (app == NULL || ptr == NULL) return;

  DeferFreeList* list = NULL;
  // if the defer list was never created, then create it now
  if (app->defer_list == NULL) {
    DeferFreeList* temp_list = (DeferFreeList*)malloc(sizeof(DeferFreeList));
    if (temp_list == NULL) {
      fprintf(stderr, "Failed to allocate memory for App Defer List.\n");
      exit(1);
    }
    // success on allocating memory for the defer list
    app->defer_list = temp_list;
    app->defer_list->capacity = 0;
    app->defer_list->pointers = NULL;
    app->defer_list->total = 0;
  }
  list = app->defer_list;

  // request memory for the defer list if necessary
  if (list->total >= list->capacity) {
    int new_capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
    void** temp = (void**)realloc(list->pointers, new_capacity * sizeof(void*));
    if (temp == NULL) {
      free(ptr);
      return;
    }
    list->pointers = temp;
    list->capacity = new_capacity;
  }

  // add a new pointer (ptr) to the defer list
  list->pointers[list->total] = ptr;
  list->total++;
}

// End ncurses
void manatui_end(Application* app) {
  endwin();

  // free all registered objects
  if (app->defer_list != NULL && app->defer_list->pointers != NULL) {
    for (int i = 0; i < app->defer_list->total; i++) {
      if (app->defer_list->pointers[i] != NULL) {
        free(app->defer_list->pointers[i]);
      }
    }
    free(app->defer_list->pointers);
    free(app->defer_list);
  }
  free(app);
}

// Handle all key inputs
void app_key_handle(Application* app, unsigned int c) {
  if (app == NULL) return;

  if (c == ctrl('q')) {
    manatui_end(app);
    exit(0);
  }

  // used to check if the focused container is changed after handling the current container focus
  Container* initial_con = app->focused_container;

  // global actions
  switch (c) {
    case '\t':
      break;
  }

  // handle container focus
  if (app->focused_container != NULL && app->focused_container->on_focus != NULL) {
    app->focused_container->on_focus(c, app->focused_container->user_data);
  }

  unsigned int new_c = c;
  // the container actions function receives 0 if the container was changed to prevent processing the old key press
  if (initial_con != app->focused_container) {
    new_c = 0;
  }

  // handle container default actions (a list handles arrow keys, for example)
  if (app->focused_container != NULL && app->focused_container->actions != NULL) {
    app->focused_container->actions(app->focused_container, new_c);
  }
}

// Application loop
void manatui_loop(Application* app) {
  if (app == NULL) return;

  // make sure to initialize the focused container when the loop starts
  if (app->focused_container != NULL) {
    container_update(app->focused_container);
  }

  unsigned int c; // unsigned allows using the extended ASCII codes
  while ((c = getch())) {
    app_key_handle(app, c);
  }
}

// works as malloc or realloc, while registering the new object to the defer free list
void* app_alloc(Application* app, void* old_ptr, size_t size) {
  if (app == NULL) return NULL;

  // given a size 0, acts as a free function
  if (size == 0) {
    if (old_ptr != NULL) {
      // remove from the defer list
      for (int i = 0; i < app->defer_list->total; i++) {
        // when find the pointer, free and substitute
        if (app->defer_list->pointers[i] == old_ptr) {
          app->defer_list->pointers[i] = NULL;
          break;
        }
      }
      free(old_ptr);
    }
    return NULL;
  }

  // works as malloc if origin is NULL
  void* new_ptr = (void*)realloc(old_ptr, size);
  // on fail, end ncurses and clear memory safely
  if (new_ptr == NULL) {
    fprintf(stderr, "Critical error: failed allocating memory.\n");
    manatui_end(app);
    exit(1);
  }

  // if the object already exists, find it in the defer free list and substitute the pointer for the new one
  if (old_ptr != NULL && old_ptr != new_ptr) {
    bool substituting_success = 0;

    // look for the old pointer in the defer list
    for (int i = 0; i < app->defer_list->total; i++) {
      // when find the pointer, free and substitute
      if (app->defer_list->pointers[i] == old_ptr) {
        app->defer_list->pointers[i] = new_ptr;
        substituting_success = TRUE;
        break;
      }
    }
    // if it was not in the list, then adds it
    if (!substituting_success) {
      app_defer_free(app, new_ptr);
    }
  }

  // if the object is completely new, just add the realloc resulting pointer to the defer free list
  else if (old_ptr == NULL) {
    app_defer_free(app, new_ptr);
  }

  return new_ptr;
}

// CTRL + key - doing this as a function helps on portability to other languages
unsigned int ctrl(unsigned int c) {
  return ((c) & (0x1f));
}


// --------------------------------------------------------------------------
// Containers 
// --------------------------------------------------------------------------

// Initialize a container inside a parent
// Obj is a component based on Container
void container_init(void* obj) {
  if (obj == NULL) exit(1);

  Container* con = (Container*)obj;

  con->dwin = derwin(con->parent, con->height, con->width, con->start_y, con->start_x);

  // so we have to activate keypad for every window
  keypad(con->dwin, TRUE);
}

// Working with base containers
Container* container_create(WINDOW* parent, int height, int width, int start_y, int start_x, const char* title, bool has_border, void (*callback)(int, void*)) {
  Container* temp = (Container*)malloc(sizeof(Container));
  if (temp == NULL || parent == NULL) exit(1);

  temp->height = height;
  temp->width = width;
  temp->start_y = start_y;
  temp->start_x = start_x;
  temp->on_focus = callback;
  temp->actions = NULL;
  temp->title = title;
  temp->has_border = has_border;
  temp->user_data = NULL;
  temp->parent = parent;
  temp->type = TYPE_CONTAINER;

  // Initialize before printing any content inside it
  container_init(temp);
  return temp;
}

// print text on the container using va_list
void vcontainer_print(Container* con, bool break_line, int y, int x, const char* format, va_list args) {
    if (!con || !con->dwin) return;

    va_list args_copy;
    va_copy(args_copy, args);
    int necessary_size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (necessary_size < 0) return;

    char* buffer = malloc(necessary_size + 1);
    if (!buffer) return;

    int size = break_line ? (necessary_size + 1) : (con->width - 1);
    vsnprintf(buffer, size, format, args);

    // instead of iterating byte‑by‑byte, print the whole string at once.
    wmove(con->dwin, y, x);
    wprintw(con->dwin, "%.*s", con->width - x - 1, buffer); // leave room for right border

    free(buffer);
}

// print text on the container
void container_print(Container *con, bool break_line, int y, int x, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vcontainer_print(con, break_line, y, x, format, args);
  va_end(args);
}

// update the container after some change
// obj is a component based on Container
void container_update(void* obj) {
  if (obj == NULL) return;

  Container* con = (Container*)obj;
  if (con->dwin == NULL) return;

  // start the colors
  if (con->color_pair_id > 0) {
    wattron(con->dwin, COLOR_PAIR(con->color_pair_id));
    wbkgd(con->dwin, COLOR_PAIR(con->color_pair_id));
  }

  // first draw the borders if they exist
  if (con->has_border) {
    box(con->dwin, 0, 0);
 
    // print title if it has one
    if (con->title != NULL && con->title[0] != '\0') {
      mvwprintw(con->dwin, 0, 2, "< %s >", con->title);
    }
  }

  // end the colors
  if (con->color_pair_id > 0) {
    wattroff(con->dwin, COLOR_PAIR(con->color_pair_id));
  }

  // the update
  wnoutrefresh(con->parent);
  wnoutrefresh(con->dwin);
  doupdate();
}

// Apply color system
void container_apply_style(void* con) {
  if (!has_colors()) return;

  Container* container = (Container*)con;

  // init common color system
  if (container->foreground != NULL && container->background != NULL) {
    int fg_id = global_color_id_counter++;
    int bg_id = global_color_id_counter++;
    int pair_id = global_color_pair_counter++;

    // init colors
    _register_hex_color(fg_id, container->foreground);
    _register_hex_color(bg_id, container->background);

    // init color pair if it succeeds
    if (init_pair(pair_id, fg_id, bg_id) == OK) {
      container->color_pair_id = pair_id;
    } else {
      container->color_pair_id = -1;
    }
  }
}


// --------------------------------------------------------------------------
// Buttons 
// --------------------------------------------------------------------------

// Creates a new button instance
Button* button_create(WINDOW* parent, int height, int width, int start_y, int start_x, const char* label, void (*callback)(int, void*)) {
  Button* btn = (Button*)malloc(sizeof(Button));
  if (btn == NULL || parent == NULL) exit(1);

  // set up base container
  btn->base.height = height;
  btn->base.width = width;
  btn->base.start_y = start_y;
  btn->base.start_x = start_x;
  btn->base.parent = parent;
  btn->base.on_focus = callback;
  btn->base.is_focused = FALSE;
  btn->base.has_border = TRUE;
  btn->base.actions = NULL;
  btn->base.user_data = NULL;
  btn->base.type = TYPE_BUTTON;

  container_init(btn);

  if (strlen(label) >= sizeof(btn->label)) {
    // truncate
    strncpy(btn->label, label, sizeof(btn->label) - 1);
    btn->label[sizeof(btn->label) - 1] = '\0';
  }
  snprintf(btn->label, sizeof(btn->label), "%s", label);
  container_update(btn);

  int center_x = (width / 2) - (strlen(btn->label) / 2);  
  container_print(&btn->base, FALSE, 1, 1, "%s", btn->label);

  wnoutrefresh(btn->base.dwin);
  doupdate();
  
  return btn;
}

// position the cursor inside the specified button and add it to the app->focused_container
void _button_actions(void* context, int c) {
  if (context == NULL) {
    fprintf(stderr, "Failed to start button actions.\n");
    exit(1);
  }

  curs_set(0);
  Button* btn = (Button*)context;

  int position_x = (btn->base.width / 2) - (strlen(btn->label) / 2);
  // 
  // print label in BOLD
  wclear(btn->base.dwin);

  wattron(btn->base.dwin, A_BOLD);
  container_print((Container*)btn, FALSE, 1, position_x, "%s", btn->label);
  wattroff(btn->base.dwin, A_BOLD); // turn off BOLD

  int pos_x = btn->base.width / 2;
  wmove(btn->base.dwin, 1, pos_x);
  container_update(btn);
  curs_set(1);
}

void button_render(Application* app, Button* button) {
  if (app == NULL || button == NULL) return;
  app_add_container(app, button);
  container_apply_style(button);
  _button_actions(button, 0);
}


// --------------------------------------------------------------------------
// Lists 
// --------------------------------------------------------------------------

void _list_draw(List* list) {
  if (list == NULL) return;
  
  // hide the cursor
  curs_set(0);

  // redraw the border and the title
  container_update(list);

  int visible_height = list->base.height - 2;
  int usable_width = list->base.width - 2;

  // draw only visible items of the list
  for (int i = 0; i < visible_height; i++) {
    int real_index = list->scroll_top + i;
    int screen_row = i + 1;

    if (real_index < list->items) {
      if (real_index == list->selected) {
        // print selected item with inverted colors
        wattron(list->base.dwin, A_REVERSE);
        container_print((Container*)list, FALSE, screen_row, 1, "%-*s", usable_width, list->content[real_index]);
        wattroff(list->base.dwin, A_REVERSE);
      } else {
        container_print((Container*) list, FALSE, screen_row, 1, "%-*s", usable_width, list->content[real_index]);
      }
    } else { // there is no item for this visible line
      // remove trash from the screen
      container_print((Container*)list, FALSE, screen_row, 1, "%-*s", usable_width, "");
    }
  }

  // now positining the cursor over the selected item
  int visual_selected_row = (list->selected - list->scroll_top) + 1;
  wmove(list->base.dwin, visual_selected_row, 1);
  
  // force updating the visual buffer
  wnoutrefresh(list->base.dwin);
  doupdate();

  // show the cursor again
  curs_set(1);
}

// Select an item at the given position
void list_item_select(List *list, const int position) {
  if (list == NULL) return;
  if (position < 0 || position >= list->items) return;

  list->selected = position;

  // adjust scroll to make the selected item visible
  int visible_height = list->base.height - 2;
  if (list->selected < list->scroll_top) {
    list->scroll_top = list->selected;
  }
  if (list->selected >= list->scroll_top + visible_height) {
    list->scroll_top = list->selected - visible_height + 1;
  }

  // update list
  _list_draw(list);
}

// render the list component
void list_render(Application* app, List* list) {
  if (list == NULL || list->base.dwin == NULL) return;

  app_add_container(app, (Container*)list);

  // apply foreground and background when they are set
  if (list->base.foreground != NULL) {
    if (list->base.background == NULL) {
      list->base.background = "#000000";
    }
    container_apply_style(list);  
  }
  _list_draw(list);
}
 
// This executes when the list is focused, with the purpose of managing default actions of each list
void _list_actions(void* context, unsigned int c) {
  if (context == NULL) return;

  // if the list is focused, we know that it is exaclty the ACTIVE_CONTAINER
  List* list = (List*)context;

  if (list == NULL || list->items == 0) return;

  int visible_height = list->base.height - 2;

  // handle arrow keys
  switch (c) {
    case KEY_DOWN:
      list_item_select(list, list->selected + 1); // select next item
      break;

    case KEY_UP:
      list_item_select(list, list->selected - 1); // select previous item
      break;
  }

  // update scroll_top if the selected item is over the list view
  if (list->selected < list->scroll_top) {
    list->scroll_top = list->selected;
  }

  // update scroll_top if the selected item is under the list view
  if (list->selected >= list->scroll_top + visible_height) {
    list->scroll_top = list->selected - visible_height + 1;
  }
  
  _list_draw(list);
}

// Creates a new list and returns its pointer
List* list_create(WINDOW* parent, int height, int width, int start_y, int start_x, const char* title, bool has_border, void (*callback)(int, void*)) {
  List* temp = (List*)malloc(sizeof(List));
  if (temp == NULL || parent == NULL) exit(1);

  // init Container members
  temp->base.height = height;
  temp->base.width = width;
  temp->base.start_y = start_y;
  temp->base.start_x = start_x;
  temp->base.has_border = has_border;
  temp->base.on_focus = callback;
  temp->base.title = title;
  temp->base.parent = parent;
  temp->base.type = TYPE_LIST;

  // List properties
  temp->content = NULL;
  temp->selected = 0;
  temp->items = 0;
  temp->base.actions = _list_actions;
  temp->base.user_data = NULL;
  temp->scroll_top = 0;

  // init the List's derwin
  container_init(temp);

  return temp;
}

// Saves the recently added text line in the list->content string array
void _list_content_add(List* list, const char* line) {
  if (list == NULL || line == NULL) return;

  char** new_content = (char**)realloc(list->content, list->items * sizeof(char*));
  if (new_content == NULL) exit(1);

  list->content = new_content;
  list->content[list->items - 1] = strdup(line); // make a copy of the string instead of storing the pointer directly
}

// add a new item to the list
void list_item_add(List* list, const char* line, ...) {
  if (list == NULL || line == NULL) return;

  // process the string on a temporary, clean, buffer
  va_list args;
  va_start(args, line);

  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(NULL, 0, line, args_copy);
  va_end(args_copy);

  if (needed < 0) { va_end(args); return; }

  char* formatted_line = (char*)malloc(needed + 1);
  if (formatted_line == NULL) { va_end(args); exit(1); }

  vsnprintf(formatted_line, needed + 1, line, args);
  va_end(args);

  list->items += 1; // increase amount of lines - it also acts a way to know in which line of the list to print the next line
  list->selected = 0;// select item 0
  _list_content_add(list, formatted_line); // register the line in the list->content

  // force update
  int target_row = list->items;
  container_print((Container*)list, FALSE, target_row, 1, "%s", formatted_line);
}


// --------------------------------------------------------------------------
// TextInput 
// --------------------------------------------------------------------------

// returns the amount of real characters in an interval
int _textinput_get_visual_distance(const char* str, int start_byte, int end_byte) {
  if (str == NULL) return 0;

  int visual_width = 0;
  for (int i = start_byte; i <= end_byte; i++) {
    if (str[i] == '\0') break;

    // count as a visual character if it is not a continuation byte
    if ((str[i] & 0xC0) != 0x80) {
      visual_width++;
    }
  }
  return visual_width;
}

// add a char at the given position
void _textinput_add_char(TextInput* input, int position, unsigned int c) {
  if (input == NULL) return;
  if (position > input->content_size || position >= sizeof(input->content) - 1 || position < 0) return;

  for (int i = input->content_size; i > position; i--) {
    input->content[i] = input->content[i - 1];
  }

  input->content[position] = (char)c;
  input->content_size++;
  input->content[input->content_size] = '\0';
}

// remove a char from the given position
void _textinput_remove_char(TextInput* input, int position) {
  if (input == NULL) return;
  if (position < 0 || position > input->content_size) return;

  for (int i = position; i < input->content_size - 1; i++) {
    input->content[i] = input->content[i + 1];
  }

  input->content_size--;
  input->content[input->content_size] = '\0';
}

// the default behavior of the text input
void _textinput_default_actions(void* context, unsigned int c) {
  if (context == NULL) return;

  TextInput* input = (TextInput*)context;

  curs_set(0);

  int usable_width = input->base.width - 2;

  // navigating through the text
  if (c == KEY_LEFT) {
    if (input->cursor_pos > 0) {
      // skip left continuation bytes
      do {
        input->cursor_pos--;
      } while (input->cursor_pos > 0 && (input->content[input->cursor_pos] & 0xC0) == 0x80);
    }
  }
  else if (c == KEY_RIGHT) {
    if (input->cursor_pos < input->content_size) {
      // skip right continuation bytes
      do {
        input->cursor_pos++;
      } while (input->cursor_pos < input->content_size && (input->content[input->cursor_pos] & 0xc0) == 0x80);
    }
  }

  // removing characters (BACKSPACE)
  else if (c == KEY_BACKSPACE || c == 127 || c == 8) {
    if (input->cursor_pos > 0 && input->disabled == FALSE) {
      // count how many bytes are in the UTF-8 char at the left side
      int bytes_to_delete = 1;
      while (input->cursor_pos - bytes_to_delete > 0) {
        if ((input->content[input->cursor_pos] & 0xc0) == 0x80) {
          bytes_to_delete++;
        } else {
          break;
        }
      }

      // remove all corresponding bytes
      for (int i = 0; i < bytes_to_delete; i++) {
        _textinput_remove_char(input, input->cursor_pos - bytes_to_delete);
      }
      input->cursor_pos -= bytes_to_delete; // the cursor walks 1 or bytes_to_delete character to the left after removing a char
    }
  }
  
  // adding characters (typing)
  // accept ASCII characters between 32-126 and extended UTF-8 chars 128-254
  else if (((c >= 32 && c <= 126) || (c >= 128 && c<= 254)) && input->disabled == FALSE) {
    _textinput_add_char(input, input->cursor_pos, c);
    input->cursor_pos++; // the cursor advances after typing
  }

  // update boders and title
  container_update(input);

  // calculate which portion of the text to show and where to position the cursor
  int visual_cursor = _textinput_get_visual_distance(input->content, 0, input->cursor_pos);
  int visual_text_pos = _textinput_get_visual_distance(input->content, 0, input->text_pos);

  // if the text fits int the input width, resets the scroll
  int total_visual_chars = _textinput_get_visual_distance(input->content, 0, input->content_size);
  if (total_visual_chars < usable_width) {
    input->text_pos = 0;
    visual_text_pos = 0;
  }
  else {
    // the cursor moves beyond the left visible area
    if (visual_cursor < visual_text_pos) {
      input->text_pos = input->cursor_pos;
      visual_text_pos = visual_cursor;
    }
    // the cursor moves beyond the right visible area
    if (visual_cursor >= visual_text_pos + usable_width) {
      while (_textinput_get_visual_distance(input->content, input->text_pos, input->cursor_pos) >= usable_width) {
        input->text_pos++;
      }
    }
    visual_text_pos = _textinput_get_visual_distance(input->content, 0, input->text_pos);
  }

  //
  // Compatible Rendering -----------------
  // 
  char* printable_content = &input->content[input->text_pos];

  // calculate the number of bytes to print to avoid truncating UTF-8 chars
  int byte_limit = 0;
  int visual_count = 0;
  while (printable_content[byte_limit] != '\0' && visual_count < usable_width) {
    if ((printable_content[byte_limit] & 0xc0) != 0x80) {
      visual_count++;
    }
    byte_limit++;
  }

  // print the safe and visible portion of the text
  container_print((Container*)input, FALSE, 1, 1, "%.*s", byte_limit, printable_content);

  // clear the rest of the visible line (remove trash chars from screen)
  int spaces_to_fill = usable_width - visual_count;
  if (spaces_to_fill > 0) {
    container_print((Container*)input, FALSE, 1, 1 + visual_count, "%*s", spaces_to_fill, "");
  }

  // where the cursor visually appears in the input
  int curs_pos_at_container = visual_cursor - visual_text_pos;
  wmove(input->base.dwin, 1, curs_pos_at_container + 1);

  // update
  wnoutrefresh(input->base.dwin);
  doupdate();

  curs_set(1);
}

// Adds a string as the textinput content
void textinput_set(TextInput* input, const char* str) {
  if (input == NULL || str == NULL) return;
  memcpy(input->content, str, strlen(str));
  input->content_size = strlen(str);
}

// create a new TextInput
TextInput* textinput_create(WINDOW* parent, int width, int start_y, int start_x, const char* label, void (*callback)(int, void*)) {
  if (parent == NULL) return NULL;
  
  TextInput* input = (TextInput*)malloc(sizeof(TextInput));
  if (input == NULL) exit(1);

  input->content[0] = '\0';
  input->content_size = 0;
  input->cursor_pos = 0;
  input->text_pos = 0;
  input->disabled = FALSE;

  // base container properties
  input->base.actions = _textinput_default_actions;
  input->base.has_border = TRUE;
  input->base.height = 3; // top and bottom borders plus content in one line
  input->base.on_focus = callback;
  input->base.parent = parent;
  input->base.start_x = start_x;
  input->base.start_y = start_y;
  input->base.width = width;
  input->base.user_data = NULL;
  input->base.title = label;
  input->base.type = TYPE_TEXTINPUT;

  container_init(input);
  container_update(input);
  wnoutrefresh(input->base.dwin);
  doupdate();

  return input;
}

// Force re-drawing the input when needed.
// For example, when you create a disabled input and then add text to it - you should re-render the input.
void textinput_render(Application* app, TextInput* input) {
  app_add_container(app, input);
  _textinput_default_actions(input, 0);
}


// --------------------------------------------------------------------------
// TextArea 
// --------------------------------------------------------------------------

// calculates and sets the number of digits for the lines numbers
void _textarea_set_line_width(TextArea* textarea) {
  int temp_lines = textarea->total_lines;
  int digits = 1;

  while (temp_lines >= 10) {
    digits++;
    temp_lines /= 10;
  }

  textarea->line_number_width = digits;
}

int _textarea_get_max_visible_lines(TextArea* textarea) {
  int max_visible_lines = textarea->base.height - 2;
  if (max_visible_lines < 1) max_visible_lines = 1;
  return max_visible_lines;
}

int textarea_get_usable_width(TextArea* textarea) {
  int usable_width = textarea->base.width - textarea->line_number_width - 3;
  if (usable_width < 1) usable_width = 1;
  return usable_width;
}

// Handles extended ASCII characters, by ignoring "virtual" characters from UTF-8
int _textarea_get_visual_distance(const char* str, int start_byte, int end_byte) {
  if (str == NULL) return 0;

  int visual_width = 0;
  for (int i = start_byte; i < end_byte; i++) {
    if (str[i] == '\0') break;
    // if it's not a continuation byte (10xxxxxx), acts a real visual char on the screen
    if ((str[i] & 0xC0) != 0x80) {
      visual_width++;
    }
  }
  return visual_width;
}

// Handles cursor position and text scroll after KEY_DOWN is pressed
bool textarea_handle_key_down(TextArea* textarea, int max_visible_lines) { 
  bool redraw_box = FALSE;
  if (textarea->total_lines - 1 > textarea->cursor_row) {

    textarea->cursor_row++;

    // if the bottom line is smaller than the current
    int next_line_len = strlen(textarea->lines[textarea->cursor_row]);
    if (textarea->cursor_col > next_line_len) {
      textarea->cursor_col = next_line_len;
    }

    // fix the cursor visibility if the new line is very short
    if (textarea->cursor_col < textarea->scroll_col) {
      textarea->scroll_col = textarea->cursor_col > 3 ? textarea->cursor_col - 3 : textarea->cursor_col;
      // Align scroll_col to valid character start
      if (textarea->scroll_col > 0 && textarea->cursor_row < textarea->total_lines) {
        char* line = textarea->lines[textarea->cursor_row];
        if (line != NULL && textarea->scroll_col < (int)strlen(line)) {
          textarea->scroll_col = _textarea_find_char_start(line, textarea->scroll_col);
        }
      }
      redraw_box = TRUE;
    }

    // scroll the text if necessary
    if (textarea->cursor_row >= textarea->scroll_row + max_visible_lines) {
      textarea->scroll_row = textarea->cursor_row - max_visible_lines + 1;
      redraw_box = TRUE;
    }
  }
  return redraw_box;
}

// Handles cursor position and text scroll after KEY_UP is pressed
void textarea_handle_key_up(TextArea* textarea) { 
  if (textarea->cursor_row > 0) {
    textarea->cursor_row--;

    // adjust cursor position if the above line is shorter
    int prev_line_len = strlen(textarea->lines[textarea->cursor_row]);
    if (textarea->cursor_col > prev_line_len) {
      textarea->cursor_col = prev_line_len;
    }

    // fix the cursor visibility if the new line is very short
    int usable_width = textarea_get_usable_width(textarea);
    if (textarea->cursor_col < textarea->scroll_col) {
      textarea->scroll_col = textarea->cursor_col > 3 ? textarea->cursor_col - 3 : 0;
      // Align scroll_col to valid character start
      if (textarea->scroll_col > 0 && textarea->cursor_row < textarea->total_lines) {
        char* line = textarea->lines[textarea->cursor_row];
        if (line != NULL && textarea->scroll_col < (int)strlen(line)) {
          textarea->scroll_col = _textarea_find_char_start(line, textarea->scroll_col);
        }
      }
    }
    else if (textarea->cursor_col >= textarea->scroll_col + usable_width) {
      textarea->scroll_col = textarea->cursor_col - usable_width + 1;
      // Align scroll_col to valid character start
      if (textarea->scroll_col > 0 && textarea->cursor_row < textarea->total_lines) {
        char* line = textarea->lines[textarea->cursor_row];
        if (line != NULL && textarea->scroll_col < (int)strlen(line)) {
          textarea->scroll_col = _textarea_find_char_start(line, textarea->scroll_col);
        }
      }
    }

    // returns the text scroll back if necessary
    if (textarea->cursor_row < textarea->scroll_row) {
      textarea->scroll_row = textarea->cursor_row;
    }
  }
}

// Handles cursor position and text scroll after KEY_RIGHT is pressed
void textarea_handle_key_right(TextArea* textarea) {
  if (textarea == NULL) return;
  
  int current_line_len = strlen(textarea->lines[textarea->cursor_row]);
  int usable_width = textarea_get_usable_width(textarea);
    
  if (textarea->cursor_col < current_line_len) {
    // Find next character start (handle UTF-8)
    do {
      textarea->cursor_col++;
    } while (textarea->cursor_col < current_line_len && 
             (textarea->lines[textarea->cursor_row][textarea->cursor_col] & 0xC0) == 0x80);

    // scroll to the right
    if (textarea->cursor_col >= textarea->scroll_col + usable_width) {
      textarea->scroll_col = textarea->cursor_col - usable_width + 1;
      // Align scroll_col to a valid character start
      if (textarea->scroll_col > 0) {
        textarea->scroll_col = _textarea_find_char_start(
          textarea->lines[textarea->cursor_row], 
          textarea->scroll_col
        );
      }
    }
  }
  // cursor jumps to the start of the next line
  else if (textarea->cursor_row < textarea->total_lines - 1) {
    textarea->cursor_col = 0;
    textarea->scroll_col = 0;
    textarea->cursor_row++;

    // scroll down if necessary
    int max_visible_lines = _textarea_get_max_visible_lines(textarea);
    if (textarea->cursor_row >= textarea->scroll_row + max_visible_lines) {
      textarea->scroll_row++;
    }
  }
}

// Handles cursor position and text scroll after KEY_LEFT is pressed
void textarea_handle_key_left(TextArea *textarea) {
  if (textarea == NULL) return;
  
  if (textarea->cursor_col > 0) {
    do {
      textarea->cursor_col--;
    } while (textarea->cursor_col > 0 && 
             (textarea->lines[textarea->cursor_row][textarea->cursor_col] & 0xC0) == 0x80);

    // scroll to the left
    if (textarea->cursor_col < textarea->scroll_col) {
      textarea->scroll_col = textarea->cursor_col;
    }
  } else {
    // move the cursor to the end of the line above
    if (textarea->cursor_row > 0) {
      textarea->cursor_row--;
      textarea->cursor_col = strlen(textarea->lines[textarea->cursor_row]);

      // adjust scroll position when moving to previous line
      int usable_width = textarea_get_usable_width(textarea);
      int max_visible_lines = _textarea_get_max_visible_lines(textarea);

      // calculate where the cursor should be visually
      if (textarea->cursor_col > usable_width) {
        textarea->scroll_col = textarea->cursor_col - usable_width;
        // Align to valid character start
        char* line = textarea->lines[textarea->cursor_row];
        if (line != NULL && textarea->scroll_col < (int)strlen(line)) {
          textarea->scroll_col = _textarea_find_char_start(line, textarea->scroll_col);
        }
      } else {
        textarea->scroll_col = 0;
      }

      // ensure the cursor row is visible
      if (textarea->cursor_row < textarea->scroll_row) {
        textarea->scroll_row = textarea->cursor_row;
      }
      // check if we need to scroll up when cursor_row is at the top of the visible area
      else if (textarea->cursor_row >= textarea->scroll_row + max_visible_lines) {
        textarea->scroll_row = textarea->cursor_row - max_visible_lines + 1;
      }
    }
  }
}

// Handles the textarea content after pressing the ENTER key
void textarea_handle_key_enter(TextArea* textarea) {
  if (textarea == NULL || textarea->disabled) return;

  // the original size of the line
  int current_len = strlen(textarea->lines[textarea->cursor_row]);
  
  // the size of the line created
  int new_line_size = current_len - textarea->cursor_col;

  // the state of the original line after pressing ENTER (breaking the line)
  char* new_line = (char*)malloc((new_line_size + 1) * sizeof(char));
  if (new_line == NULL) exit(1);

  // copy the content for the new line
  strcpy(new_line, textarea->lines[textarea->cursor_row] + textarea->cursor_col);

  // updates the current line
  textarea->lines[textarea->cursor_row][textarea->cursor_col] = '\0';
  
  // free the unused memory from the original line
  char* shrunk_line = (char*)realloc(textarea->lines[textarea->cursor_row], (textarea->cursor_col + 1) * sizeof(char));
  if (shrunk_line != NULL) {
    textarea->lines[textarea->cursor_row] = shrunk_line;
  }

  // make sure there is enough space for a new line
  // NOTE: this conditional is block possibly capable of being separated into a dedicated function,
  // since it is used also in textarea_add_line.
  if (textarea->total_lines >= textarea->lines_capacity) {
    // start with capacity for 8 lines
    int new_capacity = (textarea->lines_capacity == 0) ? 8 : textarea->lines_capacity * 2;

    char** temp_lines = (char**)realloc(textarea->lines, new_capacity * sizeof(char*));
    if (temp_lines == NULL) exit(1);

    textarea->lines = temp_lines;
    textarea->lines_capacity = new_capacity;
  }

  // copy each line until we find the new line position
  for (int i = textarea->total_lines; i > textarea->cursor_row + 1; i--) {
    textarea->lines[i] = textarea->lines[i - 1];
  }

  // insert the new line
  textarea->lines[textarea->cursor_row + 1] = new_line;

  // update textarea state
  textarea->cursor_row++;
  textarea->total_lines++;
  textarea->cursor_col = 0; // start of the line
  textarea->scroll_col = 0; // reset scroll

  // update vertical scroll if necessary
  int max_visible_lines = _textarea_get_max_visible_lines(textarea);
  if (textarea->cursor_row >= textarea->scroll_row + max_visible_lines) {
    textarea->scroll_row++;
  }
}

//
// When scrolling to the right, if the scroll_col for bigger than this line, the printer will print from
// memory addressess after the string, this prevents that from happening, by giving " " (space) to the printer
// 
char* _textarea_get_printable_content(TextArea* textarea, int file_line_index) {
  if (textarea == NULL || textarea->lines == NULL || textarea->lines[file_line_index] == NULL) return " ";

  int line_len = strlen(textarea->lines[file_line_index]);
  int scroll = textarea->scroll_col;
  
  // Ensure scroll_col is within bounds and at a valid character start
  if (scroll > 0 && scroll < line_len) {
    // Align to character start if needed
    if ((textarea->lines[file_line_index][scroll] & 0xC0) == 0x80) {
      scroll = _textarea_find_char_start(textarea->lines[file_line_index], scroll);
    }
  }
  
  char* content = (scroll <= line_len)
                  ? &textarea->lines[file_line_index][scroll]
                  : " ";
  return content;
}

// Handle adding a char to the text
void _textarea_add_char(TextArea* textarea, unsigned int c) {
  if (textarea == NULL) return;

  // calculate the current lien length
  int current_len = strlen(textarea->lines[textarea->cursor_row]);

  // get space to allocate a new char and the null terminator
  char* temp_line = (char*)realloc(textarea->lines[textarea->cursor_row], (current_len + 2) * sizeof(char));
  if (temp_line == NULL) exit(1);

  textarea->lines[textarea->cursor_row] = temp_line;

  // move each character if necessary
  for (int i = current_len; i >= textarea->cursor_col; i--) {
    textarea->lines[textarea->cursor_row][i + 1] = textarea->lines[textarea->cursor_row][i];
  }

  // add the new character and update the cursor position
  textarea->lines[textarea->cursor_row][textarea->cursor_col] = (char)c;
  textarea->cursor_col++;

  // adjust scroll
  int usable_width = textarea_get_usable_width(textarea);
  if (textarea->cursor_col >= textarea->scroll_col + usable_width) {
    textarea->scroll_col = textarea->cursor_col - usable_width + 1;
    // Align scroll_col to valid character start
    if (textarea->scroll_col > 0) {
      textarea->scroll_col = _textarea_find_char_start(
        textarea->lines[textarea->cursor_row], 
        textarea->scroll_col
      );
    }
  }
}

// remove a character just at the left from the current cursor position
void _textarea_remove_left_char(TextArea* textarea) {
  if (textarea == NULL) return;

  // NOTE case 01: line 0, column 0
  if (textarea->cursor_col == 0 && textarea->cursor_row == 0) {
    return;
  }

  // NOTE case 02: default behavior - take characters from right to left
  if (textarea->cursor_col > 0) {
    // length of the current line
    int line_len = strlen(textarea->lines[textarea->cursor_row]);
    char* current_line = textarea->lines[textarea->cursor_row];

    // get how many characters are at the left of the cursor
    int bytes_to_delete = 1;
    while (textarea->cursor_col - bytes_to_delete > 0) {
      if ((current_line[textarea->cursor_col - bytes_to_delete] & 0xC0) == 0x80) {
        bytes_to_delete++;
      } else {
        break; // yee the main byte
      }
    }

    // move characters from right to left, overwitting the target
    for (int i = textarea->cursor_col; i <= line_len; i++) {
      current_line[i - bytes_to_delete] = current_line[i];
    }

    textarea->cursor_col -= bytes_to_delete; // update cursor

    // update scroll if the cursor is at a position before the visible area
    if (textarea->cursor_col < textarea->scroll_col) {
      textarea->scroll_col = textarea->cursor_col;
    }

    // ensures visual representation is cleaned up
    // this is really important!
    int new_len = strlen(current_line);
    for (int i = new_len; i < line_len; i++) {
      current_line[i] = '\0';
    }

    return;
  }

  // NOTE case 03: when the cursor is at the begining of a line, merge this line with the end of the line above
  int prev_row = textarea->cursor_row - 1;
  int prev_len = strlen(textarea->lines[prev_row]);
  int current_len = strlen(textarea->lines[textarea->cursor_row]);

  // 1 - get enough space for the previous line
  char* new_prev_line = (char*)realloc(textarea->lines[prev_row], (prev_len + current_len + 1) * sizeof(char));
  if (new_prev_line == NULL) return;
  textarea->lines[prev_row] = new_prev_line;

  // merge these two lines
  strcpy(textarea->lines[prev_row] + prev_len, textarea->lines[textarea->cursor_row]);
  free(textarea->lines[textarea->cursor_row]); // free the moved line

  // move all the lines, after the recently moved one, one position up
  for (int i = textarea->cursor_row; i < textarea->total_lines - 1; i++) {
    textarea->lines[i] = textarea->lines[i + 1];
  } 
  textarea->lines[textarea->total_lines - 1] = NULL; // clear the last index
  textarea->total_lines--;

  // move the cursor to where the lines were merged
  textarea->cursor_row--;
  textarea->cursor_col = prev_len;

  // update the visible area
  if (textarea->cursor_row < textarea->scroll_row) {
    textarea->scroll_row = textarea->cursor_row;
  }

  int usable_width = textarea_get_usable_width(textarea);
  if (textarea->cursor_col >= textarea->scroll_col + usable_width) {
    textarea->scroll_col = textarea->cursor_col - usable_width + 1;
  }
  else if (textarea->cursor_col < textarea->scroll_col) {
    textarea->scroll_col = textarea->cursor_col;
  }
}

// if the textarea->tabs_for_spaces is set to true, write 4 ' ' characters when the user press tab
void _textarea_handle_tabs(TextArea* textarea) {
  if (textarea == NULL) {
    return;
  }
  if (textarea->tabs_for_spaces && !textarea->disabled) {
    for (int i = 0; i < 4; i++) {
      _textarea_add_char(textarea, ' ');
    }
  }
}

bool _char_is_alphanumeric(char c) {
  unsigned char uc = (unsigned char)c;
  return (uc == '_') || (uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || (uc >= '0' && uc <= '9') || (uc >= 0xC0);
}

// detects if the next characters represent a word (a sequence of alphanumeric characters).
// it returns the amount of BYTES that compose the word or -1 if it is not a word.
int _text_starting_word(char* content, int index) {
  if (content == NULL) return -1;
  
  unsigned char c = (unsigned char)content[index];
  
  // Check if starts with underscore, ASCII letter, or UTF-8 letter start
  if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0)) {
    int byte_count = 0;
    int len = strlen(content);
    int pos = index;
    
    while (pos < len) {
      unsigned char cur = (unsigned char)content[pos];
      
      // Check if character is part of a word
      if (cur == '_' || (cur >= 'a' && cur <= 'z') || (cur >= 'A' && cur <= 'Z') || 
          (cur >= '0' && cur <= '9')) {
        byte_count++;
        pos++;
      }
      // Handle UTF-8 multi-byte characters
      else if (cur >= 0xC0) {
        // Get the full UTF-8 character length
        int char_len = 1;
        if ((cur & 0xE0) == 0xC0) char_len = 2;
        else if ((cur & 0xF0) == 0xE0) char_len = 3;
        else if ((cur & 0xF8) == 0xF0) char_len = 4;
        
        // Add all bytes of this character
        for (int i = 0; i < char_len && pos + i < len; i++) {
          byte_count++;
        }
        pos += char_len;
      }
      else {
        break;
      }
    }
    
    return byte_count;
  }
  return -1;
}

// receives the lang, which is from the Language enum, and returns a pointer for the requested LanguageData.
// I think it's better to make it this way, because it avoids the necessity of importing the
// header containing all the languages definitions into their program (into the textarea). Instead,
// the enum is in the lib header and we just have to translate it here.
const LanguageData* _get_language(Language lang) {
  switch (lang) {
    case LANG_C: return &clang;
    case LANG_C3: return &c3lang;

    default:
      return &clang;
  }
}

// returns true if the given word is a keyword of the given language
bool _text_is_keyword(const char* word, Language lang) {
  const LanguageData* language = _get_language(lang);
  if (word == NULL || language->keywords == NULL || lang == LANG_NONE) {
    return false;
  }
  // look for a keyword
  for (int i = 0; i < language->total; i++) {
    if (strcmp(word, language->keywords[i]) == 0) {
      return true;
    }
  }
  return false;
}

// used to identify only selected special characters
bool _is_special_char(char c) {
  return (c == ':' || c == ';' || c == '[' || c == ']' || 
          c == '{' || c == '}' || c == '(' || c == ')' ||
          c == '+' || c == '-' || c == '*' || c == '/' || 
          c == '=' || c == '<' || c == '>' || c == '!' ||
          c == '&' || c == '|' || c == '^' || c == '%' ||
          c == '?' || c == '.' || c == '#');
}

// Helper function to check if a word is followed by '(' (function call/declaration)
int _is_function_call(TextArea* textarea, char* content, int word_start, int word_len, int content_len) {
  // check if there's a '(' after the word (ignoring whitespace)
  int pos = word_start + word_len;
  
  // skip whitespace
  while (pos < content_len && isspace((unsigned char)content[pos])) {
    pos++;
  }
  
  // if we find '(', it's a function call/declaration
  if (pos < content_len && content[pos] == '(') {
    return 1;
  }
  
  return 0;
}

// Prints a word with the appropriate color (keyword, function, or regular text)
void _print_colored_word(TextArea* textarea, const char* word, int word_len, int is_function) {
  if (is_function) {
    wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.function_id));
    wprintw(textarea->base.dwin, "%.*s", word_len, word);
    wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.function_id));
  } 
  else if (_text_is_keyword(word, textarea->language)) {
    wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.keyword_id));
    wprintw(textarea->base.dwin, "%.*s", word_len, word);
    wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.keyword_id));
  }
  else {
    wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
    wprintw(textarea->base.dwin, "%.*s", word_len, word);
    wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
  }
}

// Print highlighted line content (UTF‑8 safe)
void _textarea_print_with_color(TextArea* textarea, char* content, int row, int col_start) {
    if (textarea == NULL || content == NULL) return;

    wmove(textarea->base.dwin, row, col_start);

    int index = 0;
    int len = strlen(content);
    int max_width = textarea_get_usable_width(textarea);
    int visual_position = 0;

    while (index < len && visual_position < max_width) {
        // ---- comments -------------------------------------------------
        if (content[index] == '/' && index + 1 < len && content[index + 1] == '/') {
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.comment_id));
            // print everything from here to end of line as a single string
            wprintw(textarea->base.dwin, "%s", content + index);
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.comment_id));
            break;  // line ends
        }

        // ---- double‑quoted strings ------------------------------------
        if (content[index] == '"') {
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.string_id));
            int start = index;
            // find closing quote (handling escaped quotes)
            while (index < len) {
                if (content[index] == '"' && (index == start || content[index-1] != '\\')) {
                    break;
                }
                index++;
            }
            if (index < len && content[index] == '"') {
                index++;  // include closing quote
            }
            // print the whole string at once
            wprintw(textarea->base.dwin, "%.*s", index - start, content + start);
            // count visual characters in this segment
            int seg_visual = _textarea_get_visual_distance(content, start, index);
            visual_position += seg_visual;
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.string_id));
            continue;
        }

        // ---- single‑quoted characters --------------------------------
        if (content[index] == '\'') {
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.char_id));
            int start = index;
            while (index < len) {
                if (content[index] == '\'' && (index == start || content[index-1] != '\\')) {
                    break;
                }
                index++;
            }
            if (index < len && content[index] == '\'') {
                index++;
            }
            wprintw(textarea->base.dwin, "%.*s", index - start, content + start);
            int seg_visual = _textarea_get_visual_distance(content, start, index);
            visual_position += seg_visual;
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.char_id));
            continue;
        }

        // ---- numbers (digits) -----------------------------------------
        if (isdigit((unsigned char)content[index])) {
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.number_id));
            int start = index;
            while (index < len && isdigit((unsigned char)content[index])) {
                index++;
            }
            wprintw(textarea->base.dwin, "%.*s", index - start, content + start);
            int seg_visual = _textarea_get_visual_distance(content, start, index);
            visual_position += seg_visual;
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.number_id));
            continue;
        }

        // ---- special characters ---------------------------------------
        if (_is_special_char(content[index])) {
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.symbol_id));
            int start = index;
            // special characters are usually single-byte, but we handle multi-byte anyway
            int char_len = 1;
            unsigned char c = (unsigned char)content[index];
            if (c >= 0xC0) {
                if ((c & 0xE0) == 0xC0) char_len = 2;
                else if ((c & 0xF0) == 0xE0) char_len = 3;
                else if ((c & 0xF8) == 0xF0) char_len = 4;
            }
            index += char_len;
            wprintw(textarea->base.dwin, "%.*s", char_len, content + start);
            visual_position++;  // each special char occupies one column
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.symbol_id));
            continue;
        }

        // ---- words (keywords, function names) -------------------------
        int word_len = _text_starting_word(content, index);
        if (word_len > 0 && index + word_len <= len) {
            int start = index;
            index += word_len;
            // extract the word as a null‑terminated string
            char word[256];
            int wlen = word_len < 255 ? word_len : 255;
            strncpy(word, content + start, wlen);
            word[wlen] = '\0';

            // check if it's a function call
            int is_function = _is_function_call(textarea, content, start, word_len, len);

            if (is_function) {
                wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.function_id));
                wprintw(textarea->base.dwin, "%s", word);
                wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.function_id));
            } else if (_text_is_keyword(word, textarea->language)) {
                wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.keyword_id));
                wprintw(textarea->base.dwin, "%s", word);
                wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.keyword_id));
            } else {
                wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
                wprintw(textarea->base.dwin, "%s", word);
                wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
            }

            // count visual characters in the word
            int seg_visual = _textarea_get_visual_distance(content, start, index);
            visual_position += seg_visual;
            continue;
        }

        // ---- any remaining character (spaces, etc.) -------------------
        // Print a single character (or multi‑byte sequence) as text color
        if (visual_position < max_width) {
            unsigned char c = (unsigned char)content[index];
            int char_len = 1;
            if (c >= 0xC0) {
                if ((c & 0xE0) == 0xC0) char_len = 2;
                else if ((c & 0xF0) == 0xE0) char_len = 3;
                else if ((c & 0xF8) == 0xF0) char_len = 4;
            }
            wattron(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
            wprintw(textarea->base.dwin, "%.*s", char_len, content + index);
            wattroff(textarea->base.dwin, COLOR_PAIR(textarea_active_theme.text_id));
            index += char_len;
            visual_position++;
        } else {
            break;
        }
    }
}

void _textarea_draw_content(TextArea* textarea, int max_visible_lines, int usable_width) {
  if (textarea == NULL) {
    return;
  }

  for (int i = 0; i < max_visible_lines; i++) {
    int file_line_index = textarea->scroll_row + i;
    int screen_row = i + 1;

    // First, clear the line number area for this row
    if (textarea->show_line_numbers) {
      for (int j = 0; j < textarea->line_number_width + 1; j++) {
        mvwprintw(textarea->base.dwin, screen_row, 1 + j, " ");
      }
    }

    // if the line exists, draw the line number and the text
    if (file_line_index < textarea->total_lines) {
      // print with specific content color if it is set
      if (textarea->content_color != NULL) {
        wattron(
          textarea->base.dwin,
          COLOR_PAIR(textarea->content_color_id)
        );
        wbkgd(
          textarea->base.dwin,
          COLOR_PAIR(textarea->base.color_pair_id)
        );
      }
      
      char* content = _textarea_get_printable_content(textarea, file_line_index);

      // calculate the exact limit of bytes to avoid breaking UTF-8 chars visually
      int byte_limit = 0;
      int visual_count = 0; // how many characters to print
      int content_len = strlen(content);
      
      while (byte_limit < content_len && visual_count < usable_width) {
        unsigned char c = (unsigned char)content[byte_limit];
        // if it is not a continuation byte, add 1 to the visual_count
        if ((c & 0xC0) != 0x80) {
          visual_count++;
        }
        byte_limit++;
      }
      
      // Make sure we don't cut in the middle of a UTF-8 character
      if (byte_limit < content_len && (content[byte_limit] & 0xC0) == 0x80) {
        // Go back to the start of the current character
        while (byte_limit > 0 && (content[byte_limit] & 0xC0) == 0x80) {
          byte_limit--;
        }
      }

      // Clear the content area before printing (prevents leftover characters)
      for (int j = 0; j < usable_width; j++) {
        mvwprintw(textarea->base.dwin, screen_row, textarea->line_number_width + 2 + j, " ");
      }

      // ------------------------------------------------------
      // PRINTING CONTENT
      //
      // printing with theme colors
      if (textarea->language != LANG_NONE && textarea->use_theme_colors) {
        _textarea_print_with_color(textarea, content, screen_row, textarea->line_number_width + 2);
      }

      // printing with default textarea foreground color
      else {
        // print normal content
        container_print(
          (Container*)textarea,
          FALSE,
          screen_row,
          textarea->line_number_width + 2,
          "%.*s",
          byte_limit,
          content
        );
      }

      // fix borders
      container_update(textarea);

      // END PRINTING CONTENT
      // -------------------------------------------------------

      // if we have the content specific color active
      if (textarea->content_color != NULL) {
        wattroff(textarea->base.dwin, COLOR_PAIR(textarea->content_color_id));
        wattroff(textarea->base.dwin, COLOR_PAIR(textarea->base.color_pair_id));
      }

      if (textarea->show_line_numbers) {
        // print the line number
        mvwprintw(textarea->base.dwin, screen_row, 1, "%*d", textarea->line_number_width, file_line_index + 1);
      }
    }
    // if the line was removed, overwrite the remaining region with empty chars
    else {
      container_print(
        (Container*)textarea,
        FALSE,
        screen_row,
        textarea->line_number_width + 2,
        "%*s",
        usable_width,
        ""
      );
    }
  }
}

// Default behavior of the TextArea
void _textarea_actions(void* context, unsigned int c) {
  TextArea* textarea = (TextArea*)context;
  if (textarea == NULL) return;

  curs_set(0);

  int max_visible_lines = _textarea_get_max_visible_lines(textarea);
  int usable_width = textarea_get_usable_width(textarea);

  // fix line number width
  if (textarea->line_number_width == 0) {
    _textarea_set_line_width(textarea);
  }

  int update_box = FALSE;
  switch (c) {
    // move cursor DOWN
    case KEY_DOWN:
      update_box = textarea_handle_key_down(textarea, max_visible_lines);
      break;
    case KEY_UP: textarea_handle_key_up(textarea); break; // move cursor UP
    case KEY_RIGHT: textarea_handle_key_right(textarea); break; // move cursor RIGHT
    case KEY_LEFT: textarea_handle_key_left(textarea); break; // move cursor LEFT
    case 10: case KEY_ENTER: textarea_handle_key_enter(textarea); break; // when pressing ENTER
    case '\t': _textarea_handle_tabs(textarea); break;

    case KEY_BACKSPACE:
      if (textarea->disabled == FALSE) {
        _textarea_remove_left_char(textarea);
      }
      break;

    // print the character
    default:
      if ((c >= 32 && c <= 126 || c >= 128 && c <= 254) && textarea->disabled == FALSE) {
        _textarea_add_char(textarea, c);
      }
      break;
  }

  if (update_box) container_update(textarea);

  // Ensure scroll_col is always at a valid character start
  if (textarea->scroll_col > 0 && textarea->cursor_row >= 0 && 
      textarea->cursor_row < textarea->total_lines) {
    char* line = textarea->lines[textarea->cursor_row];
    if (line != NULL && textarea->scroll_col < (int)strlen(line)) {
      // Check if we're at a continuation byte
      if ((line[textarea->scroll_col] & 0xC0) == 0x80) {
        textarea->scroll_col = _textarea_find_char_start(line, textarea->scroll_col);
      }
    }
  }

  // Also ensure cursor_col is valid
  if (textarea->cursor_row >= 0 && textarea->cursor_row < textarea->total_lines) {
    char* line = textarea->lines[textarea->cursor_row];
    if (line != NULL) {
      int line_len = strlen(line);
      if (textarea->cursor_col > line_len) {
        textarea->cursor_col = line_len;
      }
      // Ensure cursor is at a valid character start
      if (textarea->cursor_col > 0 && textarea->cursor_col < line_len &&
          (line[textarea->cursor_col] & 0xC0) == 0x80) {
        textarea->cursor_col = _textarea_find_char_start(line, textarea->cursor_col);
      }
    }
  }

  // render the visible text and line numbers
  _textarea_draw_content(textarea, max_visible_lines, usable_width);

  // update cursor
  if (textarea->cursor_row >= 0 && textarea->cursor_row < textarea->total_lines) {
    int visual_y = textarea->cursor_row - textarea->scroll_row + 1;
    
    if (visual_y >= 1 && visual_y <= textarea->base.height - 2) {
      // count real characters instead of pure bytes
      int visual_distance = _textarea_get_visual_distance(
        textarea->lines[textarea->cursor_row],
        textarea->scroll_col,
        textarea->cursor_col
      );
      int visual_x = textarea->line_number_width + visual_distance + 2;
      
      // make sure cursor stays within bounds
      if (visual_x >= 1 && visual_x < textarea->base.width) {
        wmove(textarea->base.dwin, visual_y, visual_x);
      }
    }
  }

  // after pressing 'i' when the textarea is disabled
  if (c == textarea->enable_key) {
    textarea->disabled = FALSE;
  }

  // update screen buffers
  wnoutrefresh(textarea->base.dwin);
  doupdate();

  curs_set(1);
}

// init the given color pair
// returns the colors pair id
int _theme_start_color(char* color, char* background) {
  if (color == NULL || background == NULL || !has_colors()) return -1;

  static int background_color_id = -1; // track if bg_id was already setted up for the theme
  int fg_id = global_color_id_counter++;
  int bg_id = 0;
  int pair_id = global_color_pair_counter++;

  // init colors
  _register_hex_color(fg_id, color);
  // init the background if it's not started yet
  if (background_color_id <= 0) {
    bg_id = global_color_id_counter++;
    _register_hex_color(bg_id, background);
    background_color_id = bg_id;
  }
  // if the background already exists
  else {
    bg_id = background_color_id;
  }

  // init color pair
  init_pair(pair_id, fg_id, bg_id);
  return pair_id;
}

// init each theme color pair, but not the background variant color
void _textarea_init_theme(TextArea* textarea) {
  if (textarea == NULL || !textarea->use_theme_colors) {
    return;
  }
  // init each color and save into the textarea_active_theme
  // all textareas follow the same theme if active
  textarea_active_theme.keyword_id = _theme_start_color(active_theme->keyword_color, active_theme->background_color);
  textarea_active_theme.function_id = _theme_start_color(active_theme->function_color, active_theme->background_color);
  textarea_active_theme.char_id = _theme_start_color(active_theme->char_color, active_theme->background_color);
  textarea_active_theme.comment_id = _theme_start_color(active_theme->comment_color, active_theme->background_color);
  textarea_active_theme.line_number_id = _theme_start_color(active_theme->line_number_color, active_theme->background_color);
  textarea_active_theme.number_id = _theme_start_color(active_theme->number_color, active_theme->background_color);
  textarea_active_theme.string_id = _theme_start_color(active_theme->string_color, active_theme->background_color);
  textarea_active_theme.text_id = _theme_start_color(active_theme->text_color, active_theme->background_color);
  textarea_active_theme.symbol_id = _theme_start_color(active_theme->symbol_color, active_theme->background_color);
}

// create a new TextArea
TextArea* textarea_create(WINDOW* parent, int height, int width, int start_y, int start_x, const char* label, void (*callback)(int, void*)) {
  if (parent == NULL) return NULL;
  
  TextArea* textarea = (TextArea*)malloc(sizeof(TextArea));
  if (textarea == NULL) exit(1);

  textarea->lines = NULL;
  textarea->total_lines = 0;
  textarea->lines_capacity = 0;
  textarea->cursor_row = 0;
  textarea->cursor_col = 0;
  textarea->scroll_row = 0;
  textarea->scroll_col = 0;
  textarea->disabled = FALSE;
  textarea->enable_key = -1;
  textarea->show_line_numbers = FALSE;
  textarea->line_number_width = 4;
  textarea->tabs_for_spaces = FALSE;
  textarea->use_theme_colors = FALSE;
  textarea->language = LANG_NONE;

  // base container properties
  textarea->base.actions = _textarea_actions;
  textarea->base.has_border = FALSE;
  textarea->base.height = height; // top and bottom borders plus content in one line
  textarea->base.on_focus = callback;
  textarea->base.parent = parent;
  textarea->base.start_x = start_x;
  textarea->base.start_y = start_y;
  textarea->base.width = width;
  textarea->base.user_data = NULL;
  textarea->base.title = label;
  textarea->base.type = TYPE_TEXTAREA;

  // extra style default value
  textarea->content_color = NULL;

  container_init(textarea);
  container_update(textarea);

  int initial_y = 1;
  int initial_x = 1;

  // if line numbers are enabled, account for them too
  if (textarea->show_line_numbers) {
    initial_x += textarea->line_number_width + 1; // we add +1 to account for the space after the numbers
  }

  wmove(textarea->base.dwin, initial_y, initial_x);
  wnoutrefresh(textarea->base.dwin);
  doupdate();

  return textarea;
}

// Init colors specific to the textarea.
// For now, the only extra color is for the content inside the textarea.
void  _textarea_init_content_style(TextArea* textarea) {
  if (textarea == NULL || textarea->content_color == NULL || textarea->base.background == NULL || !has_colors()) return;

  int fg_id = global_color_id_counter++;
  int bg_id = global_color_id_counter++;
  int pair_id = global_color_pair_counter++;

  // init colors
  _register_hex_color(fg_id, textarea->content_color);
  _register_hex_color(bg_id, textarea->base.background);

  // init color pair
  init_pair(pair_id, fg_id, bg_id);
  textarea->content_color_id = pair_id;
}

// Draw the textarea content on screen
void textarea_render(Application* app, TextArea *textarea) {
  if (textarea == NULL) return;

  // add component to the focusable list
  app_add_container(app, textarea); // add to the focusable list

  // add initial content to the textarea if it's empty
  if (textarea->lines == NULL || textarea->total_lines < 1) {
    textarea_add_line(textarea, "");
  }

  // is the theme was set to be used, init its colors
  if (textarea->use_theme_colors) {
    _textarea_init_theme(textarea);
    textarea->base.background = active_theme->background_color;
  }

  // init specific colors
  // set up foreground and background colors - in the case of textarea, the content color is set separetely if we desire to
  if (textarea->base.foreground != NULL) {
    // set default background color to black
    if (textarea->base.background == NULL) {
      textarea->base.background = "#000000";
    }
    container_apply_style(textarea);
  }

  // if content color is set
  if (textarea->content_color != NULL) {
    // set default background color to black
    if (textarea->base.background == NULL) {
      textarea->base.background = "#000000";
    }
    _textarea_init_content_style(textarea);
  }

  // start initial drawing
  _textarea_actions(textarea, 0);
}

// Adds a new line under the cursor position (after the current active line)
// and move the cursor to the new line.
// On the first load, the cursor should appear at the top of the textarea.
void textarea_add_line(TextArea* textarea, const char* line) {
  if (textarea == NULL) exit(1);

  // make sure there is enough space for a new line
  if (textarea->total_lines >= textarea->lines_capacity) {
    // start with capacity for 8 lines
    int new_capacity = (textarea->lines_capacity == 0) ? 8 : textarea->lines_capacity * 2;

    char** temp_lines = (char**)realloc(textarea->lines, new_capacity * sizeof(char*));
    if (temp_lines == NULL) exit(1);

    textarea->lines = temp_lines;
    textarea->lines_capacity = new_capacity;
  }

  // allocate enough space for the new string
  int text_length = line ? strlen(line) : 0;
  textarea->lines[textarea->total_lines] = (char*)malloc((text_length + 1) * sizeof(char));
  if (textarea->lines[textarea->total_lines] == NULL) exit(1);

  // copy line to the textarea
  if (line != NULL) {
    strcpy(textarea->lines[textarea->total_lines], line);
  } else {
    // create a valid empty string on NULL
    textarea->lines[textarea->total_lines][0] = '\0';
  }

  // update lines total
  textarea->total_lines++;
}

// Read content from a file and load it inside the Textarea
void textarea_from_file(TextArea* textarea, const char* filepath, bool create_if_not_exists) {
  FILE* file = fopen(filepath, "r");

  // file not found and you want to create in this case
  if (file == NULL && create_if_not_exists) {
    file = fopen(filepath, "w+");
    if (file == NULL) {
      char err_msg[512];
      sprintf(err_msg, "// Critical Error: without permission to create file '%s'", filepath);
      textarea_add_line(textarea, err_msg);
      return;
    }
  }
  else if (file == NULL) {
    char err_msg[512];
    sprintf(err_msg, "// Critical Error: file not found '%s'", filepath);
    textarea_add_line(textarea, err_msg);
    return;
  }
    
  // add file content to the textarea
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), file)) {
    buffer[strcspn(buffer, "\r\n")] = '\0';
    textarea_add_line(textarea, buffer);
  }

  fclose(file);
}

// Save textarea content to a file
bool textarea_save_to_file(TextArea* textarea, const char* filepath) {
  FILE* file = fopen(filepath, "w");

  // file not found and not possible to create it
  if (file == NULL) {
    return false;
  }

  // add each line from the textarea to the file
  for (int i = 0; i < textarea->total_lines; i++) {
    if (textarea->lines[i] != NULL) {
      fputs(textarea->lines[i], file);
    }
    // add a \n at the end of every line to avoid the file to be a single giant line
    fputs("\n", file);
  }

  fclose(file);

  // file was saved
  return true;
}

// free memory requested by the textarea
void textarea_destroy(TextArea* textarea) {
  if (textarea == NULL) return;

  if (textarea->lines != NULL) {
    // clear each line
    for (int i = 0; i < textarea->total_lines ; i++) {
      if (textarea->lines[i] != NULL) {
        free(textarea->lines[i]);
      }
    }
    // free the lines table
    free(textarea->lines);
  }
  // free the structure
  free(textarea);
}
