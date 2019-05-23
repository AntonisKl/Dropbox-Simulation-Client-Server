#include "utils.h"

void printErrorLn(char* s) {
    printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", s);

    return;
}