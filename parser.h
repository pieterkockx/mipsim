/* Written by Pieter Kockx */
#include <stdbool.h>

#define INST_ARGS 4
#define INST_REGS 3

struct extr {
	char mnem[16];
	int reg[INST_REGS];
	int val;
	int *addr;
};

extern int InstC;

struct extr parse(/*notnull:*/char *, bool *, bool *, char * /*:notnull*/);
bool is_mnem(const char *);
bool encode(union inst *, struct extr);
void pop_labels(bool *);
bool link_labels(void);
int locate_label(const char *);
void free_labels(void);

