#include "errors.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void err_prnt(const int type, const int line, const int charno, const char *__restrict__ msg, ...) {
    va_list args;
    va_start(args, msg);
    // vwarnx(msg, args);
    switch (type) {
        case PARSE_ERR:
            fprintf(stderr, "Error line %d:%d: ", line, charno);
            verrx(PARSE_ERR, msg, args);
            break;
        case SEM_ERR:
            fprintf(stderr, "Error line %d:%d: ", line, charno);
            verrx(SEM_ERR, msg, args);
            break;
        case MISC_ERR:
            verrx(MISC_ERR, msg, args);
            break;
        default:
            fprintf(stderr, "Warning line %d:%d: ", line, charno);
            vwarnx(msg, args);
            break;
    }
    va_end(args);
}