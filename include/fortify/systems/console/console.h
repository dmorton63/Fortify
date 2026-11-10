#ifndef FORTIFY_CONSOLE_H
#define FORTIFY_CONSOLE_H

#include <stdbool.h>

bool console_init(void);
void console_write(const char *text);

#endif

