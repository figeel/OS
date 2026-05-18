#include "util.h"

void tokenize(char* input, char* tokens[], int* token_count) {
    *token_count = 0;
    char *token = strtok(input, " \t\n\r");

    while (token != NULL) {
        tokens[(*token_count)++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    tokens[*token_count] = NULL;
}

int compare_filenames(const void* a, const void* b) {
    const char* filename1 = *(const char**)a;
    const char* filename2 = *(const char**)b;

    return strcmp(filename1, filename2);
}
