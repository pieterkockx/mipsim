/* Written by Pieter Kockx */
#include <stdarg.h>

extern void exit(int);
extern int printf(const char *, ...);
extern int scanf(const char *, ...);

size_t getaline(char *, int);
void boot(union inst *[], int, int, struct data_image *, char);
void trace(void);

