#include <stdbool.h>

struct extr {
	char mnem[16];
	int reg[3];
	int val;
	int *addr;
};

extern int InstC;

struct extr parse(char *, bool *, bool *, char *);
char is_mnem(const char *);
char encode(union inst *, struct extr);
void pop_labels(bool *);
bool link_labels(void);
int locate_label(const char *);
void free_labels(void);

