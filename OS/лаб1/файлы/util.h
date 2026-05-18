#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <string.h>

// Разбиение строки на токены
void tokenize(char* input, char* tokens[], int* token_count);

// Компаратор для имен файлов и папок
int compare_filenames(const void* a, const void* b);

#endif
