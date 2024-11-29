#ifndef ERRORS_H
#define ERRORS_H

#define PARSE_SUCCESS 0
#define PARSE_ERR 1
#define SEM_ERR 2
#define MISC_ERR 3

void err_prnt(const int type, const int line, const int charno, const char *__restrict__ msg, ...);

#endif