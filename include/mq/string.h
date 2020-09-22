/* string.h: String macros */

#ifndef STRING_H
#define STRING_H

#include <string.h>

#define streq(a, b)	(strcmp((a), (b)) == 0)
#define chomp(s)    if (strlen(s)) { s[strlen(s) - 1] = 0; }

#endif

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
