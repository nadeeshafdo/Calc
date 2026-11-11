#pragma once

#include <stdlib.h>

// all colors should be hexadecimal
typedef struct {
  char* keyword_color;
  char* function_color;
  char* number_color; // for numbers inside the text
  char* line_number_color; // the column of numbers on the textarea
  char* symbol_color;
  char* background_color;
  char* background_variant_color;
  char* comment_color;
  char* string_color; // colorize inside double quotes
  char* char_color; // colorize a single char inside single quotes
  char* text_color; // other words, like variable names
} TextareaTheme;

const TextareaTheme default_theme = { "#009f80", "#5082be", "#cdaa19", "#76797a", "#64748b", "#151517", "#303030", "#514f56", "#ff5822", "#f9a337", "#9ca8ab" };

typedef struct {
  const char** keywords;
  int total;
} LanguageData;

// ----------------------
// C Language
// ----------------------
static const char* c_keywords_list[] = {
  "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
  "enum", "extern", "float", "for", "goto", "if", "int", "long", "register", "return", "short",
  "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
  "volatile", "while", "include", "define", "pragma", "once"
};

const LanguageData clang = {
  c_keywords_list,
  sizeof(c_keywords_list) / sizeof(char*)
};

// ----------------------
// C3 Language
// ----------------------
static const char* c3_keywords_list[] = {
  "alias", "align", "any", "bfloat64", "bool", "break", "case", "catch", "CChar", "char", "CInt",
  "CLong", "CLongDouble", "CLongLong", "continue", "const", "CShort", "CUInt", "CULong", "CULongLong",
  "CUShort", "default", "defer", "double", "embed", "enum", "else", "fault", "faultdef", "float",
  "float128", "float16", "fn", "for", "foreach", "ichar", "if", "import", "inline", "int", "int128",
  "iptr", "long", "macro", "module", "pure", "return", "short", "static", "String", "struct", "sz", "switch",
  "try", "typeid", "uint", "uint128", "ulong", "union", "uptr", "ushort", "usz", "var", "void", "while", "ZString"
};

const LanguageData c3lang = {
  c3_keywords_list,
  sizeof(c3_keywords_list) / sizeof(char*)
};
