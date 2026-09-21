#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

#define BUILTIN_NOT_FOUND 0
#define BUILTIN_HANDLED 1
#define BUILTIN_EXIT 2

int builtin_execute(command_t *command);

#endif
