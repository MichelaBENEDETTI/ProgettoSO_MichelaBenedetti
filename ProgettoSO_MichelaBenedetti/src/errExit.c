#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "errExit.h"
void errExit(const char *info) {
    perror(info);
    exit(1);
}
